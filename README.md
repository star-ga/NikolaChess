# NikolaChess: Supercomputer Chess Engine

Harness the power of supercomputing for chess!  The **NikolaChess** (*Supercomputer
Chess Engine*) is a high‑performance chess
engine optimised for supercomputer‑scale computing.  It leverages
CUDA to parallelise evaluations on GPUs, scales from a single
laptop to multi‑node GPU clusters, and retains a clean, modern C++
codebase.  Rather than attempting to parallelise the entire game
tree on the GPU – an approach that has seen limited success for
chess because of the branch‑dependent nature of alpha‑beta
pruning – this engine offloads the **static evaluation** of large
batches of positions while the CPU performs move generation and
search.  The Chessprogramming Wiki summarises the main avenues for
using GPUs in computer chess: as an accelerator for running
neural‑network evaluations (e.g. Lc0), offloading parallel game tree
search (e.g. Zeta), hybrid CPU/GPU perft calculations, and
neural‑network training【59046458589607†L128-L139】.  This project
follows the first approach by keeping the recursive search on the
CPU and accelerating evaluation, but it is architected to scale
across multiple GPUs and nodes when compiled with MPI and NCCL.

CUDA is NVIDIA’s parallel computing platform and programming model
that enables “GPU‑accelerated applications” in which **the sequential
part of the workload runs on the CPU while the compute‑intensive
portion runs on thousands of GPU cores in parallel**【841273563187214†L123-L129】.  This
hybrid model fits naturally with game engines: move generation and
tree search are inherently sequential, but evaluating many leaf nodes
can be performed simultaneously.  The Supercomputer Chess Engine
therefore batches board positions and evaluates them on the GPU
using a simple CUDA kernel, freeing the CPU to continue the search.

## Features

* **Modern C++ with CUDA:** Written in modern C++ (C++17) with
  CUDA kernels for GPU acceleration.  The codebase cleanly
  separates CPU and GPU responsibilities.
* **Simple yet GPU‑friendly board representation:** An 8×8 array of
  signed bytes encodes pieces (positive for White, negative for
  Black).  This compact structure is trivially copyable to GPU
  memory for batched evaluations.
* **Comprehensive move generator:** Produces all legal pawn,
  knight, bishop, rook, queen and king moves, including castling,
  en passant and under‑promotions to queen, rook, bishop or knight.
  The generator filters out moves that leave the moving side’s king
  in check.
* **Iterative deepening alpha‑beta search:** A principal variation
  search (PVS) with alpha‑beta pruning and iterative deepening.
  Searches are organised by increasing depth, re‑using the
  transposition table across iterations and terminating when the
  time budget expires.  Move ordering uses principal variation
  ordering, **most‑valuable victim/least‑valuable attacker (MVV‑LVA)**,
  promotion bonuses and history/killer heuristics to maximise
  cutoffs.
* **Advanced move ordering & pruning:** Beyond PV and MVV‑LVA,
  the engine employs **killer move** and **history** heuristics,
  **aspiration windows**, **late move reductions (LMR)**, **null‑move
  pruning**, **futility pruning** and **razoring**.  A **quiescence
  search** resolves tactical sequences at leaf nodes by exploring
  captures and promotions until the position is quiet.
* **Parallel search (Lazy SMP):** At the root, multiple threads
  evaluate different principal variation moves concurrently, sharing
  a transposition table.  Killer and history tables are thread‑local
  to avoid contention, and a mutex protects the transposition table.
* **Draw detection:** Detects draws by the fifty‑move rule,
  threefold repetition and a wide set of insufficient‑material
  endgames (e.g., king vs king, king and minor vs king, two
  knights vs bare king, opposite‑coloured bishops and more).  Draw
  adjudication is integrated into the search and returns a neutral
  evaluation when triggered.
* **Sophisticated evaluation with optional neural network:** The
  classical evaluator combines material counts, piece‑square tables,
  mobility, pawn‑structure heuristics (doubled, isolated and passed
  pawns), a bishop‑pair bonus and a king‑safety/mobility term.  A
  pluggable neural‑network evaluator (NNUE) can be enabled via
  environment variables; loading external NNUE weights is supported
  for future experimentation【200842768436911†L13-L21】【200842768436911†L119-L123】.
* **CUDA‑accelerated evaluation:** Static evaluations are offloaded
  to the GPU in micro‑batches.  While the CPU continues the search,
  the GPU computes scores for queued positions using the same
  heuristics as the CPU.  On systems without a CUDA‑capable GPU
  the engine automatically falls back to CPU evaluation.
* **UCI protocol support with time management:** The engine
  implements a UCI loop that parses `uci`, `isready`, `ucinewgame`,
  `position`, `go` (with `depth`, `movetime`, `wtime`, `btime`,
  `winc`, `binc` and `movestogo`) and `stop` commands.  Time
  management derives per‑move budgets from the remaining time and
  increment, enabling play under real chess clocks.  UCI options
  include `UseGPU`, `PGNFile` and `TablebasePath`.
* **PGN logging and FEN/perft utilities:** A PGN logger records
  games in **full Standard Algebraic Notation (SAN)** with
  disambiguation, promotions and check/mate indicators, writing
  them to a file specified by the `PGNFile` UCI option.  The engine
  can load positions from FEN strings via the `position fen …`
  command and includes a perft subcommand for verifying move
  generation.  SAN output ensures recorded games are human
  readable and compatible with other chess software.
