#!/bin/bash
# NikolaChess Installation Script
# Copyright (c) 2025-2026 STARGA, Inc. All rights reserved.
#
# Usage: curl -fsSL https://nikolachess.com/install.sh | bash
# Or:    ./install.sh [--local] [--cuda] [--metal] [--version X.X.X]
#
# 100% Pure MIND - Supercomputer-Class Chess Engine

set -e

VERSION="${NIKOLACHESS_VERSION:-3.21.0}"
INSTALL_DIR="${NIKOLACHESS_HOME:-$HOME/.nikolachess}"
GITHUB_RELEASE="https://github.com/star-ga/NikolaChess/releases/download/v${VERSION}"
LOCAL_MODE=false
CUDA_SUPPORT=false
METAL_SUPPORT=false
ROCM_SUPPORT=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

print_banner() {
    echo -e "${CYAN}"
    echo "==========================================================="
    echo ""
    echo "  _   _ _ _         _        _____ _                      "
    echo " | \\ | (_) | _____ | | __ _ / ____| |                     "
    echo " |  \\| | | |/ / _ \\| |/ _\` | |    | |__   ___  ___ ___   "
    echo " | . \` | |   < (_) | | (_| | |    | '_ \\ / _ \\/ __/ __|  "
    echo " | |\\  | | |\\ \\___/| | (_| | |____| | | |  __/\\__ \\__ \\ "
    echo " |_| \\_|_|_| \\_\\   |_|\\__,_|\\_____|_| |_|\\___||___/___/ "
    echo ""
    echo "        Supercomputer-Class Chess Engine"
    echo "                   Version ${VERSION}"
    echo ""
    echo "==========================================================="
    echo -e "${NC}"
}

log() {
    echo -e "${GREEN}[✓]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[!]${NC} $1"
}

error() {
    echo -e "${RED}[✗]${NC} $1" >&2
    exit 1
}

detect_platform() {
    OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    ARCH=$(uname -m)

    case "$OS" in
        linux)
            if [ "$ARCH" = "x86_64" ]; then
                PLATFORM="linux-x64"
            elif [ "$ARCH" = "aarch64" ]; then
                PLATFORM="linux-arm64"
            else
                error "Unsupported architecture: $ARCH"
            fi
            LIB_EXT="so"
            ;;
        darwin)
            if [ "$ARCH" = "x86_64" ]; then
                PLATFORM="macos-x64"
            elif [ "$ARCH" = "arm64" ]; then
                PLATFORM="macos-arm64"
            else
                error "Unsupported architecture: $ARCH"
            fi
            LIB_EXT="dylib"
            METAL_SUPPORT=true
            ;;
        *)
            error "Unsupported OS: $OS (use install.ps1 for Windows)"
            ;;
    esac

    log "Detected platform: ${PLATFORM}"
}

detect_gpu() {
    # NVIDIA CUDA
    if command -v nvidia-smi &>/dev/null; then
        GPU_INFO=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
        if [ -n "$GPU_INFO" ]; then
            log "NVIDIA GPU detected: ${GPU_INFO}"
            CUDA_SUPPORT=true
        fi
    fi

    # AMD ROCm
    if command -v rocm-smi &>/dev/null; then
        if rocm-smi --showproductname &>/dev/null; then
            log "AMD GPU detected (ROCm)"
            ROCM_SUPPORT=true
        fi
    fi

    # Apple Metal (already set in detect_platform for macOS)
    if [ "$METAL_SUPPORT" = true ]; then
        log "Apple Metal support enabled"
    fi
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --local)
                LOCAL_MODE=true
                shift
                ;;
            --cuda)
                CUDA_SUPPORT=true
                shift
                ;;
            --metal)
                METAL_SUPPORT=true
                shift
                ;;
            --rocm)
                ROCM_SUPPORT=true
                shift
                ;;
            --version)
                VERSION="$2"
                shift 2
                ;;
            --help|-h)
                echo "NikolaChess Installer"
                echo ""
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --local        Install from local build (development)"
                echo "  --cuda         Enable NVIDIA CUDA support"
                echo "  --metal        Enable Apple Metal support"
                echo "  --rocm         Enable AMD ROCm support"
                echo "  --version X.X  Install specific version"
                echo "  -h, --help     Show this help"
                exit 0
                ;;
            *)
                warn "Unknown option: $1"
                shift
                ;;
        esac
    done
}

