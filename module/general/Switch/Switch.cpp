#include "Switch.h"

MODULE_MAIN(Switch)

using namespace vistle;

std::vector<std::string> defaultSpeceies()
{
    std::vector<std::string> species;
    for (int i = 0; i < NumPorts; ++i) {
        species.push_back(std::to_string(i));
    }
    return species;
}

Switch::Switch(const std::string &name, int moduleID, mpi::communicator comm)
: Module(name, moduleID, comm), m_inputSpecies(defaultSpeceies())
{
    for (int i = 0; i < NumPorts; ++i) {
        m_inputs[i] = createInputPort("in" + std::to_string(i), "input " + std::to_string(i), Port::Flags::NOCOMPUTE);
    }

    m_output = createOutputPort("out", "output");

    m_choice =
        addIntParameter("choice", "choose the input that is forwarded to output", 0, Parameter::Presentation::Choice);


    setParameterChoices(m_choice, m_inputSpecies);
    configureParameter(m_choice, Parameter::RendererGui, 1);
}

bool Switch::objectAdded(int sender, const std::string &senderPort, const Port *port)
{
    size_t index = std::distance(m_inputs.begin(), std::find(m_inputs.begin(), m_inputs.end(), port));
    auto species = port->objects().front()->getAttribute(attribute::Species);
    auto datasetName = port->objects().front()->getAttribute(attribute::DatasetName);
    if (m_inputSpecies[index] != species) {
        m_inputSpecies[index] = datasetName.empty() ? species : datasetName + ":" + species;
        setParameterChoices(m_choice, m_inputSpecies);
    }

    const int choice = m_choice->getValue();
    if (choice == index) {
        auto in = m_inputs[index];
        Object::const_ptr data = expect<Object>(in);
        if (!data) {
            sendError("no object on input port");
            return false;
        }
        auto ndata = data->clone();
        updateMeta(ndata);
        addObject(m_output, ndata);
    }

    return true;
}
