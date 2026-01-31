# NikolaChess Makefile
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# MIND Compiler: https://mindlang.dev | https://github.com/star-ga/mind

.PHONY: all build release debug clean test bench install help
.PHONY: cpu cuda cuda-ampere cuda-hopper rocm rocm-mi300 metal metal-m4 webgpu
.PHONY: all-backends setup check-mindc

# Default target
all: check-mindc release

#------------------------------------------------------------------------------
# Setup & Dependencies
#------------------------------------------------------------------------------

# Auto-download MIND compiler if not installed
check-mindc:
	@command -v mindc >/dev/null 2>&1 || { \
		echo "MIND compiler not found. Running setup..."; \
		chmod +x scripts/setup.sh && ./scripts/setup.sh; \
	}

# Full setup (compiler + runtime libraries)
setup:
	@echo "Setting up NikolaChess development environment..."
	@chmod +x scripts/setup.sh
	./scripts/setup.sh

#------------------------------------------------------------------------------
# Build Targets
#------------------------------------------------------------------------------

release:
	@echo "Building NikolaChess (release)..."
	mindc build --release

debug:
	@echo "Building NikolaChess (debug)..."
	mindc build

# CPU Targets
cpu:
	@echo "Building NikolaChess (CPU AVX-512)..."
	mindc build --release --target cpu

cpu-avx2:
	@echo "Building NikolaChess (CPU AVX2)..."
	mindc build --release --target cpu-avx2

# NVIDIA CUDA Targets
cuda:
	@echo "Building NikolaChess (CUDA - RTX 40 series)..."
	mindc build --release --target cuda

cuda-ampere:
	@echo "Building NikolaChess (CUDA - RTX 30/A100)..."
	mindc build --release --target cuda-ampere

cuda-hopper:
	@echo "Building NikolaChess (CUDA - H100)..."
	mindc build --release --target cuda-hopper

cuda-blackwell:
	@echo "Building NikolaChess (CUDA - Blackwell B200/B300)..."
	mindc build --release --target cuda-blackwell

cuda-rubin:
	@echo "Building NikolaChess (CUDA - Vera Rubin)..."
	mindc build --release --target cuda-rubin

# AMD ROCm Targets
rocm:
	@echo "Building NikolaChess (ROCm - RX 7900)..."
	mindc build --release --target rocm

rocm-mi300:
	@echo "Building NikolaChess (ROCm - MI300X)..."
	mindc build --release --target rocm-mi300

# Apple Metal Targets
metal:
	@echo "Building NikolaChess (Metal - Apple M3)..."
	mindc build --release --target metal

metal-m4:
	@echo "Building NikolaChess (Metal - Apple M4)..."
	mindc build --release --target metal-m4

# WebGPU Target (Browser/Cross-platform)
webgpu:
	@echo "Building NikolaChess (WebGPU)..."
	mindc build --release --target webgpu

# Build all backends
all-backends: cpu cuda rocm metal webgpu
	@echo "Built all GPU backends"

#------------------------------------------------------------------------------
# Testing
#------------------------------------------------------------------------------

test:
	@echo "Running perft test suite..."
	mindc run tools/perft.mind -- suite

bench:
	@echo "Running benchmark..."
	mindc run tools/benchmark.mind

elo-test:
	@echo "Running Elo self-play test..."
	mindc run tools/elo_testing.mind -- self 100 12

#------------------------------------------------------------------------------
# Tools
#------------------------------------------------------------------------------

perft:
	mindc run tools/perft.mind -- run startpos 6

analyze:
	@echo "Usage: make analyze FILE=game.pgn"
	mindc run tools/analysis.mind -- game $(FILE) 16

syzygy-6:
	@echo "Downloading 6-man Syzygy tablebases..."
	mindc run tools/syzygy.mind -- download 6 data/syzygy

syzygy-7:
	@echo "Downloading 7-man Syzygy tablebases..."
	mindc run tools/syzygy.mind -- download 7 data/syzygy

datagen:
	@echo "Generating training data..."
	mindc run tools/data_gen.mind -- generate 1000000

book:
	@echo "Usage: make book INPUT=games.pgn OUTPUT=book.nkbook"
	mindc run tools/book_builder.mind -- build $(INPUT) $(OUTPUT)

#------------------------------------------------------------------------------
# Utility
#------------------------------------------------------------------------------

clean:
	@echo "Cleaning build artifacts..."
	rm -rf target/
	rm -f nikola nikola-cpu nikola-cpu-avx2
	rm -f nikola-cuda nikola-cuda-ampere nikola-cuda-hopper
	rm -f nikola-rocm nikola-rocm-mi300
	rm -f nikola-metal nikola-metal-m4

install: release
	@echo "Installing to /usr/local/bin..."
	cp target/release/nikola /usr/local/bin/

help:
	@echo "NikolaChess Build System"
	@echo ""
	@echo "Build (CPU):"
	@echo "  make cpu          - CPU with AVX-512"
	@echo "  make cpu-avx2     - CPU with AVX2 (broader compatibility)"
	@echo ""
	@echo "Build (NVIDIA CUDA):"
	@echo "  make cuda           - RTX 40 series (Ada Lovelace)"
	@echo "  make cuda-ampere    - RTX 30 series / A100"
	@echo "  make cuda-hopper    - H100"
	@echo "  make cuda-blackwell - Blackwell B200/B300"
	@echo "  make cuda-rubin     - Vera Rubin (2026)"
	@echo ""
	@echo "Build (AMD ROCm):"
	@echo "  make rocm         - RX 7900 series (RDNA3)"
	@echo "  make rocm-mi300   - MI300X"
	@echo ""
	@echo "Build (Apple Metal):"
	@echo "  make metal        - Apple M3"
	@echo "  make metal-m4     - Apple M4"
	@echo ""
	@echo "Build (WebGPU):"
	@echo "  make webgpu       - Browser/cross-platform"
	@echo ""
	@echo "Build (All):"
	@echo "  make all-backends - Build all GPU backends"
	@echo "  make release      - Build release binary (default)"
	@echo "  make debug        - Build debug binary"
	@echo ""
	@echo "Test:"
	@echo "  make test         - Run perft test suite"
	@echo "  make bench        - Run performance benchmark"
	@echo "  make elo-test     - Run Elo self-play test"
	@echo ""
	@echo "Tools:"
	@echo "  make perft        - Run perft at depth 6"
	@echo "  make syzygy-6     - Download 6-man tablebases"
	@echo "  make syzygy-7     - Download 7-man tablebases"
	@echo "  make datagen      - Generate training data"
	@echo ""
	@echo "Setup:"
	@echo "  make setup        - Download MIND compiler and runtime libraries"
	@echo ""
	@echo "Utility:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install to /usr/local/bin"
	@echo ""
	@echo "MIND Compiler: https://mindlang.dev | https://github.com/star-ga/mind"