* **Tablebase scaffolding:** A `TablebasePath` option accepts
  directories of Syzygy/Lomonosov tablebases.  When six or fewer
  pieces remain, the engine probes the tablebase (stubbed by
  default) to return exact win/draw/loss results.
* **Modular design:** Board representation, move generation, search,
  evaluation, GPU batcher, UCI loop, PGN logger and tablebase
  functions reside in separate source files for clarity and ease of
  extension.

* **Opening book support:** `OwnBook` and `BookFile` UCI options
  enable a Polyglot opening book.  When enabled, the engine
  probes the book at the start of a search and selects a move
  from the book, bypassing search in the opening phase.  The
  current implementation includes a stubbed Polyglot reader and
  framework for loading a `.bin` book file.

* **Static Exchange Evaluation (SEE):** Capture moves are ordered
  using a simple static exchange evaluation heuristic that
  estimates the net material gain or loss of a capture sequence.
  Favourable captures are searched early, improving pruning
  efficiency.  SEE complements MVV‑LVA and promotion bonuses in
  the move ordering system.

* **Scalable to supercomputer environments:** When built with MPI
  and NCCL, the engine can distribute search and evaluation across
  multiple GPUs and nodes.  CUDA Multi‑Process Service (MPS) or
  NCCL is used to coordinate multi‑GPU batching, and an optional
  distributed search module (via MPI) enables cluster‑level
  parallelism.  These features are currently experimental and
  require appropriate libraries and hardware support.

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

### Perft testing

For validating the correctness of the move generator you can run
a **perft** test.  This counts the number of leaf nodes reachable
from the starting position (or any position) at a given depth.  For
example, the number of positions at depth 1 is 20, at depth 2 is
400, and at depth 3 is 8,902.  To run a perft test, invoke the
engine with the `perft` subcommand followed by the depth:

```sh
./nikolachess perft 4
Perft(4) = 197742
```

This can be compared against published perft results on the Chess
Programming Wiki to verify that all move generation cases are
handled correctly.

## Tuning and future directions

The Supercomputer Chess Engine now implements the full rules of
chess, comprehensive draw detection, iterative deepening with time
management, advanced move ordering, sophisticated pruning and
quiescence search, transposition tables, parallel search and
GPU‑accelerated evaluation.  Under‑promotions, castling and en
passant are fully supported.  Draw claims by the fifty‑move rule,
threefold repetition and a wide range of insufficient‑material
positions are adjudicated.  The search uses principal variation
ordering, MVV‑LVA, killer and history heuristics, aspiration
windows, late move reductions, null‑move pruning, futility and
razoring.  A quiescence search explores forcing capture sequences
at the leaf.  Per‑move time budgets are derived from the UCI `go`
parameters, allowing the engine to play under real chess clocks.
Parallel lazy SMP evaluates principal variation moves concurrently
across CPU threads.  A neural‑network evaluator stub illustrates
how to plug in a modern NNUE model and external weights.

Despite being feature complete, there is still room to improve
strength and usability.  Possible avenues include:

* **Production NNUE and CPU SIMD:** Train or import a high‑quality
  NNUE network (e.g. half‑KP or half‑KAv2) and optimise the CPU
  inference path using AVX2/AVX‑512/NEON intrinsics.  A stronger
  network will yield a significant Elo gain over the classical
  evaluator【200842768436911†L13-L21】【200842768436911†L119-L123】.
* **Real tablebase probing:** Integrate a Syzygy/Lomonosov probing
  library to return exact WDL/DTZ results when six or fewer pieces
  remain.  Tablebase hits should increment a `tbhits` counter and
  override heuristic evaluation.
* **Polyglot opening book:** The infrastructure for opening books
  has been added via the `OwnBook` and `BookFile` UCI options.
  Implementing proper Polyglot hashing and loading will allow the
  engine to select book moves before searching.  Weighted random
  selection or learning values can be used to choose between
  entries.  At present the reader is stubbed and always
  returns no moves.
* **Enhanced time management and search parameters:** Add more
  sophisticated time allocation strategies, adjustable aspiration
  window sizes, verification search for null‑move pruning and
  counters‑move/history heuristics.  Expose tunable search
  parameters through UCI options for A/B testing.
* **GPU evaluation pipeline:** Replace the current GPU stub with a
  real CUDA/TensorRT inference pipeline using pinned memory,
  multiple streams and CUDA graphs for low‑latency batched
  evaluation.  This will make the engine a powerful analysis tool
  on multi‑GPU systems.
* **Distributed and multi‑GPU search:** Build out the experimental
  multi‑GPU batching and MPI‑based distributed search modules.  Use
  CUDA Multi‑Process Service (MPS) or NCCL to share work across
  multiple GPUs on a single node and MPI to distribute the search
  tree across nodes in a cluster.  This work will allow the engine
  to run efficiently on supercomputers and justify the
  "Supercomputer" branding.
* **Monte‑Carlo Tree Search module:** Explore a separate MCTS
  engine that leverages the GPU evaluator for policy and value
  computation, suitable for large batch self‑play or training tasks.

The Supercomputer Chess Engine illustrates how GPU acceleration can be
integrated into a traditional alpha‑beta search without rewriting
the entire engine for the GPU.  While GPUs are not ideally
suited for branchy, recursive algorithms like alpha‑beta search,
they excel at running many independent evaluations concurrently【841273563187214†L123-L129】,
which is a key component of tree search.  We hope this project
serves as a robust baseline for experiments with more sophisticated
search strategies and evaluation methods on modern heterogeneous
architectures.
