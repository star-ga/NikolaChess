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
* **Iterative deepening alpha‑beta search:** Implements minimax search
  with alpha‑beta pruning and **iterative deepening**.  A
  transposition table caches evaluations of previously visited
  positions at different depths to avoid redundant work.  The search
  orders moves using **principal variation (PV) ordering** combined
  with **MVV‑LVA** and promotion heuristics to improve pruning
  efficiency.  A simple **time management system** aborts the search
  when a specified time limit expires, returning the best move found
  so far.  Depth is measured in plies.
* **Advanced move ordering:** In addition to PV and MVV‑LVA ordering,
  the engine uses **killer move** and **history heuristics** to
  prioritise moves that have historically led to alpha‑beta cutoffs.
  These heuristics update a table on each cutoff and boost the
  ordering score of successful moves.  **Aspiration windows** centre
  the search window around the previously observed principal
  variation value to accelerate convergence.
* **Draw detection:** The search detects draw claims by the
  fifty‑move rule, threefold repetition and a wide variety of
  insufficient material scenarios.  In addition to classical
  K+minor vs K draws, two knights versus bare king, opposite bishop
  endgames and other cases that cannot force checkmate are
  adjudicated as draws.  Draw adjudication is applied during the
  search and returns a neutral score when the conditions are met.
* **Sophisticated evaluation with optional neural network:** Combines
  material and piece‑square table values with a mobility term,
  **pawn structure heuristics** (penalising doubled and isolated
  pawns, rewarding passed pawns), and a **bishop pair bonus**.
  These heuristics follow common practices in chess engine design and
  reflect guidelines from the Chessprogramming Wiki on pawn
  structure and passed pawn evaluation【224297850074552†L34-L36】.  An
  optional neural network evaluator demonstrates how a modern
  NNUE‑style evaluation could be integrated; the included network is a
  small multi‑layer perceptron for illustration.  Loading external
  NNUE weights is supported via the evaluation stub for future
  experimentation【200842768436911†L13-L21】【200842768436911†L119-L123】.
* **CUDA‑accelerated evaluation:** Evaluates boards in batches on
  the GPU using a constant piece‑square table and the same pawn
  structure and bishop pair heuristics as the CPU.  The CPU performs
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
The search uses iterative deepening with a transposition table and
time management.  A search depth and time limit can be specified to
control runtime.  Evaluation includes pawn structure and bishop pair
heuristics in addition to material and piece‑square tables.

## Tuning and future directions

NikolaChess implements the full rules of chess, comprehensive draw
detection, iterative deepening with time management, transposition
tables, sophisticated evaluation heuristics and GPU acceleration.  As
a result there are **no missing features** needed for a functional
chess engine.  Nevertheless, there is ample room for improvement in
playing strength and performance.  Possible avenues include:

* **Enhanced time management:** Adapting the time allocation based on
  remaining time in a game or in a tournament setting, with
  techniques such as aspiration windows and dynamic depth adjustment.
* **Advanced move ordering:** Incorporating killer move and history
  heuristics to further improve alpha‑beta pruning efficiency.
* **Neural network evaluation:** Training or importing a full NNUE
  network as used in modern engines (e.g. Stockfish) would
  significantly enhance evaluation accuracy.【200842768436911†L13-L21】【200842768436911†L119-L123】  The
  included NNUE stub is provided as a template for such integration.

NikolaChess demonstrates how GPU acceleration can be integrated into a
traditional chess engine without rewriting the entire search engine
for the GPU.  While GPUs are not ideally suited for branchy,
recursive algorithms like alpha‑beta search, they excel at running
many independent evaluations concurrently【841273563187214†L123-L129】, which is a key
component of tree search.  We hope this project serves as a robust
baseline for experiments with more sophisticated search strategies
and evaluation methods on modern heterogeneous architectures.
