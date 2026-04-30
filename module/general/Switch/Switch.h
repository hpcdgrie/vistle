#ifndef VISTLE_SWITCH_SWITCH_H
#define VISTLE_SWITCH_SWITCH_H

#include <vistle/module/module.h>
#include <vector>

constexpr int NumPorts = 4;

class Switch: public vistle::Module {
public:
    Switch(const std::string &name, int moduleID, mpi::communicator comm);

private:
    bool objectAdded(int sender, const std::string &senderPort, const vistle::Port *port) override;

    vistle::IntParameter *m_choice;
    std::array<vistle::Port *, NumPorts> m_inputs;
    std::vector<std::string> m_inputSpecies;
    vistle::Port *m_output;
};

#endif
