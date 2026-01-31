# NikolaChess Makefile
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# MIND Compiler: https://mindlang.dev | https://github.com/star-ga/mind

.PHONY: all build release debug clean test bench cuda cpu install help

# Default target
all: release

#------------------------------------------------------------------------------
# Build Targets
#------------------------------------------------------------------------------

release:
	@echo "Building NikolaChess (release)..."
	mindc build --release

debug:
	@echo "Building NikolaChess (debug)..."
	mindc build

cuda:
	@echo "Building NikolaChess (CUDA)..."
	mindc build --release --target cuda

cpu:
	@echo "Building NikolaChess (CPU-only)..."
	mindc build --release --target cpu

cuda-legacy:
	@echo "Building NikolaChess (CUDA Legacy)..."
	mindc build --release --target cuda-legacy

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
	rm -f nikola nikola-cpu nikola-cuda nikola-cuda-legacy

install: release
	@echo "Installing to /usr/local/bin..."
	cp target/release/nikola /usr/local/bin/

help:
	@echo "NikolaChess Build System"
	@echo ""
	@echo "Build:"
	@echo "  make release      - Build release binary (default)"
	@echo "  make debug        - Build debug binary"
	@echo "  make cuda         - Build CUDA version"
	@echo "  make cpu          - Build CPU-only version"
	@echo "  make cuda-legacy  - Build CUDA legacy version"
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
	@echo "Utility:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install to /usr/local/bin"
	@echo ""
	@echo "MIND Compiler: https://mindlang.dev | https://github.com/star-ga/mind"
