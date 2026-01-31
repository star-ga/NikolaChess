# MIND Runtime Libraries

This directory contains proprietary MIND runtime libraries for GPU acceleration.

## Overview

The MIND Runtime provides hardware abstraction for:
- **CUDA** - NVIDIA GPUs (RTX 30/40 series, A100, H100, Blackwell)
- **ROCm** - AMD GPUs (RX 7900 series, MI300X)
- **Metal** - Apple Silicon (M1/M2/M3/M4)
- **WebGPU** - Cross-platform browser/native support
- **CPU** - AVX2/AVX-512 optimized fallback

## Library Files

| Backend | Linux | macOS | Windows |
|---------|-------|-------|---------|
| CUDA | `libmind_cuda_linux-x64.so` | - | `mind_cuda_windows-x64.dll` |
| ROCm | `libmind_rocm_linux-x64.so` | - | - |
| Metal | - | `libmind_metal_macos-arm64.dylib` | - |
| WebGPU | `libmind_webgpu_linux-x64.so` | `libmind_webgpu_macos-arm64.dylib` | `mind_webgpu_windows-x64.dll` |
| CPU | `libmind_cpu_linux-x64.so` | `libmind_cpu_macos-arm64.dylib` | `mind_cpu_windows-x64.dll` |

## Installation

Run the setup script to automatically download the appropriate runtime:

```bash
./scripts/setup.sh
```

Or download manually from: `https://runtime.mindlang.dev/v1.0.0/`

## Proprietary Notice

**MIND Runtime is proprietary software.**

Copyright (c) 2026 STARGA, Inc. All rights reserved.

The MIND Runtime libraries are distributed in compiled form only. Source code is not available.
These libraries are provided under a proprietary license that permits use with NikolaChess.

For licensing inquiries: info@star.ga

## Technical Details

### CUDA Backend
- Supports compute capability 8.0+ (Ampere, Ada Lovelace, Hopper, Blackwell)
- Requires CUDA Toolkit 12.0+
- Uses cuBLAS, cuDNN for tensor operations
- NVLink and multi-GPU support

### ROCm Backend
- Supports RDNA3, CDNA3 architectures
- Requires ROCm 6.0+
- Uses rocBLAS, MIOpen for acceleration

### Metal Backend
- Supports Apple M1/M2/M3/M4 chips
- Uses Metal Performance Shaders (MPS)
- Unified memory architecture optimization

### WebGPU Backend
- Cross-platform via Dawn/wgpu
- Browser and native support
- Fallback for unsupported hardware

## Linking

The MIND compiler automatically links against the appropriate runtime library based on the target:

```toml
# Mind.toml
[targets.cuda]
backend = "cuda"
compute = "sm_89"
```

Libraries are loaded at runtime from:
1. `./lib/` (project directory)
2. `~/.mind/lib/` (user directory)
3. `/usr/local/lib/mind/` (system directory)