create_directories() {
    log "Creating installation directories..."
    mkdir -p "${INSTALL_DIR}/bin"
    mkdir -p "${INSTALL_DIR}/lib"
    mkdir -p "${INSTALL_DIR}/models"
    mkdir -p "${INSTALL_DIR}/book"
    mkdir -p "${INSTALL_DIR}/syzygy"
}

install_local() {
    log "Installing from local build..."

    # Find local build artifacts
    LOCAL_SRC="/home/n/nikolachess-source"
    LOCAL_CHESS="/home/n/.nikolachess/NikolaChess"

    # Copy runtime libraries
    if [ -d "${LOCAL_SRC}/runtime-build" ]; then
        for lib in "${LOCAL_SRC}/runtime-build/"*mind_*_${PLATFORM}.${LIB_EXT}; do
            if [ -f "$lib" ]; then
                cp "$lib" "${INSTALL_DIR}/lib/"
                log "Installed $(basename $lib)"
            fi
        done
    fi

    # Copy mindc compiler
    if [ -f "/home/n/.nikolachess/bin/mindc" ]; then
        cp "/home/n/.nikolachess/bin/mindc" "${INSTALL_DIR}/bin/"
        log "Installed mindc compiler"
    fi

    # Build NikolaChess
    if [ -d "$LOCAL_CHESS" ]; then
        cd "$LOCAL_CHESS"
        if command -v "${INSTALL_DIR}/bin/mindc" &>/dev/null || command -v mindc &>/dev/null; then
            log "Building NikolaChess..."
            MIND_LIB_PATH="${INSTALL_DIR}/lib" mindc build --release --target cpu
            if [ -f "target/release/nikola-cpu" ]; then
                cp "target/release/nikola-cpu" "${INSTALL_DIR}/bin/nikola"
                log "Installed nikola"
            fi
        fi
    fi
}

install_remote() {
    log "Downloading NikolaChess v${VERSION}..."

    # Download runtime library
    RUNTIME_URL="${GITHUB_RELEASE}/libmind_cpu_${PLATFORM}.${LIB_EXT}"
    curl -fsSL "$RUNTIME_URL" -o "${INSTALL_DIR}/lib/libmind_cpu_${PLATFORM}.${LIB_EXT}" || \
        error "Failed to download runtime library"
    log "Downloaded CPU runtime"

    # Download CUDA runtime if requested
    if [ "$CUDA_SUPPORT" = true ]; then
        CUDA_URL="${GITHUB_RELEASE}/libmind_cuda_${PLATFORM}.${LIB_EXT}"
        curl -fsSL "$CUDA_URL" -o "${INSTALL_DIR}/lib/libmind_cuda_${PLATFORM}.${LIB_EXT}" 2>/dev/null && \
            log "Downloaded CUDA runtime" || \
            warn "CUDA runtime not available for this platform"
    fi

    # Download Metal runtime if requested (macOS only)
    if [ "$METAL_SUPPORT" = true ] && [ "$OS" = "darwin" ]; then
        METAL_URL="${GITHUB_RELEASE}/libmind_metal_${PLATFORM}.${LIB_EXT}"
        curl -fsSL "$METAL_URL" -o "${INSTALL_DIR}/lib/libmind_metal_${PLATFORM}.${LIB_EXT}" 2>/dev/null && \
            log "Downloaded Metal runtime" || \
            warn "Metal runtime not available"
    fi

    # Download NikolaChess binary (tarball)
    NIKOLA_TAR="${GITHUB_RELEASE}/nikola-${PLATFORM}-v${VERSION}.tar.gz"
    curl -fsSL "$NIKOLA_TAR" -o "/tmp/nikola-${PLATFORM}.tar.gz" && {
        tar -xzf "/tmp/nikola-${PLATFORM}.tar.gz" -C "${INSTALL_DIR}/bin/"
        chmod +x "${INSTALL_DIR}/bin/nikola"*
        rm -f "/tmp/nikola-${PLATFORM}.tar.gz"
        log "Downloaded NikolaChess"
    } || error "Failed to download NikolaChess"

    # Download mindc compiler (tarball)
    MINDC_TAR="${GITHUB_RELEASE}/mindc-${PLATFORM}.tar.gz"
    curl -fsSL "$MINDC_TAR" -o "/tmp/mindc-${PLATFORM}.tar.gz" 2>/dev/null && {
        tar -xzf "/tmp/mindc-${PLATFORM}.tar.gz" -C "${INSTALL_DIR}/bin/"
        chmod +x "${INSTALL_DIR}/bin/mindc"
        rm -f "/tmp/mindc-${PLATFORM}.tar.gz"
        log "Downloaded MIND compiler"
    } || warn "MIND compiler not available for download"
}

