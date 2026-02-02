#!/bin/bash
# Build MIND Runtime Libraries for macOS
# Run this on a macOS machine with Xcode command line tools installed
# Supports both Apple Silicon (ARM64) and Intel (x86_64)

set -e

cd "$(dirname "$0")"

COMMON_FLAGS="-O2 -fPIC -fstack-protector-strong -fvisibility=hidden -fdata-sections -ffunction-sections"
LINK_FLAGS="-dynamiclib -ldl -lpthread -Wl,-dead_strip"

echo "Building MIND Runtime Libraries for macOS..."

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    ARCH_SUFFIX="macos-arm64"
    ARCH_FLAG="-arch arm64"
elif [ "$ARCH" = "x86_64" ]; then
    ARCH_SUFFIX="macos-x64"
    ARCH_FLAG="-arch x86_64"
else
    echo "Unknown architecture: $ARCH"
    exit 1
fi

echo "Detected architecture: $ARCH ($ARCH_SUFFIX)"

# Build each backend as .dylib
for backend in cpu cuda rocm webgpu metal; do
    echo "Building libmind_${backend}_${ARCH_SUFFIX}.dylib..."
    clang $ARCH_FLAG $COMMON_FLAGS $LINK_FLAGS \
        -o "libmind_${backend}_${ARCH_SUFFIX}.dylib" \
        runtime_cpu.c 2>&1

    # Strip symbols
    strip -x "libmind_${backend}_${ARCH_SUFFIX}.dylib"

    # Optional: Code sign (for notarization)
    # codesign --force --sign - "libmind_${backend}_${ARCH_SUFFIX}.dylib"
done

# Build base runtime
echo "Building libmind_runtime_${ARCH_SUFFIX}.dylib..."
clang $ARCH_FLAG $COMMON_FLAGS $LINK_FLAGS \
    -o "libmind_runtime_${ARCH_SUFFIX}.dylib" \
    runtime_cpu.c 2>&1
strip -x "libmind_runtime_${ARCH_SUFFIX}.dylib"

echo ""
echo "Build complete! Libraries:"
ls -la *.dylib

echo ""
echo "To build for both architectures (Universal Binary), run on each Mac and combine with lipo:"
echo "  lipo -create libmind_cpu_macos-arm64.dylib libmind_cpu_macos-x64.dylib -output libmind_cpu_macos-universal.dylib"
