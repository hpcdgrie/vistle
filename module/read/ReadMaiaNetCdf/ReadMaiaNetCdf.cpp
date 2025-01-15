#include "ReadMaiaNetcdf.h"

#include <boost/algorithm/string/predicate.hpp>

#include <vistle/core/lines.h>
#include <vistle/core/object.h>
#include <vistle/core/points.h>
#include <vistle/core/polygons.h>
#include <vistle/core/rectilineargrid.h>
#include <vistle/core/structuredgrid.h>
#include <vistle/core/uniformgrid.h>
#include <vistle/core/unstr.h>
#include <vistle/core/vec.h>
#include <vistle/util/filesystem.h>

using vistle::Parameter;

MODULE_MAIN(ReadMaiaNetCdf)
using vistle::Reader;
using vistle::DataBase;
using vistle::Parameter;

const std::string Invalid("(NONE)");

bool isCollectionFile(const std::string &fn)
{
    constexpr const char *collectionEndings[3] = {".pvd", ".vtm", ".pvtu"};
    for (const auto ending: collectionEndings)
        if (boost::algorithm::ends_with(fn, ending))
            return true;

    return false;
}

template<class VO>
std::vector<std::string> getFields(VO *dsa)
{
    std::vector<std::string> fields;
    if (!dsa)
        return fields;
    int na = dsa->GetNumberOfArrays();
    for (int i = 0; i < na; ++i) {
        fields.push_back(dsa->GetArrayName(i));
        //cerr << "field " << i << ": " << fields[i] << endl;
    }
    return fields;
}


ReadMaiaNetCdf::ReadMaiaNetCdf(const std::string &name, int moduleID, mpi::communicator comm)
: Reader(name, moduleID, comm)
{
    createOutputPort("grid_out", "grid or geometry");
    m_filename = addStringParameter("filename", "name of netcdf file", "", Parameter::ExistingFilename);
    setParameterFilters(m_filename, "netcdf files(*.Netcdf)");
    // m_readPieces = addIntParameter("read_pieces", "create block for every piece in an unstructured grid", false,
    //                                Parameter::Boolean);
    // m_ghostCells = addIntParameter("create_ghost_cells", "create ghost cells for multi-piece unstructured grids", true,
    //                                Parameter::Boolean);


    observeParameter(m_filename);
}

bool ReadMaiaNetCdf::examine(const vistle::Parameter *param)
{
    MInt fileId = -1;
    MInt status = ncmpi_open(mpiComm(), fileName().c_str(), NC_NOWRITE, MPI_INFO_NULL, &fileId);
    return true;
}

bool ReadMaiaNetCdf::prepareRead()
{
    const std::string filename = m_filename->getValue();


    return true;
}

bool ReadMaiaNetCdf::finishRead()
{
    return true;
}

bool ReadMaiaNetCdf::read(Token &token, int timestep, int block)
{
    return true;
}

bool ReadMaiaNetCdf::load(Token &token, const std::string &filename, const vistle::Meta &meta, int piece, bool ghost,
                          const std::string &part) const
{
    return true;
}
