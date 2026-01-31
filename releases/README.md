# NikolaChess Releases

Pre-built binaries are available on the [GitHub Releases](https://github.com/star-ga/NikolaChess/releases) page.

## Download

### Linux

| Binary | Description |
|--------|-------------|
| `nikola-cuda` | CUDA-accelerated (recommended for NVIDIA GPUs) |
| `nikola-cpu` | CPU-only with AVX2/AVX-512 |

### macOS

| Binary | Description |
|--------|-------------|
| `nikola-cpu-universal` | Universal binary (Intel + Apple Silicon) |
| `nikola-cpu-x64` | Intel Macs only |
| `nikola-cpu-arm64` | Apple Silicon only |

### Windows

| Binary | Description |
|--------|-------------|
| `nikola-cuda.exe` | CUDA-accelerated (recommended for NVIDIA GPUs) |
| `nikola-cpu.exe` | CPU-only with AVX2/AVX-512 |

## Requirements

### CUDA Version (Linux/Windows)
- NVIDIA GPU with CUDA 11.0+ support
- NVIDIA Driver 450.0+
- 4GB+ GPU memory recommended

### CPU Version (All Platforms)
- x64 processor with AVX2 support (Intel Haswell+, AMD Excavator+)
- Apple Silicon (M1/M2/M3) for macOS ARM64
- 4GB+ RAM recommended

## Installation

### Linux
```bash
# Download
wget https://github.com/star-ga/NikolaChess/releases/latest/download/nikola-cuda
chmod +x nikola-cuda

# Run
./nikola-cuda
```

### macOS
```bash
# Download universal binary
curl -LO https://github.com/star-ga/NikolaChess/releases/latest/download/nikola-cpu-universal
chmod +x nikola-cpu-universal

# Run
./nikola-cpu-universal
```

### Windows
```powershell
# Download from Releases page, then run:
.\nikola-cuda.exe
```

## Verification

After starting, verify the engine responds to UCI:
```
> uci
id name NikolaChess
id author STARGA, Inc.
uciok
```

## Chess GUIs

NikolaChess works with any UCI-compatible chess GUI:

- [Arena Chess GUI](http://www.playwitharena.de/) - Windows
- [Cute Chess](https://cutechess.com/) - Cross-platform
- [Lucas Chess](https://lucaschess.pythonanywhere.com/) - Windows/Linux
- [Banksia GUI](https://banksiagui.com/) - Cross-platform
- [Scid vs PC](https://scidvspc.sourceforge.net/) - Cross-platform
- [ChessX](https://chessx.sourceforge.io/) - Cross-platform

## Syzygy Tablebases

For optimal endgame play, download Syzygy tablebases:

```bash
# Using NikolaChess tool
mindc run tools/syzygy.mind -- download 6 ./syzygy

# Or download manually from:
# https://tablebase.lichess.ovh/
```

Then set the path in your chess GUI or via UCI:
```
setoption name SyzygyPath value /path/to/syzygy
```

## Building from Source

See the main [README](../README.md) for build instructions.

Requires MIND compiler from:
- https://mindlang.dev
- https://github.com/star-ga/mind
