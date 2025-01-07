cmake_minimum_required(VERSION 3.29)
project(processing)

set(CMAKE_CXX_STANDARD 20)
find_package(LSL REQUIRED)
find_package(Python REQUIRED COMPONENTS Interpreter Development)
find_package(yaml-cpp REQUIRED)

set(Torch_DIR "/opt/libtorch/share/cmake/Torch")
find_package(Torch REQUIRED)
set(CMAKE_PREFIX_PATH "/home/k-dawg/.local/lib/python3.10/site-packages/pybind11/share/cmake/pybind11" ${CMAKE_PREFIX_PATH})
find_package(pybind11 REQUIRED)

add_executable(processing main.cpp
                filter/Filter.cpp
                filter/FIR_Filter.cpp
                filter/FIR_Filter.h
                filter/IIR_Filter.cpp
                filter/IIR_Filter.h
                ../lib/xdfwriter.cpp
                ../lib/xdfwriter.h
                ../lib/conversions.h
                filter/Biquad.cpp
                filter/Biquad.h
                ../lib/config.cpp
                ../lib/xdf_writer_template.h
                ../lib/xdf_writer_template.cpp
                spikesorting/online_std_dev.cpp
                spikesorting/online_std_dev.h
                processing.cpp
                processing.h
)

target_link_libraries(processing LSL::lsl pybind11::embed yaml-cpp "${TORCH_LIBRARIES}")