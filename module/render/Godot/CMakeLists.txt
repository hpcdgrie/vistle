#build the godot-cpp bindings
if(${VISTLE_DOUBLE_PRECISION})
    set(GODOTCPP_PRECISION
        "double"
        CACHE STRING "" FORCE)
else()
    set(GODOTCPP_PRECISION
        "single"
        CACHE STRING "" FORCE)
endif()
set(GODOTCPP_DEBUG_CRT
    ON
    CACHE BOOL "" FORCE)

add_subdirectory(godot-cpp)

# create the module library

set(HEADER GodotRenderer.h Godot.h register_types.h export.h)
set(SOURCES GodotRenderer.cpp Godot.cpp register_types.cpp)

add_module(GodotRenderer "render using Godot engine" ${HEADER} ${SOURCES})
target_include_directories(GodotRenderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp/include)
target_link_libraries(GodotRenderer PRIVATE vistle_renderer godot-cpp ${BOOST_MPI})

if(NOT ${VISTLE_MULTI_PROCESS})
    vistle_target_link_libraries(GodotRenderer PRIVATE vistle_clustermanager)
    target_compile_definitions(GodotRenderer INTERFACE VISTLE_SINGLE_PROCESS)
    target_compile_definitions(GodotRenderer PRIVATE VISTLE_ROOT="${CMAKE_BINARY_DIR}" VISTLE_BUILD_TYPE="")
endif()
