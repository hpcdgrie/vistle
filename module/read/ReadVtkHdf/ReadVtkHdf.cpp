/* Reader for the VTK HDF file format (.vtkhdf)
 *
 * supported: UnstructuredGrid (partitions are mapped to blocks, temporal data via the Steps group)
 *            ImageData (axis-aligned, single piece, first time step only)
 */
#include "ReadVtkHdf.h"

#include <vistle/core/unstr.h>
#include <vistle/core/uniformgrid.h>
#include <vistle/core/vec.h>
#include <vistle/util/filesystem.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <mutex>
#include <unordered_map>

using namespace vistle;

namespace {

const char *const Root = "/VTKHDF";
const char *const PointData = "PointData";
const char *const CellData = "CellData";

#ifndef H5_HAVE_THREADSAFE
std::mutex hdf5Mutex;
#endif

// directory of the file being read: external links are resolved relative to it
std::string externalPrefix;

void setExternalPrefix(const std::string &filename)
{
    auto pos = filename.find_last_of("/\\");
    externalPrefix = pos == std::string::npos ? std::string(".") : filename.substr(0, pos);
}

// link/dataset access property list, so that relative external links resolve next to the opened file
hid_t externalAccessPlist(hid_t cls)
{
    hid_t plist = H5Pcreate(cls);
    if (plist >= 0)
        H5Pset_elink_prefix(plist, externalPrefix.c_str());
    return plist;
}

struct Handle {
    hid_t id = -1;
    herr_t (*close)(hid_t) = nullptr;
    Handle(hid_t id, herr_t (*close)(hid_t)): id(id), close(close) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    ~Handle()
    {
        if (id >= 0)
            close(id);
    }
    operator hid_t() const { return id; }
    bool valid() const { return id >= 0; }
};

hid_t scalarMemType()
{
    return sizeof(Scalar) == 8 ? H5T_NATIVE_DOUBLE : H5T_NATIVE_FLOAT;
}

std::string readStringAttr(hid_t obj, const char *name)
{
    if (H5Aexists(obj, name) <= 0)
        return std::string();
    Handle attr(H5Aopen(obj, name, H5P_DEFAULT), H5Aclose);
    if (!attr.valid())
        return std::string();
    Handle type(H5Aget_type(attr), H5Tclose);
    if (!type.valid() || H5Tget_class(type) != H5T_STRING)
        return std::string();

    if (H5Tis_variable_str(type) > 0) {
        char *str = nullptr;
        if (H5Aread(attr, type, &str) < 0 || !str)
            return std::string();
        std::string result(str);
        H5free_memory(str);
        return result;
    }

    std::vector<char> buf(H5Tget_size(type) + 1, '\0');
    if (H5Aread(attr, type, buf.data()) < 0)
        return std::string();
    return std::string(buf.data());
}

bool readNumAttr(hid_t obj, const char *name, hid_t memType, void *buf)
{
    if (H5Aexists(obj, name) <= 0)
        return false;
    Handle attr(H5Aopen(obj, name, H5P_DEFAULT), H5Aclose);
    return attr.valid() && H5Aread(attr, memType, buf) >= 0;
}

bool exists(hid_t loc, const std::string &path)
{
    Handle lapl(externalAccessPlist(H5P_LINK_ACCESS), H5Pclose);
    // for external links, also verify that the target can be resolved
    return H5Lexists(loc, path.c_str(), H5P_DEFAULT) > 0 && H5Oexists_by_name(loc, path.c_str(), lapl) > 0;
}

bool datasetDims(hid_t file, const std::string &path, std::vector<hsize_t> &dims)
{
    dims.clear();
    if (!exists(file, path))
        return false;
    Handle dapl(externalAccessPlist(H5P_DATASET_ACCESS), H5Pclose);
    Handle ds(H5Dopen2(file, path.c_str(), dapl), H5Dclose);
    if (!ds.valid())
        return false;
    Handle space(H5Dget_space(ds), H5Sclose);
    if (!space.valid())
        return false;
    int rank = H5Sget_simple_extent_ndims(space);
    if (rank < 1)
        return false;
    dims.resize(rank);
    H5Sget_simple_extent_dims(space, dims.data(), nullptr);
    return true;
}

// read rows [start, start+count) along the first dimension, all other dimensions in full
// count == -1: read until the end
template<class T>
bool readRows(hid_t file, const std::string &path, hid_t memType, long long start, long long count, std::vector<T> &out)
{
    out.clear();
    if (!exists(file, path))
        return false;
    Handle dapl(externalAccessPlist(H5P_DATASET_ACCESS), H5Pclose);
    Handle ds(H5Dopen2(file, path.c_str(), dapl), H5Dclose);
    if (!ds.valid())
        return false;
    Handle space(H5Dget_space(ds), H5Sclose);
    if (!space.valid())
        return false;
    int rank = H5Sget_simple_extent_ndims(space);
    if (rank < 1)
        return false;
    std::vector<hsize_t> dims(rank);
    H5Sget_simple_extent_dims(space, dims.data(), nullptr);

    if (start < 0 || (hsize_t)start > dims[0])
        return false;
    if (count < 0)
        count = dims[0] - start;
    if ((hsize_t)(start + count) > dims[0])
        return false;

    hsize_t rowSize = 1;
    for (int i = 1; i < rank; ++i)
        rowSize *= dims[i];
    if (count == 0)
        return true;

    std::vector<hsize_t> offset(rank, 0), size(dims);
    offset[0] = start;
    size[0] = count;
    if (H5Sselect_hyperslab(space, H5S_SELECT_SET, offset.data(), nullptr, size.data(), nullptr) < 0)
        return false;
    Handle mem(H5Screate_simple(rank, size.data(), nullptr), H5Sclose);
    if (!mem.valid())
        return false;

    out.resize(count * rowSize);
    if (H5Dread(ds, memType, mem, space, H5P_DEFAULT, out.data()) < 0) {
        out.clear();
        return false;
    }
    return true;
}

template<class T>
bool readAll(hid_t file, const std::string &path, hid_t memType, std::vector<T> &out)
{
    return readRows(file, path, memType, 0, -1, out);
}

std::vector<std::string> listGroup(hid_t file, const std::string &path)
{
    std::vector<std::string> names;
    if (!exists(file, path))
        return names;
    Handle group(H5Gopen2(file, path.c_str(), H5P_DEFAULT), H5Gclose);
    if (!group.valid())
        return names;
    H5G_info_t info;
    if (H5Gget_info(group, &info) < 0)
        return names;
    for (hsize_t i = 0; i < info.nlinks; ++i) {
        ssize_t len = H5Lget_name_by_idx(group, ".", H5_INDEX_NAME, H5_ITER_INC, i, nullptr, 0, H5P_DEFAULT);
        if (len < 0)
            continue;
        std::vector<char> buf(len + 1, '\0');
        H5Lget_name_by_idx(group, ".", H5_INDEX_NAME, H5_ITER_INC, i, buf.data(), buf.size(), H5P_DEFAULT);
        names.emplace_back(buf.data());
    }
    return names;
}

// number of components of a point or cell data array
long long numComponents(const std::vector<hsize_t> &dims, bool image, long long tuples)
{
    if (tuples <= 0)
        return 0;
    hsize_t total = 1;
    for (auto d: dims)
        total *= d;
    if (image) {
        hsize_t t = tuples;
        return total % t == 0 ? total / t : 0;
    }
    hsize_t rows = dims[0];
    return rows > 0 ? total / rows : 0;
}

// order p of a VTK Lagrange quadrilateral with n nodes ((p+1)^2), 0 if n does not match
int lagrangeQuadOrder(long long n)
{
    int p = static_cast<int>(std::lround(std::sqrt(static_cast<double>(n)))) - 1;
    return p >= 1 && (long long)(p + 1) * (p + 1) == n ? p : 0;
}

// position within the node list of a Lagrange quadrilateral for lattice point (i, j), 0 <= i, j <= p
// ordering: 4 corners, then the edge nodes (bottom, right, top, left), then the interior nodes
Index lagrangeQuadIndex(int i, int j, int p)
{
    const bool ib = i == 0 || i == p;
    const bool jb = j == 0 || j == p;
    if (ib && jb)
        return i ? (j ? 2 : 1) : (j ? 3 : 0);
    Index offset = 4;
    if (ib || jb) {
        if (!ib)
            return offset + (i - 1) + (j ? (p - 1) + (p - 1) : 0);
        return offset + (j - 1) + (i ? (p - 1) : 2 * (p - 1) + (p - 1));
    }
    offset += 4 * (p - 1);
    return offset + (i - 1) + (p - 1) * (j - 1);
}

// flip triangles/quadrilaterals so that neighbouring cells traverse shared edges in opposite directions
size_t orientSurface(const std::vector<Index> &el, std::vector<Index> &cl, const std::vector<Byte> &tl, Index numPoints)
{
    const size_t nc = tl.size();
    auto isSurface = [&](size_t c) {
        return tl[c] == UnstructuredGrid::TRIANGLE || tl[c] == UnstructuredGrid::QUAD;
    };
    auto key = [numPoints](Index a, Index b) {
        return (uint64_t)std::min(a, b) * numPoints + std::max(a, b);
    };

    // undirected edge -> (cell, edge runs from lower to higher vertex index)
    std::unordered_map<uint64_t, std::vector<std::pair<size_t, bool>>> edges;
    for (size_t c = 0; c < nc; ++c) {
        if (!isSurface(c))
            continue;
        const Index n = el[c + 1] - el[c];
        for (Index k = 0; k < n; ++k) {
            const Index a = cl[el[c] + k], b = cl[el[c] + (k + 1) % n];
            edges[key(a, b)].emplace_back(c, a < b);
        }
    }

    std::vector<char> visited(nc, 0), flip(nc, 0);
    std::vector<size_t> queue;
    for (size_t s = 0; s < nc; ++s) {
        if (visited[s] || !isSurface(s))
            continue;
        visited[s] = 1;
        queue.assign(1, s);
        while (!queue.empty()) {
            const size_t c = queue.back();
            queue.pop_back();
            const Index n = el[c + 1] - el[c];
            for (Index k = 0; k < n; ++k) {
                const Index a = cl[el[c] + k], b = cl[el[c] + (k + 1) % n];
                const bool dirC = (a < b) != (flip[c] != 0);
                for (const auto &nb: edges[key(a, b)]) {
                    if (visited[nb.first])
                        continue;
                    visited[nb.first] = 1;
                    flip[nb.first] = nb.second == dirC;
                    queue.push_back(nb.first);
                }
            }
        }
    }

    size_t flipped = 0;
    for (size_t c = 0; c < nc; ++c) {
        if (flip[c]) {
            std::reverse(cl.begin() + el[c], cl.begin() + el[c + 1]);
            ++flipped;
        }
    }
    return flipped;
}

long long sumBefore(const std::vector<long long> &v, long long first, long long n)
{
    long long sum = 0;
    for (long long i = 0; i < n; ++i)
        sum += v[first + i];
    return sum;
}

} // namespace

