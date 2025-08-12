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