setup_path() {
    log "Setting up PATH..."

    SHELL_RC=""
    if [ -n "$BASH_VERSION" ]; then
        SHELL_RC="$HOME/.bashrc"
    elif [ -n "$ZSH_VERSION" ]; then
        SHELL_RC="$HOME/.zshrc"
    else
        SHELL_RC="$HOME/.profile"
    fi

    # Add to PATH if not already present
    if ! grep -q "NIKOLACHESS_HOME" "$SHELL_RC" 2>/dev/null; then
        cat >> "$SHELL_RC" << 'EOF'

# NikolaChess
export NIKOLACHESS_HOME="$HOME/.nikolachess"
export PATH="$NIKOLACHESS_HOME/bin:$PATH"
export MIND_LIB_PATH="$NIKOLACHESS_HOME/lib"
EOF
        log "Added NikolaChess to $SHELL_RC"
    fi

    # Export for current session
    export NIKOLACHESS_HOME="${INSTALL_DIR}"
    export PATH="${INSTALL_DIR}/bin:$PATH"
    export MIND_LIB_PATH="${INSTALL_DIR}/lib"
}

verify_installation() {
    log "Verifying installation..."

    # Check nikola binary
    if [ -x "${INSTALL_DIR}/bin/nikola" ]; then
        log "NikolaChess binary: OK"
    else
        warn "NikolaChess binary not found"
    fi

    # Check runtime library
    if ls "${INSTALL_DIR}/lib/"*mind_cpu*.${LIB_EXT} &>/dev/null; then
        log "CPU runtime library: OK"
    else
        warn "CPU runtime library not found"
    fi

    # Check CUDA if enabled
    if [ "$CUDA_SUPPORT" = true ]; then
        if ls "${INSTALL_DIR}/lib/"*mind_cuda*.${LIB_EXT} &>/dev/null; then
            log "CUDA runtime library: OK"
        else
            warn "CUDA runtime library not found"
        fi
    fi
}

print_success() {
    echo ""
    echo -e "${GREEN}==========================================================${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}==========================================================${NC}"
    echo ""
    echo "  Installation directory: ${INSTALL_DIR}"
    echo ""
    echo "  To start using NikolaChess:"
    echo ""
    echo "    1. Restart your terminal or run:"
    echo "       source ~/.bashrc  # or ~/.zshrc"
    echo ""
    echo "    2. Run the engine:"
    echo "       nikola"
    echo ""
    echo "    3. Or use with a chess GUI (Arena, CuteChess, etc.)"
    echo "       Engine path: ${INSTALL_DIR}/bin/nikola"
    echo ""
    echo "  Documentation: https://nikolachess.com/docs"
    echo "  Source: https://github.com/star-ga/NikolaChess"
    echo ""
    echo -e "${CYAN}  Supercomputer-Class Chess Engine${NC}"
    echo -e "${CYAN}  100% Pure MIND - No C Code${NC}"
    echo ""
}

main() {
    print_banner
    parse_args "$@"
    detect_platform
    detect_gpu
    create_directories

    if [ "$LOCAL_MODE" = true ]; then
        install_local
    else
        install_remote
    fi

    setup_path
    verify_installation
    print_success
}

main "$@"
