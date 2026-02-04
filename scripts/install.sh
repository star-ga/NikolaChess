#!/bin/bash
# NikolaChess Installer
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# Usage: curl -fsSL https://nikolachess.com/install.sh | bash
#
# This script installs MIND compiler and NikolaChess runtime libraries

set -e

VERSION="3.21.0"
MINDC_VERSION="0.1.7"
INSTALL_DIR="$HOME/.nikolachess"
BIN_DIR="$INSTALL_DIR/bin"
LIB_DIR="$INSTALL_DIR/lib"

# Download URLs
BASE_URL="https://github.com/star-ga/mind/releases/download/v${MINDC_VERSION}"
RUNTIME_URL="https://github.com/star-ga/NikolaChess/releases/download/v${VERSION}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
WHITE='\033[1;37m'
NC='\033[0m'

info()    { echo -e "${CYAN}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

print_banner() {
    echo -e "${BLUE}"
    echo "  _   _ _ _         _        _____ _                   "
    echo " | \\ | (_) | _____ | | __ _ / ____| |                  "
    echo " |  \\| | | |/ / _ \\| |/ _\` | |    | |__   ___  ___ ___ "
    echo " | . \` | |   < (_) | | (_| | |    | '_ \\ / _ \\/ __/ __|"
    echo " | |\\  | | |\\ \\___/| | (_| | |____| | | |  __/\\__ \\__ \\"
    echo " |_| \\_|_|_| \\_\\   |_|\\__,_|\\_____|_| |_|\\___||___/___/"
    echo ""
    echo -e "${WHITE}                 Installer v${VERSION}${NC}"
    echo ""
}

detect_platform() {
    OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    ARCH=$(uname -m)

    case "$OS" in
        linux)
            case "$ARCH" in
                x86_64)  PLATFORM="linux-x64" ;;
                aarch64) PLATFORM="linux-arm64" ;;
                *) error "Unsupported architecture: $ARCH" ;;
            esac
            LIB_EXT="so"
            ;;
        darwin)
            case "$ARCH" in
                x86_64) PLATFORM="macos-x64" ;;
                arm64)  PLATFORM="macos-arm64" ;;
                *) error "Unsupported architecture: $ARCH" ;;
            esac
            LIB_EXT="dylib"
            ;;
        mingw*|msys*|cygwin*)
            PLATFORM="windows-x64"
            LIB_EXT="dll"
            ;;
        *)
            error "Unsupported OS: $OS"
            ;;
    esac

    info "Detected platform: $PLATFORM"
}

detect_gpu() {
    GPU_BACKENDS=""

    # NVIDIA CUDA
    if command -v nvidia-smi &> /dev/null; then
        GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)
        GPU_BACKENDS="cuda"
        success "NVIDIA GPU: $GPU_NAME"
    fi

    # AMD ROCm
    if command -v rocm-smi &> /dev/null || [ -d "/opt/rocm" ]; then
        GPU_BACKENDS="$GPU_BACKENDS rocm"
        success "AMD ROCm available"
    fi

    # Apple Metal
    if [ "$OS" = "darwin" ]; then
        GPU_BACKENDS="$GPU_BACKENDS metal"
        success "Apple Metal available"
    fi

    if [ -z "$GPU_BACKENDS" ]; then
        info "No GPU detected, installing CPU runtime only"
    fi
}

download() {
    local url="$1"
    local dest="$2"

    if command -v curl &> /dev/null; then
        curl -fsSL "$url" -o "$dest" 2>/dev/null
    elif command -v wget &> /dev/null; then
        wget -q "$url" -O "$dest" 2>/dev/null
    else
        error "Neither curl nor wget found"
    fi
}

install_mindc() {
    info "Installing MIND compiler..."

    mkdir -p "$BIN_DIR"

    # Download mindc binary
    MINDC_FILE="mindc-${PLATFORM}"
    [ "$OS" = "mingw" ] || [ "$OS" = "msys" ] || [ "$OS" = "cygwin" ] && MINDC_FILE="${MINDC_FILE}.exe"

    if download "${BASE_URL}/${MINDC_FILE}" "$BIN_DIR/mindc"; then
        chmod +x "$BIN_DIR/mindc"
        success "Installed mindc to $BIN_DIR/mindc"
    else
        # Fallback: try tar.gz format
        ARCHIVE="mindc-${PLATFORM}.tar.gz"
        TEMP_DIR=$(mktemp -d)
        if download "${BASE_URL}/${ARCHIVE}" "$TEMP_DIR/$ARCHIVE"; then
            tar -xzf "$TEMP_DIR/$ARCHIVE" -C "$TEMP_DIR"
            cp "$TEMP_DIR/mindc" "$BIN_DIR/"
            chmod +x "$BIN_DIR/mindc"
            success "Installed mindc to $BIN_DIR/mindc"
        else
            error "Failed to download mindc"
        fi
        rm -rf "$TEMP_DIR"
    fi
}

