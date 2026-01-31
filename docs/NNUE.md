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

NikolaChess supports two model format versions with automatic detection.

### Version Selector

```mind
fn load_model(path: str) -> NNUEModel {
    let header = read_header(path);
    match header.version {
        1 => load_v1(path),  // Legacy format
        2 => load_v2(path),  // Current format with extended features
        _ => panic!("Unsupported model version: {}", header.version),
    }
}
```

### Format v1 (Legacy)

```
Header (32 bytes):
  - Magic: 0x4E4B4E4E ("NKNN")
  - Version: u32 (1)
  - Architecture: 16 bytes ("HalfKP")
  - Feature size: u32 (40960)
  - L1 size: u32 (256)

Weights:
  - Feature transformer: int16[40960 × 256]
  - L1: int8[512 × 32]
  - Output: int8[32 × 1]
```

### Format v2 (Current)

```
Header (64 bytes):
  - Magic: 0x4E4B4E4E ("NKNN")
  - Version: u32 (2)
  - Architecture: 16 bytes ("HalfKA")
  - Feature size: u32 (45056)
  - L1 size: u32 (1024)
  - L2 size: u32 (16)
  - L3 size: u32 (32)
  - Quantization: 8 bytes ("int16/8")
  - Flags: u32 (activity features, etc.)
  - Reserved: 12 bytes

Weights:
  - Feature transformer weights: int16[45056 × 1024 × 2]
  - Feature transformer biases: int16[1024 × 2]
  - L1 weights: int8[2048 × 16]
  - L1 biases: int32[16]
  - L2 weights: int8[16 × 32]
  - L2 biases: int32[32]
  - Output weights: int8[32 × 1]
  - Output bias: int32
```

### Version Differences

| Feature | v1 | v2 |
|---------|----|----|
| Architecture | HalfKP | HalfKA |
| Feature count | 40,960 | 45,056 |
| Activity features | No | Yes |
| L1 size | 256 | 1024 |
| Hidden layers | 1 | 2 |
| Quantization info | Implicit | Explicit |

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
