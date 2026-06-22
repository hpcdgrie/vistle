#include "Godot.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture3d.hpp>

#include <iostream>
#include <random>

#include <cassert>

#ifdef MODULE_THREAD
#include <boost/algorithm/string.hpp>
#include <boost/mpi/collectives.hpp>
#include <vistle/manager/manager.h>
#include <vistle/util/directory.h>

void SpawnVistle::start_vistle()
{
    vistle::directory::setVistleRoot(VISTLE_ROOT, VISTLE_BUILD_TYPE);
    int provided = MPI_THREAD_SINGLE;
    MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
        std::cerr << "insufficient thread support in MPI: MPI_THREAD_MULTIPLE is required (maybe set "
                     "MPICH_MAX_THREAD_SAFETY=multiple?)"
                  << std::endl;
        exit(1);
    }


    {
        // ensure that manager is destroyed before MPI_Finalize
        auto t = std::thread([]() {
            std::string cmd{VISTLE_ROOT};
            cmd += "/bin/vistle_manager";
            std::vector<const char *> args;
            args.push_back(cmd.c_str());
            // for (int i = 1; i < argc; i++) {
            //     args.push_back(argv[i]);
            // }
            vistle::VistleManager manager;
            manager.run(static_cast<int>(args.size()), const_cast<char **>(args.data()));
        });
        t.join();
    }
    std::cin.get(); // Wait for user input before exiting
    MPI_Finalize();
}


void ExampleClass::start_vistle(const godot::String &options)
{
    print_line(vformat("Starting Vistle with options: %s", options));
    std::string o = options.utf8().get_data();
    print_line(vformat("Parsed options: %s", o.c_str()));
    startVistle(MPI_COMM_WORLD, options.utf8().get_data());
}

bool ExampleClass::startVistle(const MPI_Comm &comm, const std::string &options)
{
    MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, nullptr);
    vistle::directory::setVistleRoot(VISTLE_ROOT, VISTLE_BUILD_TYPE);
    m_managerThread = std::thread(
        [](const std::string &options) {
            std::string cmd{VISTLE_ROOT};
            cmd += "/bin/vistle_manager";
            std::vector<const char *> args;
            args.push_back(cmd.c_str());
            args.push_back("--root");
            args.push_back(VISTLE_ROOT);

            // Parse and add additional options
            std::istringstream iss(options);
            std::string token;
            while (iss >> token) {
                args.push_back(token.c_str());
            }

            vistle::VistleManager manager;
            manager.run(static_cast<int>(args.size()), const_cast<char **>(args.data()));
        },
        options);
    return true;
}

ExampleClass::~ExampleClass()
{
    if (m_managerThread.joinable()) {
        m_managerThread.join();
    }
}


#endif // MODULE_THREAD

ExampleClass *ExampleClass::m_instance = nullptr;

ExampleClass *ExampleClass::instance()
{
    return m_instance;
}

ExampleClass::ExampleClass()
{
    assert(m_instance == nullptr);
    m_instance = this;
}

void ExampleClass::_bind_methods()
{
    godot::ClassDB::bind_method(D_METHOD("print_type", "variant"), &ExampleClass::print_type);
    godot::ClassDB::bind_method(D_METHOD("create_godot_object", "size", "count"), &ExampleClass::create_godot_object);
#ifdef MODULE_THREAD
    godot::ClassDB::bind_method(D_METHOD("start_vistle", "options"), &ExampleClass::start_vistle);
#endif
    godot::ClassDB::bind_method(D_METHOD("changed"), &ExampleClass::changed);
}

void ExampleClass::print_type(const Variant &p_variant) const
{
    print_line(vformat("Type: %d", p_variant.get_type()));
    auto path = getenv("path");
    print_line(vformat("PATH: %s", path ? path : "not set"));
}

void ExampleClass::setChanged()
{
    m_changed = true;
}

bool ExampleClass::changed()
{
    auto changed = m_changed;
    m_changed = false;
    return changed;
}

void ExampleClass::create_godot_object(float size, size_t count) const
{
    volumeRendering();
    createPointCloud(size, count);
}


