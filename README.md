# NikolaChess

NikolaChess is a minimal yet complete chess engine designed to
demonstrate how **general purpose GPUs (GPGPUs)** can accelerate
certain aspects of traditional alpha‑beta search.  Rather than
attempting to parallelise the entire game tree on the GPU – an
approach that has seen limited success for chess because of the
branch‑dependent nature of alpha‑beta pruning – this engine uses
CUDA to offload the **static evaluation** of many positions in
parallel while the CPU performs move generation and search.  The
Chessprogramming Wiki summarises the main avenues for using GPUs in
computer chess: as an accelerator for running neural‑network
evaluations (e.g. Lc0), offloading parallel game tree search (e.g.
Zeta), hybrid CPU/GPU perft calculations, and neural‑network
training【59046458589607†L128-L139】.  NikolaChess follows the first
approach by keeping the search on the CPU and accelerating the
evaluation.

CUDA is NVIDIA’s parallel computing platform and programming model
that enables “GPU‑accelerated applications” in which **the sequential
part of the workload runs on the CPU while the compute‑intensive
portion runs on thousands of GPU cores in parallel**【841273563187214†L123-L129】.  This
hybrid model fits naturally with game engines: move generation and
tree search are inherently sequential, but evaluating many leaf nodes
can be performed simultaneously.  NikolaChess therefore batches
board positions and evaluates them on the GPU using a simple CUDA
kernel, freeing the CPU to continue the search.

## Features

* **Modern C++ with CUDA:** Uses C++17 and CUDA C for GPU kernels.
* **Simple 8×8 array representation:** The engine uses an array of
  signed bytes to represent pieces; positive values are White and
  negative values Black.  The board structure is trivially copyable to
  the GPU.
* **Legal move generator with special moves:** Generates pawn,
  knight, bishop, rook, queen and king moves.  Castling, en passant
  and pawn promotion to queen are supported.  Moves that leave the
  moving side’s king in check are automatically discarded.
* **Alpha‑beta search:** Implements a basic minimax search with
  alpha‑beta pruning on the CPU.  Depth is measured in plies.
* **CUDA‑accelerated evaluation:** Evaluates material and simple
  positional bonuses for many boards in parallel on the GPU.  On
  systems without a compatible GPU the engine automatically falls back
  to CPU evaluation.
* **Modular design:** Board representation, move generation, search
  and evaluation are split into separate source files for clarity.

## Building

NikolaChess uses [CMake](https://cmake.org/) for configuration and
requires a recent NVIDIA GPU with CUDA Toolkit installed.  On Linux
with CUDA 11 or newer the following commands build the engine:

```sh
cd NikolaChess
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

The resulting executable `nikolachess` will be placed in the `build`
directory.  If CMake cannot find CUDA or your compiler does not
support C++17, adjust the toolchain accordingly.  You can control the
target compute capability via the `CUDA_ARCHITECTURES` property in
`CMakeLists.txt`.

## Running

Running the engine without arguments will initialise the standard
starting position, print its evaluation using both CPU and GPU
evaluators, and search to a depth of two plies (one move for each
side) before printing the chosen move:

```sh
./nikolachess
CPU evaluation of starting position: 0
GPU evaluation of starting position: 0
Engine selects move: b1 -> a3
```

Because the move generator does not yet filter out moves that leave
the king in check, some moves produced may be illegal in real chess.
Adding full legality checking and support for special moves would be a
natural extension.

## Limitations and future work

* **Move generation completeness:** Only promotions to queen are
  supported.  Underpromotions to rook, bishop and knight could be
  added for full compliance.  The en passant and castling rules are
  implemented but have not been thoroughly tested in tournament play.
* **Search depth:** The built‑in minimax search depth is modest to
  keep runtime manageable.  Implementing iterative deepening and
  transposition tables would greatly improve playing strength.
* **Evaluation function:** The evaluation considers only material and
  simple centralisation bonuses.  More sophisticated heuristics and
  neural network evaluators could be integrated following the
  approaches highlighted on the GPU‑chess page【59046458589607†L128-L139】.

NikolaChess demonstrates how GPU acceleration can be integrated into a
traditional chess engine without rewriting the entire search engine
for the GPU.  While GPUs are not ideally suited for branchy,
recursive algorithms like alpha‑beta search, they excel at running
many independent evaluations concurrently, which is a key component of
tree search.  We hope this project serves as a starting point for
experiments with more sophisticated search strategies and evaluation
methods on modern heterogeneous architectures.
