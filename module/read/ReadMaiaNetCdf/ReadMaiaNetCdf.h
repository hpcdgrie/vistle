#ifndef READVTK_H
#define READVTK_H

#include <vistle/module/reader.h>

#include <vector>
#include <string>
#include <pnetcdf>

class ReadMaiaNetCdf: public vistle::Reader {
public:
    ReadMaiaNetCdf(const std::string &name, int moduleID, mpi::communicator comm);

    // reader interface
    bool examine(const vistle::Parameter *param) override;
    bool read(vistle::Reader::Token &token, int timestep = -1, int block = -1) override;
    bool prepareRead() override;
    bool finishRead() override;

private:
    static const int NumPorts = 3;

    //bool changeParameter(const vistle::Parameter *p) override;
    //bool compute() override;
    vistle::StringParameter *m_filename;

    bool load(Token &token, const std::string &filename, const vistle::Meta &meta = vistle::Meta(), int piece = -1,
              bool ghost = false, const std::string &part = std::string()) const;
};

#endif
