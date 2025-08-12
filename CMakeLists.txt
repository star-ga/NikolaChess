cmake_minimum_required(VERSION 3.12)
project(NikolaChess LANGUAGES CXX CUDA)

# Set C++ standard and CUDA settings.  The engine is written in modern C++
# with separate CUDA kernels for evaluation.  Enabling separable
# compilation instructs CMake to perform the necessary GPU linking for
# objects compiled with nvcc.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable target for the engine.  Source files are split
# between C++ and CUDA.  Header only files are included automatically.
add_executable(nikolachess
    src/main.cpp
    src/board.h
    src/board.cpp
    src/move_generation.cu
    src/evaluate.cu
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
)

# Enable CUDA separable compilation for proper device linking.
set_target_properties(nikolachess PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 70 # Target compute capability 7.0 by default
)

# Include directories so that headers in src/ can be found during
# compilation.  Having a dedicated include directory would work as
# well, but keeping headers in src simplifies the project layout for
# this demonstration.
target_include_directories(nikolachess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link against standard C++ and CUDA runtime libraries implicitly.

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

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_sources(${PROJECT_NAME} PRIVATE ${SRC_HPC})
