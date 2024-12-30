cmake_minimum_required(VERSION 3.29)
project(sim)

set(CMAKE_CXX_STANDARD 20)

find_package(LSL REQUIRED)
find_package(libxdf REQUIRED)
find_library(PUGIXML_LIB pugixml REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MATIO REQUIRED matio)
find_package(yaml-cpp REQUIRED)

link_directories(/usr/lib/x86_64-linux-gnu/hdf5/serial)

add_executable(sim main.cpp
        ../lib/config.cpp
        ../lib/sim_file_io.h
        ../lib/sim_file_io.cpp)

target_link_libraries(sim PRIVATE LSL::lsl ${MATIO_LIBRARIES} hdf5 z yaml-cpp curl xdf pugixml)
target_include_directories(sim PRIVATE ${MATIO_INCLUDE_DIRS})
