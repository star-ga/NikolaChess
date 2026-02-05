#!/bin/bash
# NikolaChess Setup Script
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# Downloads and installs MIND compiler and runtime libraries

set -e

MINDC_VERSION="0.1.9"
RUNTIME_VERSION="3.21.0"
MINDC_URL="https://github.com/star-ga/mind/releases/download/v${MINDC_VERSION}"
RUNTIME_URL="https://github.com/star-ga/NikolaChess/releases/download/v${RUNTIME_VERSION}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

info() { echo -e "${CYAN}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${WHITE}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Detect OS and architecture
detect_platform() {
    OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    ARCH=$(uname -m)

    case "$OS" in
        linux)
            case "$ARCH" in
                x86_64) PLATFORM="linux-x64" ;;
                aarch64) PLATFORM="linux-arm64" ;;
                *) error "Unsupported architecture: $ARCH" ;;
            esac
            ;;
        darwin)
            case "$ARCH" in
                x86_64) PLATFORM="macos-x64" ;;
                arm64) PLATFORM="macos-arm64" ;;
                *) error "Unsupported architecture: $ARCH" ;;
            esac
            ;;
        mingw*|msys*|cygwin*)
            PLATFORM="windows-x64"
            ;;
        *)
            error "Unsupported OS: $OS"
            ;;
    esac

    info "Detected platform: $PLATFORM"
}

# Detect available GPU backends
detect_gpu() {
    BACKENDS=""

    # Check for NVIDIA CUDA
    if command -v nvidia-smi &> /dev/null; then
        BACKENDS="$BACKENDS cuda"
        GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
        success "NVIDIA GPU detected: $GPU_NAME"
    fi

    # Check for AMD ROCm
    if command -v rocm-smi &> /dev/null || [ -d "/opt/rocm" ]; then
        BACKENDS="$BACKENDS rocm"
        success "AMD ROCm detected"
    fi

    # Check for Apple Metal (macOS)
    if [ "$OS" = "darwin" ]; then
        BACKENDS="$BACKENDS metal"
        success "Apple Metal available"
    fi

    # WebGPU is always available as fallback
    BACKENDS="$BACKENDS webgpu"

    if [ -z "$BACKENDS" ]; then
        warn "No GPU detected, will use CPU-only build"
        BACKENDS="cpu"
    fi

    info "Available backends:$BACKENDS"
}

# Download and install MIND compiler
install_mindc() {
    info "Checking for MIND compiler..."

    if command -v mindc &> /dev/null; then
        INSTALLED_VERSION=$(mindc --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
        if [ "$INSTALLED_VERSION" = "$MINDC_VERSION" ]; then
            success "MIND compiler v$MINDC_VERSION already installed"
            return 0
        else
            warn "MIND compiler v$INSTALLED_VERSION found, upgrading to v$MINDC_VERSION"
        fi
    fi

    info "Downloading MIND compiler v$MINDC_VERSION..."

    MINDC_ARCHIVE="mindc-${PLATFORM}.tar.gz"
    DOWNLOAD_URL="${MINDC_URL}/${MINDC_ARCHIVE}"

    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    trap "rm -rf $TEMP_DIR" EXIT

    # Download
    if command -v curl &> /dev/null; then
        curl -fsSL "$DOWNLOAD_URL" -o "$TEMP_DIR/$MINDC_ARCHIVE" || error "Download failed: $DOWNLOAD_URL"
    elif command -v wget &> /dev/null; then
        wget -q "$DOWNLOAD_URL" -O "$TEMP_DIR/$MINDC_ARCHIVE" || error "Download failed: $DOWNLOAD_URL"
    else
        error "Neither curl nor wget found. Please install one."
    fi

    # Extract
    info "Installing MIND compiler..."
    tar -xzf "$TEMP_DIR/$MINDC_ARCHIVE" -C "$TEMP_DIR"

    # Install to /usr/local/bin or ~/.local/bin
    if [ -w /usr/local/bin ]; then
        INSTALL_DIR="/usr/local/bin"
    else
        INSTALL_DIR="$HOME/.local/bin"
        mkdir -p "$INSTALL_DIR"
    fi

    cp "$TEMP_DIR/mindc" "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/mindc"

    # Add to PATH if needed
    if ! echo "$PATH" | grep -q "$INSTALL_DIR"; then
        warn "Add $INSTALL_DIR to your PATH"
        echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$HOME/.bashrc"
    fi

    success "MIND compiler installed to $INSTALL_DIR/mindc"
}

# Download runtime libraries for detected backends
install_runtime() {
    info "Installing MIND runtime libraries..."

    LIB_DIR="$(dirname "$0")/../lib"
    mkdir -p "$LIB_DIR"

    for backend in $BACKENDS; do
        case "$backend" in
            cuda)
                info "Downloading CUDA runtime..."
                RUNTIME_FILE="libmind_cuda_${PLATFORM}.so"
                ;;
            rocm)
                info "Downloading ROCm runtime..."
                RUNTIME_FILE="libmind_rocm_${PLATFORM}.so"
                ;;
            metal)
                info "Downloading Metal runtime..."
                RUNTIME_FILE="libmind_metal_${PLATFORM}.dylib"
                ;;
            webgpu)
                info "Downloading WebGPU runtime..."
                RUNTIME_FILE="libmind_webgpu_${PLATFORM}.so"
                [ "$OS" = "darwin" ] && RUNTIME_FILE="libmind_webgpu_${PLATFORM}.dylib"
                ;;
            cpu)
                info "Downloading CPU runtime..."
                RUNTIME_FILE="libmind_cpu_${PLATFORM}.so"
                [ "$OS" = "darwin" ] && RUNTIME_FILE="libmind_cpu_${PLATFORM}.dylib"
                ;;
        esac

        DOWNLOAD_URL="${RUNTIME_URL}/${RUNTIME_FILE}"

        if command -v curl &> /dev/null; then
            curl -fsSL "$DOWNLOAD_URL" -o "$LIB_DIR/$RUNTIME_FILE" 2>/dev/null || warn "Runtime not available: $backend"
        elif command -v wget &> /dev/null; then
            wget -q "$DOWNLOAD_URL" -O "$LIB_DIR/$RUNTIME_FILE" 2>/dev/null || warn "Runtime not available: $backend"
        fi

        if [ -f "$LIB_DIR/$RUNTIME_FILE" ]; then
            success "Installed: $RUNTIME_FILE"
        fi
    done
}

# Main
main() {
    echo -e "\n${BLUE}♔ NikolaChess${NC} - Supercomputer Chess Engine · Written 100% in Mind Lang\n"

    detect_platform
    detect_gpu
    install_mindc
    install_runtime

    echo ""
    success "Setup complete!"
    echo ""
    echo "Build commands:"
    echo "  make release     - Build optimized binary"
    echo "  make cuda        - Build with CUDA support"
    echo "  make metal       - Build with Metal support (macOS)"
    echo "  make rocm        - Build with ROCm support (AMD)"
    echo "  make help        - Show all build options"
    echo ""
}

main "$@"
