add_module(VertexOverlap "find vertices shared with other ranks using DIY" VertexOverlap.cpp)

target_include_directories(VertexOverlap PRIVATE ${DIY_INCLUDE_DIR})
if(MSVC)
    target_compile_options(VertexOverlap PRIVATE /utf-8 /permissive)
endif()