void ExampleClass::createCube() const
{
    // create a cube in the scene to test that we can interact with Godot

    Ref<ArrayMesh> mesh = memnew(ArrayMesh);

    PackedVector3Array vertices;
    vertices.resize(8);
    vertices[0] = Vector3(-0.5f, -0.5f, -0.5f);
    vertices[1] = Vector3(0.5f, -0.5f, -0.5f);
    vertices[2] = Vector3(0.5f, 0.5f, -0.5f);
    vertices[3] = Vector3(-0.5f, 0.5f, -0.5f);
    vertices[4] = Vector3(-0.5f, -0.5f, 0.5f);
    vertices[5] = Vector3(0.5f, -0.5f, 0.5f);
    vertices[6] = Vector3(0.5f, 0.5f, 0.5f);
    vertices[7] = Vector3(-0.5f, 0.5f, 0.5f);

    // 12 triangles (2 per face), using the 8 vertices above.
    PackedInt32Array indices;
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(0);
    indices.push_back(2);
    indices.push_back(3); // Back

    indices.push_back(4);
    indices.push_back(6);
    indices.push_back(5);
    indices.push_back(4);
    indices.push_back(7);
    indices.push_back(6); // Front

    indices.push_back(0);
    indices.push_back(4);
    indices.push_back(5);
    indices.push_back(0);
    indices.push_back(5);
    indices.push_back(1); // Bottom

    indices.push_back(3);
    indices.push_back(2);
    indices.push_back(6);
    indices.push_back(3);
    indices.push_back(6);
    indices.push_back(7); // Top

    indices.push_back(1);
    indices.push_back(5);
    indices.push_back(6);
    indices.push_back(1);
    indices.push_back(6);
    indices.push_back(2); // Right

    indices.push_back(0);
    indices.push_back(3);
    indices.push_back(7);
    indices.push_back(0);
    indices.push_back(7);
    indices.push_back(4); // Left

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_INDEX] = indices;

    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

    MeshInstance3D *instance = memnew(MeshInstance3D);
    instance->set_mesh(mesh);

    // Add to current scene root if available, otherwise use parent.
    Node *scene_root = nullptr;
    if (SceneTree *tree = get_tree()) {
        print_line("Found scene tree, using current scene as root");
        scene_root = tree->get_current_scene();
    }
    if (!scene_root) {
        print_line("No current scene, using parent as root");
        scene_root = get_parent();
    }
    if (scene_root) {
        print_line(vformat("Adding cube instance to scene root: %s", scene_root->get_class()));
        scene_root->add_child(instance);
    } else {
        Node *owner_node = const_cast<ExampleClass *>(this);
        if (owner_node) {
            print_line(vformat("Adding cube instance to owner node: %s", owner_node->get_class()));
            owner_node->add_child(instance);
        } else
            print_line("No scene root found, cannot add cube instance to scene");
    }


    // Ref<Node> cube = Object::cast_to<Node>(ClassDB::instantiate("MeshInstance3D"));
    // if (cube.is_valid()) {
    //     Ref<Mesh> cube_mesh = Ref<Mesh>(ClassDB::instantiate("CubeMesh"));
    //     if (cube_mesh.is_valid()) {
    //         cube->set("mesh", cube_mesh);
    //         // Add the cube to the scene (assuming there's a root node called "Root")
    //         Node *parent = get_parent();
    //         // if (parent) {
    //         //     parent->add_child(cube);
    //         // }
    //     }
    // }
}


