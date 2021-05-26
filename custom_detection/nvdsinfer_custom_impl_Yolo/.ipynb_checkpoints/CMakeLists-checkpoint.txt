cmake_minimum_required(VERSION 3.0)

project(nvds-infer-custom-imp-yolo VERSION 1.0.0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -fPIC")
option(BUILD_TESTS "Build tests alongside the project" OFF)

message(STATUS "Using toolkit chain file ${CMAKE_TOOLKIT_CHAIN}")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
  include_directories(
    ${CMAKE_SYSROOT}/usr/include/aarch64-linux-gnu
    ${CMAKE_SYSROOT}/usr/local/cuda/targets/aarch64-linux/include)
  link_directories(${CMAKE_SYSROOT}/usr/local/cuda/targets/aarch64-linux/lib)
else()
  include_directories(${CMAKE_SYSROOT}/usr/local/cuda/include)
  link_directories(${CMAKE_SYSROOT}/usr/local/cuda/lib64)
endif()

find_package(CUDA REQUIRED)

set(DEEPSTREAM_PATH ${CMAKE_SYSROOT}/opt/nvidia/deepstream/deepstream)
set(DEEPSTREAM_INCLUDE_DIR ${DEEPSTREAM_PATH}/sources/includes)

include_directories(${DEEPSTREAM_INCLUDE_DIR})
include_directories(${CMAKE_SYSROOT}/usr/src/tensorrt/samples/common)

link_directories(${DEEPSTREAM_LIBRARY_DIR} ${DEEPSTREAM_LIBRARY_DIR}/gst-plugins)

set(CMAKE_CXX_STANDARD 11)
set(CUDA_NVCC_PLAGS ${CUDA_NVCC_PLAGS};-std=c++11;-g;-G;-gencode;arch=compute_60;code=sm_60;-gencode;arch=compute_61;code=sm_61)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Ofast -Wfatal-errors -D_MWAITXINTRIN_H_INCLUDED")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
cuda_add_library(nvdsinfer_custom_impl_Yolo SHARED yololayer.cu nvdsparsebbox_Yolo.cpp)

target_link_libraries(nvdsinfer_custom_impl_Yolo 
    nvinfer_plugin
    nvinfer
    nvparsers
    cudart
    cublas
    stdc++fs)
