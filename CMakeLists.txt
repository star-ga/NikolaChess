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
set(NIKOLA_SOURCES
  src/main.cpp
  src/board.h
  src/board.cpp
  src/move_generation.cu
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
  # (from master side)
  src/engine_options.cpp
  src/time_manager.cpp
  src/pv.cpp
  src/multipv_search.cpp
  src/uci_extensions.cpp
)

# GPU-specific sources or CPU fallbacks
  if(NIKOLA_HAS_CUDA)
    list(APPEND NIKOLA_SOURCES src/cuda_eval.cu)
  # Treat CUDA files properly and enable device linking.
  set_source_files_properties(
      src/move_generation.cu
      src/cuda_eval.cu
    PROPERTIES LANGUAGE CUDA
  )
else()
  # Build move generation as C++ and use a CPU stub for evaluation.
  # (The stub should provide the same symbols as evaluate.cu.)
  set_source_files_properties(
    src/move_generation.cu
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x c++"
  )
  list(APPEND NIKOLA_SOURCES src/evaluate_stub.cpp)
endif()

# Executable
add_executable(nikolachess ${NIKOLA_SOURCES})

# Include path for headers under src/
target_include_directories(nikolachess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# CUDA target properties (only if actually using CUDA)
if(NIKOLA_HAS_CUDA)
  set_target_properties(nikolachess PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 70  # adjust for your GPUs
  )
endif()

# Optional MPI support for distributed search
option(NIKOLA_USE_MPI "Enable MPI distributed search" OFF)
if(NIKOLA_USE_MPI)
  find_package(MPI REQUIRED)
  target_compile_definitions(nikolachess PRIVATE NIKOLA_USE_MPI)
  target_include_directories(nikolachess PRIVATE ${MPI_INCLUDE_PATH})
  target_link_libraries(nikolachess PRIVATE MPI::MPI_CXX)
endif()

# Optional NCCL support for multi-GPU communication
option(NIKOLA_USE_NCCL "Enable NCCL multi-GPU communication" OFF)
if(NIKOLA_USE_NCCL)
  find_path(NCCL_INCLUDE_DIR nccl.h)
  find_library(NCCL_LIBRARY nccl)
  if(NCCL_INCLUDE_DIR AND NCCL_LIBRARY)
    target_compile_definitions(nikolachess PRIVATE NIKOLA_USE_NCCL)
    target_include_directories(nikolachess PRIVATE ${NCCL_INCLUDE_DIR})
    target_link_libraries(nikolachess PRIVATE ${NCCL_LIBRARY})
  endif()
endif()

# HPC / TT / affinity bits
set(SRC_HPC
  src/tt_entry.h
  src/tt_sharded.cpp
  src/thread_affinity.cpp
  src/cpu_features.cpp
)
target_sources(nikolachess PRIVATE ${SRC_HPC})

# Ensure property is set on the target (redundant but explicit)
set_target_properties(nikolachess PROPERTIES CXX_STANDARD 17)

enable_testing()

add_executable(tbprobe_tests
  tests/tbprobe_tests.cpp
  tests/fathom_stub.cpp
  src/tbprobe.cpp
)
target_include_directories(tbprobe_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME tbprobe_tests COMMAND tbprobe_tests)
