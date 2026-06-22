
#ifndef VISTLE_GODOT_GODOTRENDERER_H
#define VISTLE_GODOT_GODOTRENDERER_H

#include <vistle/renderer/renderer.h>
#include <vistle/core/object.h>
#include <vistle/core/parameter.h>
#include <map>

class GodotRenderer: public vistle::Renderer {
public:
    GodotRenderer(const std::string &name, int moduleID, mpi::communicator comm);

    std::shared_ptr<vistle::RenderObject> addObject(int senderId, const std::string &senderPort,
                                                    vistle::Object::const_ptr container, vistle::Object::const_ptr geom,
                                                    vistle::Object::const_ptr normal,
                                                    vistle::Object::const_ptr mapped) override;

    bool compute(const std::shared_ptr<vistle::BlockTask> &task) const override;
    bool changeParameter(const vistle::Parameter *p) override;

private:
    vistle::IntParameter *m_test;
};

#endif // VISTLE_GODOT_GODOTRENDERER_H
