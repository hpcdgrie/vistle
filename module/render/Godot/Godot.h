#ifndef VISTLE_GODOT_GODOT_H
#define VISTLE_GODOT_GODOT_H


#include <godot_cpp/classes/node3d.hpp>
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "export.h"

#ifdef MODULE_THREAD

#include <mpi.h>
#include <thread>
#endif
using namespace godot;

class ExampleClass: public Node3D {
    GDCLASS(ExampleClass, Node3D)

protected:
    static void _bind_methods();

public:
    static ExampleClass *instance();

    ExampleClass();
#ifdef MODULE_THREAD
    ~ExampleClass();
#endif


    void print_type(const Variant &p_variant) const;
    void create_godot_object(float size, size_t count) const;
    void createCube() const;
    void createPointCloud(float size, size_t count) const;
    void volumeRendering() const;
    void setChanged();
    bool changed();
#ifdef MODULE_THREAD
    void start_vistle(const godot::String &options = godot::String(""));
#endif

private:
    static ExampleClass *m_instance;
    bool m_changed = false;
#ifdef MODULE_THREAD
    std::thread m_managerThread;
    bool startVistle(const MPI_Comm &comm, const std::string &options);
#endif
};

#endif // VISTLE_GODOT_GODOT_H
