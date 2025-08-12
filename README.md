# NikolaChess

An experimental, research-friendly chess engine with:
- UCI protocol support and time management
- Optional CUDA acceleration (CPU fallback)
- Bitboard representation and a legal move generator (castling, en passant, promotions)
- Alpha-beta search with common heuristics (iterative deepening, transposition table, move ordering, etc.)
- Experimental distributed search prototype (MPI, optional NCCL) with
  local work-stealing fallback
- PGN logging, Polyglot opening book and Syzygy tablebase support

> Status: GPU evaluation now runs through an asynchronous NNUE pipeline and
> tablebases probe via the Fathom backend.  Distributed search features a
> basic work-stealing scheduler with a shared transposition table.

## Build

You need a C++17 compiler and CMake ≥ 3.12.

**CPU-only:**
```sh
cmake -S . -B build
cmake --build build --config Release
```

**With CUDA (optional):**
If a CUDA toolkit is found, GPU support is enabled automatically; otherwise the engine builds a CPU stub. You can also set architectures via `CUDA_ARCHITECTURES` if needed.

**Distributed prototype (optional):**
```sh
cmake -S . -B build -DNIKOLA_USE_MPI=ON         # MPI ranks (multi-node capable)
cmake -S . -B build -DNIKOLA_USE_MPI=ON -DNIKOLA_USE_NCCL=ON  # also enable NCCL
cmake --build build --config Release
```
This enables an MPI master/worker search with optional NCCL device broadcasts.

## Run

The executable is `./nikolachess`. Useful flags:

* `--gpu-streams <N>` — number of CUDA streams for the GPU evaluator (default 1). Values < 1 clamp to 1.
* `--distributed` — run the experimental MPI/NCCL distributed search prototype. Launch with `mpirun`, e.g.:
  ```sh
  mpirun -n 4 ./nikolachess --distributed
  ```

If you run without arguments, the engine initializes the standard starting position, prints CPU/GPU evaluations, then searches briefly and reports a move.

## UCI

Start UCI mode with:
```sh
./nikolachess uci
```

Supported/recognized options include:

* `LimitStrength` (bool), `Strength` (int) — cap search strength/depth.
* `SyzygyPath` (string) — set endgame tablebase directory; probed via the Fathom backend when available.
* `UCI_ShowWDL` (bool), `UseGPU` (bool), `PGNFile` (string), `OwnBook` (bool), `BookFile` (string) — basic plumbing in place; some are stubs until the corresponding modules are finalized.

## Tests

GoogleTest-based unit tests are part of the build (`nikola_tests`); additional focused tests exist for tablebase integration and options. Run:
```sh
ctest --output-on-failure
```

## HPC notes

Engine supports TT sharding and CPU affinity controls; `TT_SHARDS` defaults to 64 (tune per system). Pin threads if beneficial. See comments in the source and the HPC section.

## Roadmap (short)

* Expand TensorRT backend and batching heuristics for the NNUE evaluator.
* Add DTZ tablebase probing and richer book generation tools.
* Extend distributed search to cluster environments with true TT merging.