ReadVtkHdf::ReadVtkHdf(const std::string &name, int moduleID, mpi::communicator comm): Reader(name, moduleID, comm)
{
    H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);

    m_filename = addStringParameter("filename", "name of VTK HDF file", "", Parameter::ExistingFilename);
    setParameterFilters(m_filename, "VTK HDF Files (*.vtkhdf *.hdf *.h5)/All Files (*)");

    m_gridOut = createOutputPort("grid_out", "grid");
    for (int i = 0; i < NumFields; ++i) {
        std::string suffix = std::to_string(i);
        m_fieldChoice[i] =
            addStringParameter("field_" + suffix, "data field " + suffix, Reader::InvalidChoice, Parameter::Choice);
        setParameterChoices(m_fieldChoice[i], std::vector<std::string>{Reader::InvalidChoice});
        m_fieldOut[i] = createOutputPort("data_out_" + suffix, "data field " + suffix);
        linkPortAndParameter(m_fieldOut[i], m_fieldChoice[i]);
    }

    m_detectSeries =
        addIntParameter("file_series", "read files with consecutive numbers at the end of their name as time steps",
                        true, Parameter::Boolean);

    m_refineLagrange = addIntParameter("refine_lagrange_cells",
                                       "split high-order Lagrange quadrilaterals into linear cells (otherwise replace "
                                       "them by one linear cell using only the corner vertices)",
                                       false, Parameter::Boolean);

    m_fixOrientation = addIntParameter("fix_orientation",
                                       "flip triangles and quadrilaterals whose orientation disagrees with a neighbour",
                                       false, Parameter::Boolean);

    setParallelizationMode(ParallelizeTimeAndBlocks);
    observeParameter(m_filename);
    observeParameter(m_detectSeries);

