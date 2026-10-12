vistle_find_package(HDF5 COMPONENTS C)

if(NOT HDF5_FOUND)
    return()
endif()

add_module(ReadVtkHdf "read VTK HDF (.vtkhdf) files" ReadVtkHdf.cpp ReadVtkHdf.h)
target_include_directories(ReadVtkHdf SYSTEM PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(ReadVtkHdf PRIVATE ${HDF5_LIBRARIES})
