cmake_minimum_required(VERSION 3.29)
project(processing)

set(CMAKE_CXX_STANDARD 20)
find_package(LSL REQUIRED)
find_package(Python REQUIRED COMPONENTS Interpreter Development)
find_package(yaml-cpp REQUIRED)

set(CMAKE_PREFIX_PATH "/home/k-dawg/.local/lib/python3.10/site-packages/pybind11/share/cmake/pybind11" ${CMAKE_PREFIX_PATH})
find_package(pybind11 REQUIRED)

add_executable(processing main.cpp
        lib/Filter.cpp
        lib/FIR_Filter.cpp
        lib/FIR_Filter.h
        lib/IIR_Filter.cpp
        lib/IIR_Filter.h
        lib/xdfwriter.cpp
        lib/xdfwriter.h
        lib/conversions.h
        lib/Biquad.cpp
        lib/Biquad.h
        ../lib/config.cpp)

target_link_libraries(processing LSL::lsl pybind11::embed yaml-cpp)