void ExampleClass::volumeRendering() const
{
    // Create cube mesh
    Ref<ArrayMesh> mesh = memnew(ArrayMesh);
    PackedVector3Array vertices;
    vertices.resize(8);
    vertices[0] = Vector3(-0.5f, -0.5f, -0.5f);
    vertices[1] = Vector3(0.5f, -0.5f, -0.5f);
    vertices[2] = Vector3(0.5f, 0.5f, -0.5f);
    vertices[3] = Vector3(-0.5f, 0.5f, -0.5f);
    vertices[4] = Vector3(-0.5f, -0.5f, 0.5f);
    vertices[5] = Vector3(0.5f, -0.5f, 0.5f);
    vertices[6] = Vector3(0.5f, 0.5f, 0.5f);
    vertices[7] = Vector3(-0.5f, 0.5f, 0.5f);

    PackedInt32Array indices;
    // Front face
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(0);
    indices.push_back(2);
    indices.push_back(3);
    // Back face
    indices.push_back(5);
    indices.push_back(4);
    indices.push_back(7);
    indices.push_back(5);
    indices.push_back(7);
    indices.push_back(6);
    // Left face
    indices.push_back(4);
    indices.push_back(0);
    indices.push_back(3);
    indices.push_back(4);
    indices.push_back(3);
    indices.push_back(7);
    // Right face
    indices.push_back(1);
    indices.push_back(5);
    indices.push_back(6);
    indices.push_back(1);
    indices.push_back(6);
    indices.push_back(2);
    // Top face
    indices.push_back(3);
    indices.push_back(2);
    indices.push_back(6);
    indices.push_back(3);
    indices.push_back(6);
    indices.push_back(7);
    // Bottom face
    indices.push_back(4);
    indices.push_back(5);
    indices.push_back(1);
    indices.push_back(4);
    indices.push_back(1);
    indices.push_back(0);

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_INDEX] = indices;
    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

    // Create 3D volume data (e.g., 32x32x32 grid)
    int size = 32;
    Array slices;
    slices.resize(size);

    for (int z = 0; z < size; ++z) {
        PackedByteArray slice_data;
        slice_data.resize(size * size);

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float fx = (x - size / 2.0f) / (size / 2.0f);
                float fy = (y - size / 2.0f) / (size / 2.0f);
                float fz = (z - size / 2.0f) / (size / 2.0f);
                float dist = sqrt(fx * fx + fy * fy + fz * fz);
                uint8_t density = (dist < 1.0f) ? uint8_t(255 * (1.0f - dist)) : 0;
                slice_data[x + y * size] = density;
            }
        }

        Ref<Image> slice = Image::create_from_data(size, size, false, Image::FORMAT_L8, slice_data);
        slices[z] = slice;
    }
    Ref<ImageTexture3D> volume_texture = memnew(ImageTexture3D);
    volume_texture->create(Image::FORMAT_L8, size, size, size, false, slices);

    // Apply shader
    Ref<ShaderMaterial> mat = memnew(ShaderMaterial);
    Ref<Shader> shader = memnew(Shader);
    shader->set_code(R"(
        shader_type spatial;
        render_mode blend_mix, depth_draw_opaque, cull_back, unshaded;

        uniform sampler3D volume_texture : filter_linear;

        varying vec3 local_pos;

        void vertex() {
            local_pos = VERTEX;
        }

        void fragment() {
            vec3 uvw = clamp(local_pos + vec3(0.5), vec3(0.0), vec3(1.0));
            float density = texture(volume_texture, uvw).r;
            ALBEDO = vec3(density);
            ALPHA = density;
        }
    )");
    mat->set_shader(shader);
    mat->set_shader_parameter("volume_texture", volume_texture);

    mesh->surface_set_material(0, mat);

    // Add to scene
    MeshInstance3D *instance = memnew(MeshInstance3D);
    instance->set_mesh(mesh);
    get_parent()->add_child(instance);
}

void ExampleClass::createPointCloud(float size, size_t count) const
{
    Ref<ArrayMesh> mesh = memnew(ArrayMesh);

    PackedVector3Array vertices;
    vertices.resize(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (int i = 0; i < vertices.size(); ++i) {
        vertices[i] = Vector3(dist(gen), dist(gen), dist(gen));
    }

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;

    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, arrays);


    Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_albedo(Color(1.0f, 0.0f, 0.0f, 1.0f));
    mat->set_flag(StandardMaterial3D::FLAG_USE_POINT_SIZE, true);
    mat->set_point_size(size);
    mesh->surface_set_material(0, mat);

    MeshInstance3D *instance = memnew(MeshInstance3D);
    instance->set_mesh(mesh);

    Node *scene_root = nullptr;
    if (SceneTree *tree = get_tree()) {
        print_line("Found scene tree, using current scene as root");
        scene_root = tree->get_current_scene();
    }
    if (!scene_root) {
        print_line("No current scene, using parent as root");
        scene_root = get_parent();
    }
    if (scene_root) {
        print_line(vformat("Adding point cloud instance to scene root: %s", scene_root->get_class()));
        scene_root->add_child(instance);
    } else {
        Node *owner_node = const_cast<ExampleClass *>(this);
        if (owner_node) {
            print_line(vformat("Adding point cloud instance to owner node: %s", owner_node->get_class()));
            owner_node->add_child(instance);
        } else {
            print_line("No scene root found, cannot add point cloud instance to scene");
        }
    }
}
