cmake_minimum_required(VERSION 3.15)

# =========================================================
# Project
# =========================================================

project(
    artifact_remover
    VERSION 0.1
    LANGUAGES CXX
)

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
# find_package(MKL CONFIG REQUIRED)
# find_package(FFTW REQUIRED)

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

add_executable(
    demo_app
    apps/demo/RemoverExample.cpp
)

target_sources(artifact_remover
    PRIVATE
        src/core/SignalDecomposition.cpp
        src/core/FFT.cpp
        src/dsp/remover.cpp
    
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES 
            include/artifact_remover/core/SignalDecomposition.h
            include/artifact_remover/core/FFT.h
            include/artifact_remover/dsp/remover.h
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

if(MSVC)
    find_package(MKL QUIET)
    if (NOT MKL_FOUND)
        message(WARNING "MKL not found. Please install MKL and set MKL_DIR to the appropriate path. Using default library instead")
    else()
        target_link_libraries(artifact_remover
        PRIVATE
        MKL::MKL
    )
    endif()

endif()
    

# =========================================================
# Link libraries
# =========================================================
target_link_libraries(artifact_remover
    PRIVATE
    Eigen3::Eigen
)

target_compile_definitions(artifact_remover
    PRIVATE
        EIGEN_POCKETFFT_DEFAULT
)
if (MSVC && MKL_FOUND) 
    target_compile_definitions(artifact_remover
        PRIVATE
            EIGEN_USE_MKL_ALL
    )
endif()

target_link_libraries(demo_app
    PRIVATE
    artifact_remover
    Eigen3::Eigen
)
