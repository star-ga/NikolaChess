cmake_minimum_required(VERSION 3.12)

# Toggle CUDA support (OFF by default). If ON we try to find a toolkit;
# if not found we still build CPU-only rather than hard-failing.
option(NIKOLA_USE_CUDA "Build NikolaChess with CUDA support" OFF)

project(NikolaChess LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to detect CUDA toolkit if the user asked for CUDA.
set(NIKOLA_HAS_CUDA FALSE)
if(NIKOLA_USE_CUDA)
  find_package(CUDAToolkit)
  if(CUDAToolkit_FOUND)
    enable_language(CUDA)
    set(NIKOLA_HAS_CUDA TRUE)
  else()
    message(WARNING "NIKOLA_USE_CUDA=ON but CUDA toolkit not found – building without GPU support")
  endif()
endif()

# Core/common sources
set(NIKOLA_LIB_SOURCES
  src/board.h
  src/board.cpp
  src/bitboard.cpp
  src/nnue.cpp
  src/move_generation.cpp
  src/search.cpp
  src/perft.cpp
  src/gpu_eval.cpp
  src/uci.cpp
  src/micro_batcher.cpp
  src/tablebase.cpp
  src/rules.cpp
  src/pgn_logger.cpp
  src/san.cpp
  src/see.cpp
  src/polyglot.cpp
  src/distributed.cpp
  src/engine_options.cpp
  src/time_manager.cpp
  src/pv.cpp
  src/multipv_search.cpp
  src/uci_extensions.cpp
)
# GPU-specific sources or CPU fallbacks
if(NIKOLA_HAS_CUDA)
  list(APPEND NIKOLA_LIB_SOURCES src/evaluate.cu)
else()
  list(APPEND NIKOLA_LIB_SOURCES src/evaluate_stub.cpp)
endif()
# Library and executable
add_library(nikola_core ${NIKOLA_LIB_SOURCES})
add_executable(nikolachess src/main.cpp)
target_link_libraries(nikolachess nikola_core)

# Include path for headers under src/
target_include_directories(nikola_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(nikolachess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# CUDA target properties (only if actually using CUDA)
if(NIKOLA_HAS_CUDA)
  set_target_properties(nikola_core PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 70
  )
endif()
# Optional MPI support for distributed search
option(NIKOLA_USE_MPI "Enable MPI distributed search" OFF)
if(NIKOLA_USE_MPI)
  find_package(MPI REQUIRED)
  target_compile_definitions(nikola_core PRIVATE NIKOLA_USE_MPI)
  target_include_directories(nikola_core PRIVATE ${MPI_INCLUDE_PATH})
  target_link_libraries(nikola_core PRIVATE MPI::MPI_CXX)
  target_link_libraries(nikolachess PRIVATE MPI::MPI_CXX)
endif()
# HPC / TT / affinity bits
set(SRC_HPC
  src/tt_entry.h
  src/tt_sharded.cpp
  src/thread_affinity.cpp
  src/cpu_features.cpp
)
target_sources(nikola_core PRIVATE ${SRC_HPC})
# Ensure property is set on the target (redundant but explicit)
set_target_properties(nikola_core PROPERTIES CXX_STANDARD 17)
set_target_properties(nikolachess PROPERTIES CXX_STANDARD 17)

# GoogleTest unit tests
include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)
enable_testing()
file(GLOB TEST_SOURCES tests/*_test.cpp)
add_executable(nikola_tests ${TEST_SOURCES})
target_link_libraries(nikola_tests nikola_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(nikola_tests)
