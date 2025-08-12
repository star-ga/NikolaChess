cmake_minimum_required(VERSION 3.12)

# Allow building without the CUDA toolkit.  When NIKOLA_USE_CUDA is
# enabled CMake will attempt to find a CUDA compiler and the GPU
# evaluation sources will be compiled.  Otherwise the engine builds in a
# CPU‑only configuration which is useful for environments without CUDA
# installed, such as continuous integration runners.
option(NIKOLA_USE_CUDA "Build NikolaChess with CUDA support" OFF)

project(NikolaChess LANGUAGES CXX)

# Set C++ standard for the whole project.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core engine sources common to both CPU and GPU builds.
set(NIKOLA_SOURCES
    src/main.cpp
    src/board.h
    src/board.cpp
    src/move_generation.cpp
    src/search.cpp
    src/perft.cpp
    src/gpu_eval.cpp
    src/uci.cpp
    src/micro_batcher.cpp
    src/tablebase.cpp
    src/pgn_logger.cpp
    src/san.cpp
    src/see.cpp
    src/polyglot.cpp
    src/distributed.cpp
    src/rules.cpp)

# When CUDA is requested, enable the language and add GPU evaluation
# kernels.  The optional sources require the CUDA toolkit.
if(NIKOLA_USE_CUDA)
    enable_language(CUDA)
    list(APPEND NIKOLA_SOURCES src/evaluate.cu)
    set_source_files_properties(src/evaluate.cu PROPERTIES LANGUAGE CUDA)
endif()

# Add the executable target for the engine using the accumulated source list.
add_executable(nikolachess ${NIKOLA_SOURCES})

# Include directories so that headers in src/ can be found during
# compilation.  Having a dedicated include directory would work as
# well, but keeping headers in src simplifies the project layout for
# this demonstration.
target_include_directories(nikolachess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link against standard runtime libraries implicitly.

# If CUDA support is enabled, configure device linking properties for the
# executable target after it has been created.
if(NIKOLA_USE_CUDA)
    set_target_properties(nikolachess PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES 70)
endif()

# Optional MPI support for distributed search.  Users can enable
# NIKOLA_USE_MPI when configuring CMake to compile the distributed
# search code.  If enabled, we locate the MPI package, define
# NIKOLA_USE_MPI for conditional compilation and link against the
# MPI C++ library.
option(NIKOLA_USE_MPI "Enable MPI distributed search" OFF)
if(NIKOLA_USE_MPI)
    find_package(MPI REQUIRED)
    target_compile_definitions(nikolachess PRIVATE NIKOLA_USE_MPI)
    target_include_directories(nikolachess PRIVATE ${MPI_INCLUDE_PATH})
    target_link_libraries(nikolachess PRIVATE MPI::MPI_CXX)
endif()
# HPC additions
set(SRC_HPC
  src/tt_entry.h
  src/tt_sharded.cpp
  src/thread_affinity.cpp
  src/cpu_features.cpp)

set_target_properties(nikolachess PROPERTIES CXX_STANDARD 17)
target_sources(nikolachess PRIVATE ${SRC_HPC})
