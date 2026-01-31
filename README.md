# NikolaChess

**Supercomputer Chess Engine Written 100% in MIND**

Copyright (c) 2026 STARGA, Inc. All rights reserved.

---

## About

NikolaChess is a state-of-the-art chess engine built entirely in the **MIND programming language** - demonstrating the power of next-generation systems programming with native tensor operations and GPU acceleration.

### Why MIND?

MIND (Machine Intelligence Native Design) is a modern systems programming language developed by STARGA, Inc. that enables:

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

**MIND Language**: [mindlang.dev](https://mindlang.dev) | [GitHub](https://github.com/star-ga/mind)

---

## Features

### Engine Capabilities

- **NNUE Evaluation**: GPU-accelerated neural network with HalfKA architecture
- **CUDA Backend**: Massively parallel search via MIND Runtime
- **Syzygy Tablebases**: Perfect endgame play (7-man standard, 8-man extended)
- **Advanced Search**: Alpha-beta, ABDADA parallel, LMR, null-move, futility pruning
- **Pondering**: Continuous analysis during opponent's time
- **Time Management**: Aggressive allocation optimized for online play

### Performance

| Metric | Value |
|--------|-------|
| Estimated Elo | ~3200+ |
| Search Speed | 50M+ nodes/sec (RTX 4090) |
| NNUE Batch Eval | 1M positions/sec (GPU) |
| Tablebase Coverage | 7-man (140GB) / 8-man (16TB) |

---

## Architecture

```
NikolaChess/
│
├── src/                          # Engine Source (22 files)
│   │
│   ├── Core
│   │   ├── main.mind             - Entry point, initialization
│   │   ├── lib.mind              - Module exports
│   │   ├── board.mind            - Board representation, Zobrist hashing
│   │   ├── move64.mind           - Move encoding (16-bit compact format)
│   │   └── movegen64.mind        - Legal move generation (magic bitboards)
│   │
│   ├── Search
│   │   ├── search.mind           - Alpha-beta with PVS, aspiration windows
│   │   ├── abdada.mind           - ABDADA parallel search algorithm
│   │   ├── lmr.mind              - Late Move Reductions (adaptive)
│   │   └── endgame.mind          - Endgame evaluation, tablebase probing
│   │
│   ├── Evaluation (NNUE)
│   │   ├── nnue.mind             - Neural network forward pass
│   │   ├── halfka.mind           - HalfKA feature extraction (45,056 features)
│   │   ├── transformer.mind      - Attention-based root move reranking
│   │   ├── tensor_board.mind     - Board tensor representations
│   │   └── training.mind         - GPU training pipeline
│   │
│   ├── Draw Specialization
│   │   ├── draw_eval.mind        - Draw probability network
│   │   ├── draw_db.mind          - Known draw position database
│   │   ├── draw_tt.mind          - Draw-specific transposition table
│   │   ├── fortress_conv.mind    - Fortress detection CNN
│   │   └── ocb_simd.mind         - Opposite-color bishop endgames (SIMD)
│   │
│   └── Integration
│       ├── uci.mind              - UCI protocol implementation
│       ├── lichess_bot.mind      - Lichess API bot integration
│       └── opening_book.mind     - Opening book (polyglot format)
│
├── tools/                        # Development Tools (12 files)
│   ├── benchmark.mind            - Performance benchmarking
│   ├── perft.mind                - Move generation verification
│   ├── elo_testing.mind          - Self-play & engine matches
│   ├── data_gen.mind             - Training data generation
│   ├── book_builder.mind         - Opening book creation
│   ├── pgn_tools.mind            - PGN parsing, filtering, analysis
│   ├── model_tools.mind          - NNUE model conversion/analysis
│   ├── analysis.mind             - Game analysis, blunder detection
│   ├── tune_lmr.mind             - LMR parameter optimization (SPSA)
│   ├── syzygy.mind               - Tablebase download & server
│   ├── datahub.mind              - Data management
│   └── cuda_bench.mind           - GPU vs CPU benchmarking
│
├── docs/                         # Documentation
│   ├── ARCHITECTURE.md           - Detailed architecture overview
│   ├── NNUE.md                   - Neural network documentation
│   └── SYZYGY.md                 - Tablebase integration guide
│
├── tests/                        # Test Suite
│   └── test_*.mind               - Unit tests
│
├── benchmark/                    # Benchmark Suite
│   └── *.mind                    - Performance tests
│
├── data/                         # Runtime Data
│   ├── models/                   - NNUE weights (.nknn)
│   ├── books/                    - Opening books
│   └── syzygy/                   - Tablebase files
│
├── releases/                     # Release Binaries
│   └── README.md                 - Download instructions
│
├── .github/workflows/            # CI/CD
│   └── build.yml                 - Multi-platform builds
│
├── Mind.toml                     - Build configuration
├── Makefile                      - Build commands
├── LICENSE                       - Apache 2.0
└── README.md                     - This file
```

---

## System Requirements

### Minimum Requirements

| Component | Linux | macOS | Windows |
|-----------|-------|-------|---------|
| OS | Ubuntu 20.04+ | macOS 12+ | Windows 10+ |
| CPU | x64 with AVX2 | Intel/Apple Silicon | x64 with AVX2 |
| RAM | 4 GB | 4 GB | 4 GB |
| Storage | 500 MB | 500 MB | 500 MB |

### Recommended Requirements

| Component | Linux | macOS | Windows |
|-----------|-------|-------|---------|
| OS | Ubuntu 22.04 | macOS 14+ | Windows 11 |
| CPU | x64 with AVX-512 | Apple M2+ | x64 with AVX-512 |
| GPU | NVIDIA RTX 4060+ | - | NVIDIA RTX 4060+ |
| RAM | 32 GB | 16 GB | 32 GB |
| Storage | 200 GB (7-man TB) | 200 GB | 200 GB |

### GPU Requirements (CUDA version)

- NVIDIA GPU with CUDA 11.0+ support
- NVIDIA Driver 450.0+
- 4GB+ GPU memory

---

## Downloads

### Pre-built Binaries

Download from [Releases](https://github.com/star-ga/NikolaChess/releases):

| Platform | CUDA | CPU-only |
|----------|------|----------|
| **Linux x64** | `nikola-cuda` | `nikola-cpu` |
| **macOS x64** | - | `nikola-cpu-x64` |
| **macOS ARM64** | - | `nikola-cpu-arm64` |
| **macOS Universal** | - | `nikola-cpu-universal` |
| **Windows x64** | `nikola-cuda.exe` | `nikola-cpu.exe` |

---

## Building from Source

Requires the MIND compiler (`mindc`) from [mindlang.dev](https://mindlang.dev) or [GitHub](https://github.com/star-ga/mind):

```bash
# Clone NikolaChess
git clone https://github.com/star-ga/NikolaChess.git
cd NikolaChess

# Build with MIND compiler
mindc build --release

# Or use Makefile
make release        # Build release binary
make cuda           # Build CUDA version
make cpu            # Build CPU-only version
```

### Build Targets

| Target | Command | Output |
|--------|---------|--------|
| Release (CUDA) | `mindc build --release` | `nikola-cuda` |
| CPU Only | `mindc build --release --target cpu` | `nikola-cpu` |
| Legacy CUDA | `mindc build --release --target cuda-legacy` | `nikola-cuda-legacy` |

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

### UCI Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| Hash | spin | 256 | Hash table size in MB |
| Threads | spin | 1 | Number of search threads |
| SyzygyPath | string | | Path to Syzygy tablebases |
| SyzygyProbeDepth | spin | 1 | Minimum depth to probe |
| SyzygyProbeLimit | spin | 7 | Max pieces for tablebase |
| Ponder | check | true | Think during opponent's time |
| MultiPV | spin | 1 | Number of principal variations |

### Lichess Bot

```bash
export LICHESS_API_TOKEN="your_token"
mindc run src/lichess_bot.mind
```

---

## Tools

NikolaChess includes comprehensive tooling for development and testing:

```bash
# Run performance benchmark
mindc run tools/benchmark.mind

# Verify move generation (perft)
mindc run tools/perft.mind -- suite

# Self-play Elo testing
mindc run tools/elo_testing.mind -- self 100 12

# Engine vs engine match
mindc run tools/elo_testing.mind -- match ./nikola ./stockfish 100 60000

# Analyze a game
mindc run tools/analysis.mind -- game mygame.pgn 16

# Download Syzygy tablebases
mindc run tools/syzygy.mind -- download 6 ./data/syzygy

# Build opening book from PGN
mindc run tools/book_builder.mind -- build games.pgn book.nkbook

# Tune LMR parameters
mindc run tools/tune_lmr.mind -- tune 500 100
```

---

## Documentation

- [Architecture Overview](docs/ARCHITECTURE.md) - System design and components
- [NNUE Documentation](docs/NNUE.md) - Neural network details
- [Syzygy Integration](docs/SYZYGY.md) - Tablebase setup

---

## Source Code

The complete chess engine source code is available in this repository:

- **22 source files** in `src/` - Complete engine implementation
- **12 tool files** in `tools/` - Development utilities
- **100% Pure MIND** - No Rust, No Python, No C++

This demonstrates:
- GPU acceleration patterns with CUDA via MIND
- Neural network evaluation with NNUE/HalfKA
- Parallel search with ABDADA algorithm
- Modern chess engine architecture

---

## License

**Source Code**: Apache License 2.0 - See [LICENSE](LICENSE)

---

## About STARGA

STARGA, Inc. develops next-generation programming languages and AI systems.

| | |
|---|---|
| **Website** | [star.ga](https://star.ga) |
| **MIND Language** | [mindlang.dev](https://mindlang.dev) \| [GitHub](https://github.com/star-ga/mind) |
| **Contact** | info@star.ga |

---

**NikolaChess** - Supercomputer Chess Engine. 100% MIND.
