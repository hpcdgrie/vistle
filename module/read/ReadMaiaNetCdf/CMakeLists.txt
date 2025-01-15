

return()
add_module(ReadMaiaNetcdf "read netCdf data form RWTH mAia CFD code " ReadMaiaNetcdf.cpp ReadMaiaNetcdf.h)

vistle_find_package(HDF5 COMPONENTS C)
vistle_find_package(PNETCDF REQUIRED)
vistle_find_package(netCDF REQUIRED)
if(PNETCDF_FOUND)
    target_link_libraries(ReadMaiaNetcdf ${PNETCDF_LIBRARIES})
    target_include_directories(ReadMaiaNetcdf SYSTEM PRIVATE ${PNETCDF_INCLUDE_DIRS})
    message("linking ReadMaiaNetcdf against pnetcdf")
endif()

# vistle_find_package(NETCDF)
# if(NETCDF_FOUND)
#     add_module(ReadMaiaNetcdf "Read Duisburg files" ${SOURCES} ${HEADERS})
#     target_compile_definitions(ReadMaiaNetcdf PRIVATE USE_NETCDF)
#     if(NETCDF_PARALLEL)
#         target_compile_definitions(ReadMaiaNetcdf PRIVATE NETCDF_PARALLEL)
#     endif()
#     target_link_libraries(ReadMaiaNetcdf ${NETCDF_LIBRARIES} vistle_netcdf)
#     target_include_directories(ReadMaiaNetcdf SYSTEM PRIVATE ${NETCDF_INCLUDE_DIRS})
# endif()

target_link_libraries(ReadMaiaNetcdf ${NETCDF_LIBRARIES})


