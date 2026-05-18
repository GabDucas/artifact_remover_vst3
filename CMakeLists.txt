cmake_minimum_required(VERSION 3.15)

# =========================================================
# Project
# =========================================================

project(
    artifact_remover
    VERSION 0.1
    LANGUAGES CXX
)

# Force 64-bit build for MKL compatibility
if(NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "Platform" FORCE)
endif()

# =========================================================
# C++ standard
# =========================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================
# Build type
# =========================================================

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# =========================================================
# Compiler warnings
# =========================================================

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -O3)
endif()

# =========================================================
# Dependencies
# =========================================================
# set(Eigen3_DIR "C:\\Program Files (x86)\\Eigen3")

# find_package(Eigen3 REQUIRED)
find_package(MKL CONFIG QUIET)
# find_package(FFTW REQUIRED)

option(USE_MKL "Use Intel MKL for linear algebra" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# =========================================================
# Submodules
# =========================================================

# add_subdirectory(src/utils)

# =========================================================
# Executable
# =========================================================

add_library(
    artifact_remover
)

# add_executable(
#     demo_app
#     demo/RemoverExample.cpp
# )

add_executable(
    audio_test
    demo/JuceAudioProcessingTest.cpp
)

target_sources(artifact_remover
    PRIVATE
        src/SignalDecomposition.cpp
        src/FFT.cpp
        src/remover.cpp
    
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES 
            include/artifact_remover/Types.h
            include/artifact_remover/SignalDecomposition.h
            include/artifact_remover/FFT.h
            include/artifact_remover/remover.h
)

set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
  pocketfft
  GIT_REPOSITORY https://github.com/mreineck/pocketfft.git
  GIT_TAG cpp
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/pocketfft"
)

FetchContent_Declare(
    Eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/eigen"
    )

FetchContent_MakeAvailable(pocketfft)
FetchContent_MakeAvailable(Eigen)

target_include_directories(artifact_remover
 PRIVATE
    ${pocketfft_SOURCE_DIR}
    ${Eigen_SOURCE_DIR}
)
add_subdirectory(juce_plugin)
    

# =========================================================
# Link libraries
# =========================================================
target_link_libraries(artifact_remover
    PRIVATE
    Eigen3::Eigen
)

set(MKL_ROOT "C:/Program Files (x86)/Intel/oneAPI/mkl/2026.0")
set(MKL_INCLUDE_DIR "${MKL_ROOT}/include")
set(MKL_LIB_DIR "${MKL_ROOT}/lib")

if(EXISTS "${MKL_INCLUDE_DIR}/mkl.h")
    message(STATUS "Using MKL for linear algebra (Release build)")
    message(STATUS "MKL include dir: ${MKL_ROOT}")

    target_include_directories(artifact_remover PRIVATE "${MKL_INCLUDE_DIR}")
    
    # Link MKL libraries
    target_link_directories(artifact_remover PRIVATE "${MKL_LIB_DIR}")

    target_link_libraries(artifact_remover
        PUBLIC
        ${MKL_ROOT}/lib/mkl_intel_lp64.lib
        ${MKL_ROOT}/lib/mkl_core.lib
        ${MKL_ROOT}/lib/mkl_sequential.lib
    )
    target_compile_definitions(artifact_remover
        PRIVATE
            EIGEN_USE_MKL_ALL
            EIGEN_MKL_DEFAULT
    )
else()
    message(STATUS "Using Eigen built-in algorithms")
    target_compile_definitions(artifact_remover
        PRIVATE
            EIGEN_POCKETFFT_DEFAULT
    )
endif()
# target_link_libraries(demo_app
#     PRIVATE
#     artifact_remover
#     Eigen3::Eigen
# )

target_link_libraries(audio_test
    PRIVATE
    artifact_remover
    Eigen3::Eigen
)
