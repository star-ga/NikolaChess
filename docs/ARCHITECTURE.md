# NikolaChess Architecture

Copyright (c) 2026 STARGA, Inc. All rights reserved.

## Overview

NikolaChess is a supercomputer-class chess engine written 100% in MIND.

## Core Components

### Board Representation (`src/board.mind`)
- 64-bit bitboard representation
- Piece-centric board arrays
- Incremental Zobrist hashing
- Legal move validation

### Move Generation (`src/movegen64.mind`, `src/move64.mind`)
- Magic bitboard sliding piece attacks
- Staged move generation
- Move ordering with history heuristics

### Search (`src/search.mind`)
- Alpha-beta with principal variation search
- Iterative deepening
- Aspiration windows
- Null move pruning
- Late move reductions (`src/lmr.mind`)
- Futility pruning
- Check extensions
- Singular extensions

### Parallel Search (`src/abdada.mind`)
- ABDADA algorithm for SMP
- Lock-free transposition table
- Work stealing
- GPU acceleration via MIND Runtime

### GPU MCTS Search (`src/search/mcts.mind`)
- Monte Carlo Tree Search with PUCT selection (AlphaZero-style)
- Policy+Value network with ResNet backbone
- Batched leaf evaluation on GPU
- Virtual loss for parallel expansion
- Dirichlet noise for root exploration

### Hybrid Search (`src/search/hybrid.mind`)
- SPTT (Superparallel Tree Traversal) algorithm
- Dynamic switching between alpha-beta and MCTS
- Position analyzer for algorithm selection:
  - High branching factor → MCTS
  - Tactical positions → Alpha-beta
  - Endgame → Alpha-beta with tablebases

### Search Improvements (`src/search/search_improvements.mind`)
- History-modulated Late Move Reductions
- ProbCut pruning with statistical cutoff prediction
- Enhanced killer move tracking
- Countermove heuristic with history bonuses

### Neural Network Evaluation

#### NNUE (`src/nnue.mind`, `src/halfka.mind`)
- HalfKA feature extraction (45,056 features)
- Efficiently updatable accumulator
- Quantized int8/int16 weights
- SIMD acceleration (AVX2/AVX-512)
- CUDA batched inference

#### Architecture
```
Input: HalfKA features (64 * 64 * 11 = 45,056)
  ↓
Feature Transformer (45,056 → 1024) × 2 perspectives
  ↓
ClippedReLU
  ↓
Hidden Layer 1 (2048 → 16)
  ↓
ClippedReLU
  ↓
Hidden Layer 2 (16 → 32)
  ↓
ClippedReLU
  ↓
Output (32 → 1)
```

#### Transformer Reranking (`src/transformer.mind`)
- Attention-based root move reordering
- 4-head self-attention
- Position encoding
- ~15 Elo gain at root

#### GPU-Batched NNUE (`src/gpu/batched_nnue.mind`)
- Batch positions from Lazy SMP threads for GPU inference
- 32-256 positions per batch
- Asynchronous CUDA streams
- 1M+ positions/sec throughput
- Multi-GPU distribution (up to 8 GPUs)

#### Evaluation Improvements (`src/eval/eval_improvements.mind`)
- CNN-based fortress detection (95%+ accuracy)
- Tapered phase evaluation with smooth transitions
- Dynamic contempt based on opponent strength
- King activity bonus in endgames

### Draw Specialization

#### Draw Evaluation (`src/draw_eval.mind`)
- Fortress detection network
- Draw probability estimation
- Opposite-color bishop handling (`src/ocb_simd.mind`)

#### Draw Database (`src/draw_db.mind`)
- Known draw positions
- Theoretical endgame draws
- Fortress patterns

### Endgame (`src/endgame.mind`)
- Syzygy tablebase probing (7-man, 8-man)
- Endgame-specific evaluation
- Mating patterns

### UCI Protocol (`src/api/uci_protocol.mind`)
- Standard UCI implementation
- MultiPV support
- Ponder support
- Hash/Threads options

### Benchmark Framework (`src/bench/`)
- SPRT (Sequential Probability Ratio Test) for Elo measurement
- A/B testing configuration with customizable parameters
- 30 curated benchmark positions:
  - Openings, middlegame tactics, complex positions
  - Rook, pawn, and complex endgames
- Automated statistical significance testing

### Lichess Integration (`src/lichess_bot.mind`)
- Lichess API v2
- Challenge handling
- Time management
- Rating-based behavior

## Data Flow

```
Position → Move Generation → Search Tree
              ↓
         NNUE Evaluation ← Feature Extraction
              ↓
         Alpha-Beta Pruning
              ↓
         Best Move Selection
              ↓
         UCI Output
```

## GPU Acceleration

```mind
on(cuda::gpu0) {
    parallel for i in 0..batch_size {
        features[i] = extract_halfka(&positions[i]);
        scores[i] = forward(&net, &features[i]);
    }
}
```

## Memory Layout

| Component | Size | Notes |
|-----------|------|-------|
| Transposition Table | 256MB-32GB | Configurable |
| NNUE Weights | ~40MB | Quantized |
| Accumulator Stack | 2MB/thread | Per-thread |
| Move Stack | 256KB/thread | Per-thread |

## Performance Targets

| Metric | Target | Hardware |
|--------|--------|----------|
| Search NPS | 50M+ | RTX 4090 |
| NNUE Eval | 1M/sec | RTX 4090 |
| Perft(6) | <1 sec | Single core |

## Build System

See `Mind.toml` for configuration:
- Release optimizations (LTO, codegen-units=1)
- Target-specific builds (CUDA, CPU, legacy)
- Feature flags (syzygy, lichess)
