# NNUE Model Documentation

Copyright (c) 2026 STARGA, Inc. All rights reserved.

## Overview

NikolaChess uses a HalfKA NNUE (Efficiently Updatable Neural Network) for position evaluation.

## Architecture

### HalfKA Features

- **Half**: Each perspective (white/black) has its own feature set
- **K**: King position is part of the feature index
- **A**: All pieces (pawns through queens) are tracked

Feature index = `king_square * 640 + piece_type * 64 + piece_square`

Total features per perspective: `64 * 10 * 64 = 40,960` + activity features = 45,056

### Network Topology

```
Layer 0: Feature Transformer
  Input:  45,056 binary features (per perspective)
  Output: 1024 activations (per perspective)
  Weights: 45,056 × 1024 int16

Layer 1: Hidden
  Input:  2048 (concatenated perspectives)
  Output: 16
  Weights: 2048 × 16 int8

Layer 2: Hidden
  Input:  16
  Output: 32
  Weights: 16 × 32 int8

Layer 3: Output
  Input:  32
  Output: 1 (centipawn evaluation)
  Weights: 32 × 1 int8
```

## Quantization

| Layer | Weight Type | Scale |
|-------|-------------|-------|
| Feature Transformer | int16 | 127 |
| Hidden 1 | int8 | 64 |
| Hidden 2 | int8 | 64 |
| Output | int8 | 127 |

## Accumulator Updates

Efficient incremental updates:
- Track which features changed
- Add/subtract feature vectors instead of full recomputation
- Copy from parent position when possible

```mind
fn update_accumulator(acc: &mut Accumulator, added: &[Feature], removed: &[Feature]) {
    for f in removed {
        acc.values -= weights[f];
    }
    for f in added {
        acc.values += weights[f];
    }
}
```

## SIMD Optimization

- AVX2: 256-bit operations (8 × int32)
- AVX-512: 512-bit operations (16 × int32)
- CUDA: Batch inference for leaf nodes

## Model File Format (.nknn)

```
Header (48 bytes):
  - Magic: 0x4E4B4E4E ("NKNN")
  - Version: u32
  - Architecture: 16 bytes ("HalfKA")
  - Feature size: u32
  - L1 size: u32
  - L2 size: u32
  - L3 size: u32
  - Quantization: 8 bytes

Weights (variable):
  - Feature transformer weights: int16[]
  - Feature transformer biases: int16[]
  - L1 weights: int8[]
  - L1 biases: int32[]
  - L2 weights: int8[]
  - L2 biases: int32[]
  - Output weights: int8[]
  - Output bias: int32
```

## Training

Training pipeline (`src/training.mind`):
1. Generate self-play games with random moves
2. Label positions with search results
3. Train on labeled positions
4. Quantize weights
5. Export to .nknn format

Training data format:
```
FEN, eval, result, ply, best_move
```

## Performance

| Metric | Value |
|--------|-------|
| Forward pass (CPU) | 10M/sec |
| Forward pass (GPU batch) | 1M/sec |
| Accumulator update | <100 cycles |
| Model size | ~40MB |
