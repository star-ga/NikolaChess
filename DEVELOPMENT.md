# NikolaChess - The Unbeatable Chess Engine

## 100% Pure MIND - Open Source

### What This Is

NikolaChess is a chess engine written entirely in the MIND language.
It's designed to NEVER LOSE - draw or win only (FORTRESS philosophy).

### Directory Structure

```
NikolaChess/
├── src/
│   ├── main.mind          # Entry point, UCI loop
│   ├── board.mind         # Board representation (bitboards)
│   ├── movegen64.mind     # Move generation
│   ├── move64.mind        # Move encoding/decoding
│   ├── search.mind        # Alpha-beta search with ABDADA
│   ├── abdada.mind        # Parallel search algorithm
│   ├── lmr.mind           # Late Move Reductions
│   ├── deep_eval.mind     # Neural network evaluation
│   ├── halfka.mind        # HalfKA NNUE features
│   ├── transformer.mind   # Transformer root reranking
│   ├── nnue.mind          # NNUE evaluation network
│   ├── endgame.mind       # Endgame knowledge
│   ├── endgame_db.mind    # Endgame database
│   ├── opening_book.mind  # Opening book
│   ├── fortress_conv.mind # Fortress detection
│   ├── tensor_board.mind  # Tensor board representation
│   ├── ocb_simd.mind      # Opposite-colored bishops SIMD
│   ├── wdl_tt.mind        # WDL transposition table
│   ├── uci.mind           # UCI protocol
│   ├── lichess_bot.mind   # Lichess bot integration
│   ├── training.mind      # Self-play training
│   └── lib.mind           # Module exports
├── Mind.toml              # Build configuration
├── Makefile               # Build targets
├── target/
│   └── release/
│       └── nikola-cpu     # Compiled executable
├── models/                # Neural network weights
├── book/                  # Opening book
└── data/                  # Tablebase data
```

### Build Commands

```bash
# Build release (CPU)
make release
# Or directly:
mindc build --release --target cpu

# Build for specific GPU
make cuda          # NVIDIA CUDA
make rocm          # AMD ROCm
make metal         # Apple Metal

# Run tests
make test

# Run benchmarks
make bench
```

### Key Features

- **HalfKA NNUE** - Efficient neural network evaluation
- **Transformer Reranking** - Root move reordering with attention
- **ABDADA Parallel Search** - Better than Lazy SMP
- **Fortress Detection** - Draw-focused endgame play
- **LMR Tuning** - Adaptive late move reductions
- **Syzygy Support** - 6-7 man tablebases

### Runtime Dependency

NikolaChess requires the MIND runtime library:
- `libmind_cpu_linux-x64.so` (Linux)
- `libmind_cpu_macos-arm64.dylib` (macOS)
- `mind_cpu_windows-x64.dll` (Windows)

Set `MIND_LIB_PATH` to the library location:
```bash
export MIND_LIB_PATH=/home/n/.nikolachess/lib
```

### UCI Commands

```
uci              - Enter UCI mode
isready          - Check if ready
position startpos moves e2e4 e7e5  - Set position
go depth 20      - Search to depth 20
go movetime 5000 - Search for 5 seconds
quit             - Exit
```

### Version History

- 3.20.0 - Current (Feb 2026)
- 3.19.0 - Previous stable

### License

Apache-2.0 (Open Source)

### Links

- Website: https://nikolachess.com
- GitHub: https://github.com/star-ga/NikolaChess
- MIND Language: https://mindlang.dev
