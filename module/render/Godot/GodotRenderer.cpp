
#include "GodotRenderer.h"
#include <vistle/core/geometry.h>
#include "Godot.h"
using namespace vistle;

MODULE_MAIN(GodotRenderer)

GodotRenderer::GodotRenderer(const std::string &name, int moduleID, mpi::communicator comm)
: Renderer(name, moduleID, comm)
{
    m_test = addIntParameter("test", "test parameter", 0);
}

std::shared_ptr<vistle::RenderObject> GodotRenderer::addObject(int senderId, const std::string &senderPort,
                                                               vistle::Object::const_ptr container,
                                                               vistle::Object::const_ptr geom,
                                                               vistle::Object::const_ptr normal,
                                                               vistle::Object::const_ptr mapped)
{
    return nullptr;
}


bool GodotRenderer::compute(const std::shared_ptr<BlockTask> &task) const
{
    (void)task;
    return true;
}

bool GodotRenderer::changeParameter(const Parameter *p)
{
    if (p == m_test) {
        std::cerr << "test parameter changed to " << m_test->getValue() << std::endl;
        assert(ExampleClass::instance() != nullptr);
        ExampleClass::instance()->setChanged();

        return true;
    }
    return Renderer::changeParameter(p);
}
