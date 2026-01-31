# NikolaChess

**Supercomputer Chess Engine Written 100% in MIND**

Copyright (c) 2026 STARGA, Inc. All rights reserved.

---

## About

NikolaChess is a state-of-the-art chess engine built entirely in the **MIND programming language** - demonstrating the power of next-generation systems programming with native tensor operations and GPU acceleration.

### Why MIND?

MIND is a modern systems programming language developed by STARGA, Inc. that enables:

- **Native Tensor Operations**: First-class tensor types (`tensor<f16, (16, 8, 8)>`)
- **GPU Acceleration**: Seamless CUDA integration (`on(gpu0) { }`)
- **SIMD Intrinsics**: Hardware-optimized vector operations
- **Rust-Level Safety**: Memory safety without garbage collection
- **Python-Level Ergonomics**: Clean, readable syntax

```mind
// GPU-accelerated batch NNUE evaluation
fn batch_eval(positions: &[Board], results: &mut [i32]) {
    on(cuda::gpu0) {
        parallel for i in 0..positions.len() {
            let features = extract_halfka(&positions[i]);
            let acc = accumulator_forward(&features);
            results[i] = network_forward(&acc);
        }
    }
}
```

Learn more: [mindlang.dev](https://mindlang.dev)

---

## Features

### Engine Capabilities

- **NNUE Evaluation**: GPU-accelerated neural network with HalfKA architecture
- **CUDA Backend**: Massively parallel search via MIND Runtime
- **Syzygy Tablebases**: Perfect endgame play (8-man support, 16TB)
- **Advanced Search**: ABDADA parallel, LMR, null-move, futility pruning
- **Pondering**: Continuous analysis during opponent's time
- **Time Management**: Aggressive allocation optimized for online play

### Performance

| Metric | Value |
|--------|-------|
| Estimated Elo | ~3200+ |
| Search Speed | 50M+ nodes/sec (RTX 4090) |
| NNUE Batch Eval | 1M positions/sec (GPU) |
| Tablebase Coverage | 8-man endgames |

---

## Architecture

```
NikolaChess
├── MIND Core (src/)
│   ├── board.mind         - Board representation
│   ├── movegen64.mind     - Move generation
│   ├── search.mind        - Alpha-beta search
│   ├── abdada.mind        - Parallel search (ABDADA)
│   ├── lmr.mind           - Late move reductions
│   └── endgame.mind       - Endgame evaluation
│
├── MIND Runtime (protected)
│   ├── CPU Backend        - SIMD-optimized execution
│   └── CUDA Backend       - GPU acceleration
│
├── NNUE Module (src/)
│   ├── nnue.mind          - Neural network evaluation
│   ├── halfka.mind        - HalfKA feature extraction
│   ├── transformer.mind   - Attention-based heads
│   └── training.mind      - GPU training pipeline
│
├── Tablebase Support
│   ├── 7-man Syzygy       - 140GB perfect endgames
│   └── 8-man Syzygy       - 16TB extended coverage
│
└── Integration (src/)
    ├── uci.mind           - UCI protocol
    ├── lichess_bot.mind   - Lichess API integration
    └── opening_book.mind  - Opening book support
```

---

## System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | Windows 10 / Linux | Windows 11 / Ubuntu 22.04 |
| GPU | CUDA 11.0+ | NVIDIA RTX 4060+ |
| RAM | 8 GB | 32 GB |
| Storage | 2 GB | 200 GB (with 7-man TB) / 16 TB (8-man) |

---

## Downloads

### Pre-built Binaries

Download from [Releases](https://github.com/star-ga/NikolaChess/releases):

| Platform | CUDA | CPU-only |
|----------|------|----------|
| Windows | `nikola-cuda.exe` | `nikola-cpu.exe` |
| Linux | `nikola-cuda` | `nikola-cpu` |

### Building from Source

Requires MIND compiler and runtime (available at [mindlang.dev](https://mindlang.dev)):

```bash
mindc build --release
```

---

## Usage

### UCI Protocol

```
> ./nikola-cuda
NikolaChess v3.20 by STARGA, Inc.

> uci
id name NikolaChess
id author STARGA, Inc.
option name SyzygyPath type string default
option name Threads type spin default 1 min 1 max 256
option name Hash type spin default 256 min 16 max 65536
uciok

> position startpos
> go depth 20
info depth 1 score cp 20 nodes 21 pv e2e4
info depth 20 score cp 35 nodes 48291827 nps 51283947 pv e2e4 e7e5 g1f3 ...
bestmove e2e4 ponder e7e5
```

### Lichess Bot

```bash
export LICHESS_API_TOKEN="your_token"
mindc run src/lichess_bot.mind
```

---

## Source Code

The complete chess engine source code is available in this repository, demonstrating:

- **Pure MIND implementation** - No Rust, No Python, No C++
- **GPU acceleration patterns** - CUDA via MIND Runtime
- **Neural network evaluation** - NNUE with HalfKA features
- **Parallel search** - ABDADA algorithm in MIND

The MIND Runtime is a separately licensed component. Contact info@star.ga for runtime access.

---

## License

**Source Code**: Apache License 2.0 - See [LICENSE](LICENSE)

**MIND Runtime**: Proprietary - Licensed separately at [mindlang.dev](https://mindlang.dev)

---

## About STARGA

STARGA, Inc. develops next-generation programming languages and AI systems.
The MIND programming language represents our vision for the future of systems programming.

**Website**: [star.ga](https://star.ga)
**MIND Language**: [mindlang.dev](https://mindlang.dev)
**Contact**: info@star.ga

---

**NikolaChess** - Supercomputer Chess Engine. 100% MIND.