install_mindc_build() {
    info "Installing mindc-build..."

    # Download mindc-build script
    SCRIPT_URL="https://raw.githubusercontent.com/star-ga/NikolaChess/main/scripts/mindc-build"

    if download "$SCRIPT_URL" "$BIN_DIR/mindc-build"; then
        chmod +x "$BIN_DIR/mindc-build"
        success "Installed mindc-build"
    else
        # Create minimal mindc-build
        cat > "$BIN_DIR/mindc-build" << 'SCRIPT'
#!/bin/bash
# mindc-build - MIND project build script
set -e
MINDC="${MINDC:-mindc}"
PROJECT_DIR="${1:-.}"
BUILD_TYPE="${2:-debug}"

if [ -f "$PROJECT_DIR/Mind.toml" ]; then
    $MINDC build --project "$PROJECT_DIR" ${BUILD_TYPE:+--$BUILD_TYPE}
else
    echo "Error: Mind.toml not found in $PROJECT_DIR"
    exit 1
fi
SCRIPT
        chmod +x "$BIN_DIR/mindc-build"
        success "Created mindc-build"
    fi
}

install_runtime() {
    info "Installing runtime libraries..."

    mkdir -p "$LIB_DIR"

    # Always install CPU runtime
    CPU_LIB="libmind_cpu_${PLATFORM}.${LIB_EXT}"
    if download "${RUNTIME_URL}/${CPU_LIB}" "$LIB_DIR/${CPU_LIB}"; then
        # Also create standard name symlink
        ln -sf "$CPU_LIB" "$LIB_DIR/libmind_cpu_${PLATFORM//-/_}.${LIB_EXT}" 2>/dev/null || true
        success "Installed CPU runtime"
    else
        warn "CPU runtime not available for download"
    fi

    # Install GPU runtimes if detected
    for backend in $GPU_BACKENDS; do
        case "$backend" in
            cuda)
                LIB_FILE="libmind_cuda_${PLATFORM}.${LIB_EXT}"
                ;;
            rocm)
                LIB_FILE="libmind_rocm_${PLATFORM}.${LIB_EXT}"
                ;;
            metal)
                LIB_FILE="libmind_metal_${PLATFORM}.${LIB_EXT}"
                ;;
        esac

        if download "${RUNTIME_URL}/${LIB_FILE}" "$LIB_DIR/${LIB_FILE}"; then
            success "Installed $backend runtime"
        else
            warn "$backend runtime not yet available"
        fi
    done
}

setup_environment() {
    info "Configuring environment..."

    # Detect shell config file
    SHELL_NAME=$(basename "$SHELL")
    case "$SHELL_NAME" in
        zsh)  SHELL_RC="$HOME/.zshrc" ;;
        bash) SHELL_RC="$HOME/.bashrc" ;;
        fish) SHELL_RC="$HOME/.config/fish/config.fish" ;;
        *)    SHELL_RC="$HOME/.profile" ;;
    esac

    # Check if already configured
    if grep -q "\.nikolachess" "$SHELL_RC" 2>/dev/null; then
        info "Environment already configured in $SHELL_RC"
        return
    fi

    # Add to shell config
    cat >> "$SHELL_RC" << 'CONFIG'

# NikolaChess / MIND Language
export PATH="$HOME/.nikolachess/bin:$PATH"
export MIND_LIB_PATH="$HOME/.nikolachess/lib"
export LD_LIBRARY_PATH="$HOME/.nikolachess/lib:$LD_LIBRARY_PATH"
CONFIG

    success "Added to $SHELL_RC"
}

verify_installation() {
    echo ""
    info "Verifying installation..."

    export PATH="$BIN_DIR:$PATH"
    export LD_LIBRARY_PATH="$LIB_DIR:$LD_LIBRARY_PATH"

    if [ -x "$BIN_DIR/mindc" ]; then
        VERSION_OUTPUT=$("$BIN_DIR/mindc" --version 2>/dev/null || echo "mindc installed")
        success "mindc: $VERSION_OUTPUT"
    else
        warn "mindc not executable"
    fi

    if [ -f "$LIB_DIR/libmind_cpu_${PLATFORM}.${LIB_EXT}" ] || [ -f "$LIB_DIR/libmind_cpu_linux-x64.so" ]; then
        success "Runtime libraries installed"
    else
        warn "Runtime libraries may be missing"
    fi
}

print_success() {
    echo ""
    echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "  To use NikolaChess, restart your shell or run:"
    echo ""
    echo -e "    ${CYAN}source $SHELL_RC${NC}"
    echo ""
    echo "  Then build NikolaChess:"
    echo ""
    echo -e "    ${CYAN}git clone https://github.com/star-ga/NikolaChess${NC}"
    echo -e "    ${CYAN}cd NikolaChess${NC}"
    echo -e "    ${CYAN}make release${NC}"
    echo ""
    echo "  Documentation: https://nikolachess.com/docs"
    echo "  Source: https://github.com/star-ga/NikolaChess"
    echo ""
}

main() {
    print_banner
    detect_platform
    detect_gpu
    install_mindc
    install_mindc_build
    install_runtime
    setup_environment
    verify_installation
    print_success
}

main "$@"
