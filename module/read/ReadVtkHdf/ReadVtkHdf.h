#ifndef VISTLE_READVTKHDF_READVTKHDF_H
#define VISTLE_READVTKHDF_READVTKHDF_H

#include <vistle/module/reader.h>

#include <hdf5.h>

#include <mutex>
#include <string>
#include <vector>

class ReadVtkHdf: public vistle::Reader {
public:
    ReadVtkHdf(const std::string &name, int moduleID, mpi::communicator comm);

    bool examine(const vistle::Parameter *param) override;
    bool read(Token &token, int timestep = -1, int block = -1) override;

private:
    static const int NumFields = 6;

    enum class DataType { None, UnstructuredGrid, ImageData };

    // time step information from the /VTKHDF/Steps group
    struct StepInfo {
        std::vector<double> values;
        std::vector<long long> partOffsets, numParts;
        std::vector<long long> pointOffsets, cellOffsets, connectivityOffsets;
    };

    // location of the data of one block within the point/cell data arrays
    struct FieldRange {
        long long pointRow = 0, pointRows = 0, numPoints = 0;
        long long cellRow = 0, cellRows = 0, numCells = 0;
        long long pointStepOffset = 0, cellStepOffset = 0;
        // for refined cells: index of the original cell for every cell of the grid
        const std::vector<vistle::Index> *cellMap = nullptr;
    };

    // objects created while reading, to be added to their ports after the HDF5 lock has been released
    using Outputs = std::vector<std::pair<vistle::Port *, vistle::Object::ptr>>;

    bool readUnstructured(Token &token, Outputs &outputs, hid_t file, int timestep, int block);
    bool readImage(Token &token, Outputs &outputs, hid_t file, int block, int timestep, double realTime);
    void findSeries(const std::string &filename);
    bool addFields(Token &token, Outputs &outputs, hid_t file, vistle::Object::const_ptr grid, const FieldRange &range,
                   int step, int timestep, int block, double realTime);

    vistle::StringParameter *m_filename = nullptr;
    vistle::IntParameter *m_detectSeries = nullptr;
    vistle::IntParameter *m_refineLagrange = nullptr;
    vistle::StringParameter *m_fieldChoice[NumFields] = {};
    vistle::Port *m_gridOut = nullptr;
    vistle::Port *m_fieldOut[NumFields] = {};

    DataType m_type = DataType::None;
    bool m_temporal = false;
    // one file per time step, e.g. results_0.hdf, results_1.hdf, ...
    bool m_series = false;
    std::vector<std::string> m_files;
    std::vector<double> m_fileTimes;
    std::vector<unsigned char> m_reportedFileSteps;
    StepInfo m_steps;
    std::vector<long long> m_numPoints, m_numCells, m_numConnectivity;
    long long m_extent[6] = {0, 0, 0, 0, 0, 0};
    double m_origin[3] = {0., 0., 0.};
    double m_spacing[3] = {1., 1., 1.};
};

#endif
