#ifndef VISTLE_READVTKHDF_READVTKHDF_H
#define VISTLE_READVTKHDF_READVTKHDF_H

#include <vistle/module/reader.h>

#include <hdf5.h>

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
    };

    bool readUnstructured(Token &token, hid_t file, int timestep, int block);
    bool readImage(Token &token, hid_t file, int block);
    bool addFields(Token &token, hid_t file, vistle::Object::const_ptr grid, const FieldRange &range, int step,
                   int timestep, int block, double realTime);

    vistle::StringParameter *m_filename = nullptr;
    vistle::StringParameter *m_fieldChoice[NumFields] = {};
    vistle::Port *m_gridOut = nullptr;
    vistle::Port *m_fieldOut[NumFields] = {};

    DataType m_type = DataType::None;
    bool m_temporal = false;
    StepInfo m_steps;
    std::vector<long long> m_numPoints, m_numCells, m_numConnectivity;
    long long m_extent[6] = {0, 0, 0, 0, 0, 0};
    double m_origin[3] = {0., 0., 0.};
    double m_spacing[3] = {1., 1., 1.};
};

#endif
