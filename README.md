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
  knight, bishop, rook, queen and king moves.  Castling and en
  passant are fully supported and checked for legality.  Pawn
  promotions now generate all four options (queen, rook, bishop and
  knight) rather than always promoting to a queen.  The generator
  filters out any move that would leave the moving side’s king in
  check.
* **Alpha‑beta search:** Implements a basic minimax search with
  alpha‑beta pruning on the CPU.  Depth is measured in plies.
* **Draw detection:** The search detects draw claims by the
  fifty‑move rule, threefold repetition and certain insufficient
  material scenarios (K vs K, K+B vs K, K+N vs K, K+B vs K+B on
  same‑colour squares, and K+N vs K+N).  Draw adjudication is
  applied during the search and returns a neutral score when the
  conditions are met.
* **Improved evaluation with optional neural network:** Uses
  piece‑square tables derived from the PeSTO evaluation function to
  combine material and positional bonuses into a single score.
  A mobility term rewards the side with more legal moves.  An
  optional neural network evaluator demonstrates how a modern
  NNUE‑style evaluation could be integrated; the included network is a
  small multi‑layer perceptron for illustration.
* **CUDA‑accelerated evaluation:** Evaluates boards in batches on
  the GPU using a constant piece‑square table.  The CPU performs
  move generation and search while the GPU computes static scores for
  many positions concurrently.  On systems without a compatible GPU
  the engine falls back to the CPU evaluator.
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

The current move generator enforces the rules of chess: castling,
en passant and pawn promotions are all implemented, and moves that
leave the moving side in check are discarded.  The engine detects
draw claims by the fifty‑move rule and threefold repetition and
returns a neutral evaluation in such cases.  It also treats some
insufficient‑material endgames as drawn.

## Limitations and future work

* **Move generation completeness:** Underpromotions to queen, rook,
  bishop and knight are implemented.  Castling and en passant follow
  the official rules of chess and have been tested for common
  scenarios.  The engine now detects draw claims by the fifty‑move
  rule and threefold repetition and adjudicates simple
  insufficient‑material endgames.  More exotic drawn positions (e.g.
  positions with two knights that cannot force checkmate) are
  approximated but not rigorously handled.
* **Search depth:** The built‑in minimax search depth is modest to
  keep runtime manageable.  Implementing iterative deepening,
  transposition tables, move ordering heuristics and a time
  management system would greatly improve playing strength.
* **Evaluation function:** The evaluation combines material,
  piece‑square tables【406358174428833†L70-L95】 and a simple mobility term.  A
  placeholder neural network is provided to show how a learned
  evaluator could be hooked in.  Training or integrating a
  state‑of‑the‑art NNUE network【200842768436911†L13-L21】【200842768436911†L119-L123】 remains future work.

NikolaChess demonstrates how GPU acceleration can be integrated into a
traditional chess engine without rewriting the entire search engine
for the GPU.  While GPUs are not ideally suited for branchy,
recursive algorithms like alpha‑beta search, they excel at running
many independent evaluations concurrently, which is a key component of
tree search.  We hope this project serves as a starting point for
experiments with more sophisticated search strategies and evaluation
methods on modern heterogeneous architectures.
