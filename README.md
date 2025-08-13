# NikolaChess

NikolaChess is an experimental, research-friendly chess engine designed for scalable supercomputer environments. It targets high-performance hardware with optional GPU acceleration and distributed search while remaining accessible for chess engine enthusiasts.

## Current Features

- **Core Representation**: Bitboard-based board and legal move generator (castling, en passant, promotions).
- **Search**: Alpha-beta with iterative deepening, transposition table, move ordering heuristics (including countermoves).
- **Evaluation**: Hybrid classical and NNUE evaluator, including an asynchronous GPU pipeline.
- **GPU Optimizations**: Optional CUDA acceleration with dynamic batching, TensorRT integration and configurable streams (`--gpu-streams <N>`). CPU fallback included.
- **Distributed Search**: Experimental MPI prototype with optional NCCL; local work-stealing scheduler and scalable transposition table merging/sharing.
- **Protocol and Utility**: UCI support, time management, PGN logging, Polyglot opening book with generation utilities, weighted random book selection.
- **Endgames**: Full Syzygy tablebase support (WDL/DTZ via Fathom backend) with result caching.
- **Other**: Quiescence search, basic pruning, perft testing, research-oriented CMake build and executable `./nikolachess`.

## Build

You need a C++17 compiler and CMake ≥ 3.12. After cloning, initialize submodules:

```sh
git submodule update --init --recursive
```

### CPU-only

```sh
cmake -S . -B build
cmake --build build --config Release
```

### With CUDA (optional)

If a CUDA toolkit is found, GPU support is enabled automatically. Architectures can be supplied via `CUDA_ARCHITECTURES`.

### Distributed prototype (optional)

```sh
cmake -S . -B build -DNIKOLA_USE_MPI=ON         # MPI ranks (multi-node capable)
cmake -S . -B build -DNIKOLA_USE_MPI=ON -DNIKOLA_USE_NCCL=ON  # also enable NCCL
cmake --build build --config Release
```
This enables an MPI master/worker search with optional NCCL device broadcasts.

## Run

The executable is `./nikolachess`. Useful flags:

- `--gpu-streams <N>` – number of CUDA streams for the GPU evaluator (default 1, values < 1 clamp to 1).
- `--distributed` – run the experimental MPI/NCCL distributed search prototype. Launch with `mpirun`, e.g.:

```sh
mpirun -n 4 ./nikolachess --distributed
```

If you run without arguments, the engine initializes the standard starting position, prints CPU/GPU evaluations, then searches briefly and reports a move.

## UCI

Start UCI mode with:

```sh
./nikolachess uci
```

Recognized options include:

- `LimitStrength` (bool), `Strength` (int) – cap search strength/depth.
- `SyzygyPath` (string) – set endgame tablebase directory; probed via the Fathom backend.
- `UCI_ShowWDL` (bool), `UseGPU` (bool), `PGNFile` (string), `OwnBook` (bool), `BookFile` (string) – basic plumbing in place; some are stubs until corresponding modules are finalized.

## Tests

GoogleTest-based unit tests are part of the build (`nikola_tests`); additional focused tests exist for tablebase integration, distributed search and opening-book generation. Run:

```sh
ctest --output-on-failure
```

## Roadmap

The next development cycle focuses on polish, strength, and portability. Planned milestones:

1. **Advanced Search Enhancements**
   - *Search extensions*: extend depth for checks, recaptures and singular moves (limit +4 ply).
   - *Additional pruning*: integrate ProbCut, multi-cut, history leaf pruning and delta pruning in quiescence. Tune margins by depth.

2. **Evaluation Polish and NNUE Training**
   - *Enhanced classical evaluation*: add space, initiative and king safety terms with phase-tapered interpolation and GPU kernel updates.
   - *NNUE training pipeline*: Python tooling for self-play data generation and training larger networks, with UCI load option.

3. **Protocol and User Features**
   - *MultiPV and ponder*: report multiple principal variations and support pondering during the opponent’s time.
   - *Skill levels and analysis mode*: adjustable strength with depth reduction, plus infinite analysis with detailed output.
   - *Chess variants (FRC/960)*: support custom castling/backrank generation with UCI `variant` command.

4. **Hardware Portability and Optimizations**
   - *ROCm/HIP backend*: port CUDA kernels to HIP for AMD GPUs via conditional compilation.
   - *Vector intrinsics*: use AVX/SSE intrinsics for CPU bit operations and popcount to boost NPS.

5. **Testing, Tuning, and Polish**
   - *Automated testing framework*: integrate GoogleTest submodule and GitHub Actions CI for build/perft/self-play.
   - *Parameter tuning*: add SPSA scripts for automated tuning of search/eval parameters.
   - *Documentation and release*: Doxygen comments, expanded user documentation and a v1.0 release with binaries.

Progress is tracked on the `final-polish` branch with weekly milestones: Week 1 – search; Week 2 – evaluation; Week 3 – protocol/variants; Week 4 – portability/testing; Week 5 – tuning/release.