#if defined(MODULE_THREAD) && !defined(H5_HAVE_THREADSAFE)
    sendWarning("HDF5 library is not thread-safe, multiple modules must not use hdf5 at the same time!");
#endif
}

// collect files that only differ in the trailing number of their name
void ReadVtkHdf::findSeries(const std::string &filename)
{
    namespace fs = vistle::filesystem;
    m_series = false;
    m_files.clear();
    m_fileTimes.clear();
    m_reportedFileSteps.clear();
    if (!m_detectSeries->getValue())
        return;

    try {
        const fs::path path(filename);
        const std::string stem = path.stem().string();
        const std::string ext = path.extension().string();
        size_t b = stem.size();
        while (b > 0 && std::isdigit(static_cast<unsigned char>(stem[b - 1])))
            --b;
        if (b == stem.size())
            return;
        const std::string prefix = stem.substr(0, b);

        fs::path dir = path.parent_path();
        if (dir.empty())
            dir = fs::path(".");

        std::vector<std::pair<long long, std::string>> found;
        for (fs::directory_iterator it(dir), end; it != end; ++it) {
            const fs::path p = it->path();
            if (p.extension().string() != ext)
                continue;
            const std::string s = p.stem().string();
            if (s.size() <= prefix.size() || s.compare(0, prefix.size(), prefix) != 0)
                continue;
            const std::string num = s.substr(prefix.size());
            if (num.size() > 18 || !std::all_of(num.begin(), num.end(),
                                                [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
                continue;
            found.emplace_back(std::stoll(num), p.string());
        }
        if (found.size() < 2)
            return;
        std::sort(found.begin(), found.end());
        for (const auto &f: found)
            m_files.push_back(f.second);
        m_series = true;
    } catch (std::exception &ex) {
        sendWarning("failed to scan for file series: %s", ex.what());
        m_series = false;
        m_files.clear();
    }
}

bool ReadVtkHdf::examine(const Parameter *param)
{
    if (param && param != m_filename && param != m_detectSeries)
        return true;

    m_type = DataType::None;
    m_temporal = false;
    m_series = false;
    m_files.clear();
    m_fileTimes.clear();
    m_steps = StepInfo();
    m_numPoints.clear();
    m_numCells.clear();
    m_numConnectivity.clear();
    setTimesteps(0);
    setPartitions(0);

    std::vector<std::string> choices{Reader::InvalidChoice};
    auto updateChoices = [this, &choices]() {
        for (int i = 0; i < NumFields; ++i)
            setParameterChoices(m_fieldChoice[i], choices);
    };

    const std::string filename = m_filename->getValue();
    if (filename.empty()) {
        updateChoices();
        return false;
    }
#ifndef H5_HAVE_THREADSAFE
    std::lock_guard<std::mutex> guard(hdf5Mutex);
#endif
    setExternalPrefix(filename);
    Handle file(H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
    if (!file.valid()) {
        sendError("could not open %s", filename.c_str());
        updateChoices();
        return false;
    }
    Handle root(H5Gopen2(file, Root, H5P_DEFAULT), H5Gclose);
    if (!root.valid()) {
        sendError("%s does not contain a /VTKHDF group", filename.c_str());
        updateChoices();
        return false;
    }

    const std::string type = readStringAttr(root, "Type");
    long long version[2] = {0, 0};
    if (readNumAttr(root, "Version", H5T_NATIVE_LLONG, version) && version[0] > 2)
        sendWarning("VTK HDF version %lld.%lld is newer than the supported version 2", version[0], version[1]);

    int numParts = 0;
    if (type == "UnstructuredGrid") {
        m_type = DataType::UnstructuredGrid;
        if (!readAll(file, std::string(Root) + "/NumberOfPoints", H5T_NATIVE_LLONG, m_numPoints) ||
            !readAll(file, std::string(Root) + "/NumberOfCells", H5T_NATIVE_LLONG, m_numCells) ||
            !readAll(file, std::string(Root) + "/NumberOfConnectivityIds", H5T_NATIVE_LLONG, m_numConnectivity) ||
            m_numPoints.size() != m_numCells.size() || m_numPoints.size() != m_numConnectivity.size()) {
            sendError("inconsistent part information in UnstructuredGrid");
            m_type = DataType::None;
            updateChoices();
            return false;
        }
        numParts = m_numPoints.size();

        const std::string steps = std::string(Root) + "/Steps";
        int nsteps = 0;
        if (exists(file, steps)) {
            Handle sg(H5Gopen2(file, steps.c_str(), H5P_DEFAULT), H5Gclose);
            long long n = 0;
            if (sg.valid() && readNumAttr(sg, "NSteps", H5T_NATIVE_LLONG, &n))
                nsteps = n;
        }
        if (nsteps > 0) {
            m_temporal = true;
            auto &s = m_steps;
            readAll(file, steps + "/Values", H5T_NATIVE_DOUBLE, s.values);
            bool ok = readAll(file, steps + "/PartOffsets", H5T_NATIVE_LLONG, s.partOffsets) &&
                      readAll(file, steps + "/NumberOfParts", H5T_NATIVE_LLONG, s.numParts);
            if (!readAll(file, steps + "/PointOffsets", H5T_NATIVE_LLONG, s.pointOffsets))
                s.pointOffsets.assign(nsteps, 0);
            if (!readAll(file, steps + "/CellOffsets", H5T_NATIVE_LLONG, s.cellOffsets))
                s.cellOffsets.assign(nsteps, 0);
            if (!readAll(file, steps + "/ConnectivityIdOffsets", H5T_NATIVE_LLONG, s.connectivityOffsets))
                s.connectivityOffsets.assign(nsteps, 0);
            if (s.values.size() < (size_t)nsteps)
                s.values.resize(nsteps, 0.);
            auto nrows = [](const std::vector<long long> &v) {
                return (long long)v.size();
            };
            ok = ok && nrows(s.partOffsets) >= nsteps && nrows(s.numParts) >= nsteps &&
                 nrows(s.pointOffsets) >= nsteps && nrows(s.cellOffsets) >= nsteps &&
                 nrows(s.connectivityOffsets) >= nsteps;
            numParts = 0;
            for (int i = 0; ok && i < nsteps; ++i) {
                if (s.partOffsets[i] < 0 || s.numParts[i] < 0 ||
                    s.partOffsets[i] + s.numParts[i] > (long long)m_numPoints.size()) {
                    ok = false;
                    break;
                }
                numParts = std::max<long long>(numParts, s.numParts[i]);
            }
            if (!ok) {
                sendError("inconsistent time step information in Steps group");
                m_type = DataType::None;
                m_temporal = false;
                updateChoices();
                return false;
            }
            setTimesteps(nsteps);
        }
    } else if (type == "ImageData") {
        m_type = DataType::ImageData;
        numParts = 1;
        if (!readNumAttr(root, "WholeExtent", H5T_NATIVE_LLONG, m_extent)) {
            sendError("ImageData without WholeExtent");
            m_type = DataType::None;
            updateChoices();
            return false;
        }
        readNumAttr(root, "Origin", H5T_NATIVE_DOUBLE, m_origin);
        readNumAttr(root, "Spacing", H5T_NATIVE_DOUBLE, m_spacing);
        double dir[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        if (readNumAttr(root, "Direction", H5T_NATIVE_DOUBLE, dir)) {
            const double id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            for (int i = 0; i < 9; ++i) {
                if (std::abs(dir[i] - id[i]) > 1e-9) {
                    sendWarning("ImageData with rotated direction matrix: orientation is ignored");
                    break;
                }
            }
        }
        if (exists(file, std::string(Root) + "/Steps"))
            sendWarning("time steps of ImageData are not supported: reading first step only");
    } else {
        sendError("unsupported VTK HDF type '%s' (supported: UnstructuredGrid, ImageData)", type.c_str());
        updateChoices();
        return false;
    }

    // collect data arrays
    long long tuples[2] = {0, 0};
    const bool image = m_type == DataType::ImageData;
    if (image) {
        long long n = 1;
        for (int c = 0; c < 3; ++c)
            n *= std::max<long long>(1, m_extent[2 * c + 1] - m_extent[2 * c] + 1);
        tuples[0] = n;
        n = 1;
        for (int c = 0; c < 3; ++c)
            n *= std::max<long long>(1, m_extent[2 * c + 1] - m_extent[2 * c]);
        tuples[1] = n;
    }
    const char *groups[2] = {PointData, CellData};
    for (int g = 0; g < 2; ++g) {
        const std::string path = std::string(Root) + "/" + groups[g];
        for (const auto &name: listGroup(file, path)) {
            std::vector<hsize_t> dims;
            if (!datasetDims(file, path + "/" + name, dims))
                continue;
            if (!image) {
                long long nc = numComponents(dims, false, 1);
                if (nc != 1 && nc != 3)
                    continue;
            } else {
                long long nc = numComponents(dims, true, tuples[g]);
                if (nc != 1 && nc != 3)
                    continue;
            }
            choices.push_back(std::string(groups[g]) + "/" + name);
        }
    }
    updateChoices();

    findSeries(filename);
    if (m_series) {
        if (m_temporal)
            sendWarning("file series: ignoring time steps within the individual files");
        m_temporal = false;
        // the file with the lowest number is the reference for time 0, if no explicit time is stored
        for (size_t i = 0; i < m_files.size(); ++i) {
            double t = static_cast<double>(i);
            Handle f(H5Fopen(m_files[i].c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
            if (f.valid()) {
                std::vector<double> tv;
                if (readAll(f, "/time", H5T_NATIVE_DOUBLE, tv) && tv.size() == 1)
                    t = tv[0];
            }
            m_fileTimes.push_back(t);
        }
        m_reportedFileSteps.resize(m_files.size(), 0);
        setTimesteps(m_files.size());
    }

    setPartitions(numParts);
    return true;
}

bool ReadVtkHdf::read(Token &token, int timestep, int block)
{
    if (m_type == DataType::None)
        return false;
    const bool timeVarying = m_temporal || m_series;
    if (timeVarying && timestep < 0)
        return true;
    if (!timeVarying && timestep >= 0)
        return true;

    std::string filename = m_filename->getValue();
    double realTime = 0.;
    if (m_series) {
        if (timestep >= (int)m_files.size())
            return true;
        filename = m_files[timestep];
        realTime = m_fileTimes[timestep];
        {
            if (!m_reportedFileSteps[timestep]) {
                sendInfo("timestep %d uses file %s", timestep, filename.c_str());
                m_reportedFileSteps[timestep] = 1;
            }
        }
    }
    std::stringstream output;
    output << "ReadVtkHdf: reading " << filename << " timestep " << timestep << " block " << block << std::endl;
    std::cerr << output.str();
    Outputs outputs;
    {
#ifndef H5_HAVE_THREADSAFE
        std::lock_guard<std::mutex> guard(hdf5Mutex);
#endif
        setExternalPrefix(filename);
        Handle file(H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT), H5Fclose);
        if (!file.valid()) {
            sendError("could not open %s", filename.c_str());
            return false;
        }

        bool ok = m_type == DataType::ImageData
                      ? readImage(token, outputs, file, block, m_series ? timestep : -1, realTime)
                      : readUnstructured(token, outputs, file, timestep, block);
        if (!ok)
            return false;
    }

    // adding objects may block on the previous token, so do it without holding the HDF5 lock
    for (auto &out: outputs)
        token.addObject(out.first, out.second);
    return true;
}

bool ReadVtkHdf::readUnstructured(Token &token, Outputs &outputs, hid_t file, int timestep, int block)
{
    const int step = m_temporal ? timestep : 0;
    const long long firstPart = m_temporal ? m_steps.partOffsets[step] : 0;
    const long long numParts = m_temporal ? m_steps.numParts[step] : m_numPoints.size();
    if (block < 0 || block >= numParts)
        return true;

    const long long g = firstPart + block;
    const long long np = m_numPoints[g], nc = m_numCells[g], nconn = m_numConnectivity[g];
    const long long pointStepOffset = m_temporal ? m_steps.pointOffsets[step] : 0;
    const long long cellStepOffset = m_temporal ? m_steps.cellOffsets[step] : 0;
    const long long connStepOffset = m_temporal ? m_steps.connectivityOffsets[step] : 0;
    const long long pointStart = pointStepOffset + sumBefore(m_numPoints, firstPart, block);
    const long long cellStart = cellStepOffset + sumBefore(m_numCells, firstPart, block);
    const long long connStart = connStepOffset + sumBefore(m_numConnectivity, firstPart, block);
    // every part contributes numCells+1 entries to Offsets
    const long long offsetsStart = cellStart + g;

    const std::string root = Root;
    std::vector<Scalar> points;
    std::vector<long long> offsets, connectivity;
    std::vector<unsigned char> types;
    if (!readRows(file, root + "/Points", scalarMemType(), pointStart, np, points) || points.size() != (size_t)np * 3 ||
        !readRows(file, root + "/Offsets", H5T_NATIVE_LLONG, offsetsStart, nc + 1, offsets) ||
        !readRows(file, root + "/Connectivity", H5T_NATIVE_LLONG, connStart, nconn, connectivity) ||
        !readRows(file, root + "/Types", H5T_NATIVE_UCHAR, cellStart, nc, types)) {
        sendError("failed to read geometry of block %d", block);
        return false;
    }

    std::vector<Index> el(nc + 1), cl(nconn);
    std::vector<Byte> tl(nc);
    // cells with more than the corner nodes that are split into several linear cells
    std::vector<long long> refined;
    for (long long i = 0; i < nconn; ++i) {
        if (connectivity[i] < 0 || connectivity[i] >= np) {
            sendError("invalid connectivity in block %d", block);
            return false;
        }
        cl[i] = connectivity[i];
    }
    long long unhandled = 0;
    std::set<int> unhandledTypes;
    for (long long i = 0; i <= nc; ++i) {
        if (offsets[i] < 0 || offsets[i] > nconn || (i > 0 && offsets[i] < offsets[i - 1])) {
            sendError("invalid cell offsets in block %d", block);
            return false;
        }
        el[i] = offsets[i];
    }
    for (long long i = 0; i < nc; ++i) {
        switch (types[i]) {
        case 1: // vertex
            tl[i] = UnstructuredGrid::POINT;
            break;
        case 3:
            tl[i] = UnstructuredGrid::BAR;
            break;
        case 4:
            tl[i] = UnstructuredGrid::POLYLINE;
            break;
        case 5:
            tl[i] = UnstructuredGrid::TRIANGLE;
            break;
        case 7:
            tl[i] = UnstructuredGrid::POLYGON;
            break;
        case 9:
            tl[i] = UnstructuredGrid::QUAD;
            break;
        case 10:
            tl[i] = UnstructuredGrid::TETRAHEDRON;
            break;
        case 12:
            tl[i] = UnstructuredGrid::HEXAHEDRON;
            break;
        case 13:
            tl[i] = UnstructuredGrid::PRISM;
            break;
        case 14:
            tl[i] = UnstructuredGrid::PYRAMID;
            break;
        // first-order Lagrange cells are identical to their linear counterparts
        case 68:
        case 69:
        case 70:
        case 71:
        case 72:
        case 73: {
            static const UnstructuredGrid::Type lagrange[] = {
                UnstructuredGrid::BAR,         UnstructuredGrid::TRIANGLE,   UnstructuredGrid::QUAD,
                UnstructuredGrid::TETRAHEDRON, UnstructuredGrid::HEXAHEDRON, UnstructuredGrid::PRISM};
            const auto t = lagrange[types[i] - 68];
            const Index n = el[i + 1] - el[i];
            if (n == UnstructuredGrid::NumVertices[t]) {
                tl[i] = t;
            } else if (types[i] == 70 && lagrangeQuadOrder(n) > 1) {
                tl[i] = UnstructuredGrid::QUAD;
                refined.push_back(i);
            } else {
                tl[i] = UnstructuredGrid::NONE;
                ++unhandled;
                unhandledTypes.insert(types[i]);
            }
            break;
        }
        case 8: // pixel
        case 11: // voxel
        {
            const bool voxel = types[i] == 11;
            tl[i] = voxel ? UnstructuredGrid::HEXAHEDRON : UnstructuredGrid::QUAD;
            const Index n = voxel ? 8 : 4;
            if (el[i + 1] - el[i] == n) {
                std::swap(cl[el[i] + 2], cl[el[i] + 3]);
                if (voxel)
                    std::swap(cl[el[i] + 6], cl[el[i] + 7]);
            } else {
                tl[i] = UnstructuredGrid::NONE;
                ++unhandled;
            }
            break;
        }
        default:
            tl[i] = UnstructuredGrid::NONE;
            ++unhandled;
            unhandledTypes.insert(types[i]);
            break;
        }
    }
    if (unhandled > 0) {
        std::string list;
        for (int t: unhandledTypes)
            list += (list.empty() ? "" : ", ") + std::to_string(t);
        sendWarning("block %d: %lld cells of unsupported VTK type (%s) skipped", block, unhandled, list.c_str());
    }

    // split high-order Lagrange quadrilaterals into p*p linear quadrilaterals
    std::vector<Index> cellMap;
    if (!refined.empty()) {
        std::vector<Index> nel, ncl;
        std::vector<Byte> ntl;
        nel.reserve(el.size());
        ncl.reserve(cl.size());
        ntl.reserve(tl.size());
        nel.push_back(0);
        size_t r = 0;
        for (long long i = 0; i < nc; ++i) {
            const Index *nodes = cl.data() + el[i];
            if (r < refined.size() && refined[r] == i) {
                ++r;
                const int p = lagrangeQuadOrder(el[i + 1] - el[i]);
                if (!m_refineLagrange->getValue()) {
                    // a single linear cell from the corner vertices (the first four nodes)
                    ncl.insert(ncl.end(), nodes, nodes + 4);
                    ntl.push_back(UnstructuredGrid::QUAD);
                    nel.push_back(ncl.size());
                    cellMap.push_back(i);
                    continue;
                }
                for (int j = 0; j < p; ++j) {
                    for (int k = 0; k < p; ++k) {
                        ncl.push_back(nodes[lagrangeQuadIndex(k, j, p)]);
                        ncl.push_back(nodes[lagrangeQuadIndex(k + 1, j, p)]);
                        ncl.push_back(nodes[lagrangeQuadIndex(k + 1, j + 1, p)]);
                        ncl.push_back(nodes[lagrangeQuadIndex(k, j + 1, p)]);
                        ntl.push_back(UnstructuredGrid::QUAD);
                        nel.push_back(ncl.size());
                        cellMap.push_back(i);
                    }
                }
            } else {
                ncl.insert(ncl.end(), nodes, nodes + (el[i + 1] - el[i]));
                ntl.push_back(tl[i]);
                nel.push_back(ncl.size());
                cellMap.push_back(i);
            }
        }
        el = std::move(nel);
        cl = std::move(ncl);
        tl = std::move(ntl);
    }

    if (m_fixOrientation->getValue())
        orientSurface(el, cl, tl, np);

    const size_t numOutCells = tl.size();
    auto grid = std::make_shared<UnstructuredGrid>(numOutCells, cl.size(), np);
    Scalar *x = grid->x().data(), *y = grid->y().data(), *z = grid->z().data();
    for (long long i = 0; i < np; ++i) {
        x[i] = points[3 * i];
        y[i] = points[3 * i + 1];
        z[i] = points[3 * i + 2];
    }
    std::copy(el.begin(), el.end(), grid->el().data());
    std::copy(cl.begin(), cl.end(), grid->cl().data());
    std::copy(tl.begin(), tl.end(), grid->tl().data());

    const bool timeVarying = m_temporal || m_series;
    const int outStep = timeVarying ? timestep : -1;
    const double realTime = m_temporal ? m_steps.values[step] : m_series ? m_fileTimes[timestep] : 0.;
    grid->setBlock(block);
    if (timeVarying) {
        grid->setTimestep(timestep);
        grid->setRealTime(realTime);
    }
    token.applyMeta(grid);
    std::stringstream output;
    output << "ReadVtkHdf: addObject grid timestep " << timestep << " block " << block << std::endl;
    std::cerr << output.str();
    outputs.emplace_back(m_gridOut, grid);

    FieldRange range;
    range.pointRow = sumBefore(m_numPoints, firstPart, block);
    range.pointRows = range.numPoints = np;
    range.cellRow = sumBefore(m_numCells, firstPart, block);
    range.cellRows = range.numCells = nc;
    range.pointStepOffset = pointStepOffset;
    range.cellStepOffset = cellStepOffset;
    range.cellMap = cellMap.empty() ? nullptr : &cellMap;
    return addFields(token, outputs, file, grid, range, step, outStep, block, realTime);
}

bool ReadVtkHdf::readImage(Token &token, Outputs &outputs, hid_t file, int block, int timestep, double realTime)
{
    if (block > 0)
        return true;

    long long n[3];
    for (int c = 0; c < 3; ++c)
        n[c] = std::max<long long>(1, m_extent[2 * c + 1] - m_extent[2 * c] + 1);

    auto grid = std::make_shared<UniformGrid>((Index)n[0], (Index)n[1], (Index)n[2]);
    for (int c = 0; c < 3; ++c) {
        grid->min()[c] = m_origin[c] + m_extent[2 * c] * m_spacing[c];
        grid->max()[c] = m_origin[c] + m_extent[2 * c + 1] * m_spacing[c];
    }
    grid->setBlock(0);
    if (timestep >= 0) {
        grid->setTimestep(timestep);
        grid->setRealTime(realTime);
    }
    token.applyMeta(grid);
    std::stringstream output;
    output << "ReadVtkHdf: addObject field timestep " << timestep << " block " << block << std::endl;
    std::cerr << output.str();
    outputs.emplace_back(m_gridOut, grid);

    FieldRange range;
    range.numPoints = n[0] * n[1] * n[2];
    range.pointRows = n[2];
    long long c[3];
    for (int i = 0; i < 3; ++i)
        c[i] = std::max<long long>(1, m_extent[2 * i + 1] - m_extent[2 * i]);
    range.numCells = c[0] * c[1] * c[2];
    range.cellRows = c[2];
    return addFields(token, outputs, file, grid, range, 0, timestep, 0, realTime);
}

bool ReadVtkHdf::addFields(Token &token, Outputs &outputs, hid_t file, Object::const_ptr grid, const FieldRange &range,
                           int step, int timestep, int block, double realTime)
{
    const bool image = m_type == DataType::ImageData;
    for (int i = 0; i < NumFields; ++i) {
        if (!m_fieldOut[i]->isConnected())
            continue;
        const std::string choice = m_fieldChoice[i]->getValue();
        if (choice == Reader::InvalidChoice || choice.empty())
            continue;

        const bool point = choice.compare(0, strlen(PointData), PointData) == 0;
        const std::string field = choice.substr(choice.find('/') + 1);
        const std::string path = std::string(Root) + "/" + choice;

        long long row = point ? range.pointRow : range.cellRow;
        long long rows = point ? range.pointRows : range.cellRows;
        const long long tuples = point ? range.numPoints : range.numCells;
        if (m_temporal && !image) {
            const std::string offsetPath =
                std::string(Root) + "/Steps/" + (point ? "PointDataOffsets/" : "CellDataOffsets/") + field;
            std::vector<long long> off;
            if (readRows(file, offsetPath, H5T_NATIVE_LLONG, step, 1, off) && off.size() == 1)
                row += off[0];
            else
                row += point ? range.pointStepOffset : range.cellStepOffset;
        }

        std::vector<hsize_t> dims;
        if (!datasetDims(file, path, dims))
            continue;
        const long long ncomp = numComponents(dims, image, tuples);
        std::vector<Scalar> data;
        if (!readRows(file, path, scalarMemType(), row, rows, data) || ncomp * tuples != (long long)data.size() ||
            (ncomp != 1 && ncomp != 3)) {
            sendWarning("could not read data field %s for block %d", choice.c_str(), block);
            continue;
        }

        // cells that have been split: replicate the value of the original cell
        const bool mapped = !point && range.cellMap;
        const long long outTuples = mapped ? (long long)range.cellMap->size() : tuples;
        auto src = [&](long long j) {
            return mapped ? (long long)(*range.cellMap)[j] : j;
        };

        DataBase::ptr obj;
        if (ncomp == 1) {
            auto v = std::make_shared<Vec<Scalar, 1>>(outTuples);
            Scalar *x = v->x().data();
            for (long long j = 0; j < outTuples; ++j)
                x[j] = data[src(j)];
            obj = v;
        } else {
            auto v = std::make_shared<Vec<Scalar, 3>>(outTuples);
            Scalar *x = v->x().data(), *y = v->y().data(), *z = v->z().data();
            for (long long j = 0; j < outTuples; ++j) {
                const long long s = src(j);
                x[j] = data[3 * s];
                y[j] = data[3 * s + 1];
                z[j] = data[3 * s + 2];
            }
            obj = v;
        }
        obj->setMapping(point ? DataBase::Vertex : DataBase::Element);
        obj->setGrid(grid);
        obj->setBlock(block);
        if (timestep >= 0) {
            obj->setTimestep(timestep);
            obj->setRealTime(realTime);
        }
        obj->describe(field, id());
        token.applyMeta(obj);
        std::cerr << "adding field " << field << ", timestep " << timestep << ", block " << block << std::endl;

        outputs.emplace_back(m_fieldOut[i], obj);
    }
    return true;
}

MODULE_MAIN(ReadVtkHdf)
