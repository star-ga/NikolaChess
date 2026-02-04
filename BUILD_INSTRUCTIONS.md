# NikolaChess Build Instructions

## Full Build Matrix (22 Runtime Libraries)

| Platform | Backends | Count |
|----------|----------|-------|
| Linux x64 | cpu, cuda, rocm, oneapi, webgpu | 5 |
| Linux ARM64 | cpu, cuda, rocm, oneapi, webgpu | 5 |
| macOS ARM64 | cpu, metal, webgpu | 3 |
| macOS x64 | cpu, metal, webgpu | 3 |
| Windows x64 | cpu, cuda, directx, oneapi, rocm, webgpu | 6 |

**Total: 22 runtime libraries + 5 platform binaries = 27 artifacts**

---

## Option 1: Build on Windows with Claude Code

### Prerequisites on Windows

```powershell
# 1. Install Git
winget install Git.Git

# 2. Install Rust
winget install Rustlang.Rustup
rustup default stable

# 3. Install LLVM (for mindc)
winget install LLVM.LLVM

# 4. Clone the repos
git clone https://github.com/star-ga/NikolaChess.git
git clone https://github.com/star-ga/nikolachess-source.git  # Private - needs token
```

### Build Runtime Library (Windows)

```powershell
cd nikolachess-source

# Build the Rust runtime
cd runtime-rust
cargo build --release

# Output: target/release/mind_runtime.dll
```

### Protect the Library

```powershell
# Strip symbols (use LLVM strip)
llvm-strip --strip-all target/release/mind_runtime.dll

# Copy to output with backend names
$backends = @("cpu", "cuda", "directx", "oneapi", "rocm", "webgpu")
foreach ($backend in $backends) {
    Copy-Item "target/release/mind_runtime.dll" "output/mind_${backend}_windows-x64.dll"
}
```

### Push to Repo

```powershell
cd ../NikolaChess

# Copy built libs
Copy-Item ../nikolachess-source/output/*.dll lib/

# Commit and push
git add lib/
git commit -m "Add Windows x64 runtime libraries"
git push origin main
```

---

## Option 2: Launch GitHub Actions (Builds ALL Platforms)

### Step 1: Ensure Secrets are Set

In GitHub repo settings, ensure these secrets exist:
- `RUNTIME_TOKEN` - Personal access token with repo access to `nikolachess-source`

### Step 2: Trigger Workflow Manually

```bash
# From command line
gh workflow run release.yml

# Or push a tag
git tag v3.21.0
git push origin v3.21.0
```

### Step 3: Monitor Build

```bash
gh run list --workflow=release.yml
gh run watch
```

---

## Option 3: Full Instructions for Claude on Windows

Copy this prompt to Claude Code on Windows:

```
Build NikolaChess runtime libraries for Windows x64.

1. Clone nikolachess-source repo (private):
   git clone https://github.com/star-ga/nikolachess-source.git

2. Build the Rust runtime:
   cd nikolachess-source/runtime-rust
   cargo build --release

3. The output is: target/release/mind_runtime.dll

4. Strip the DLL for protection:
   llvm-strip --strip-all target/release/mind_runtime.dll

5. Create copies for each backend:
   - mind_cpu_windows-x64.dll
   - mind_cuda_windows-x64.dll
   - mind_directx_windows-x64.dll
   - mind_oneapi_windows-x64.dll
   - mind_rocm_windows-x64.dll
   - mind_webgpu_windows-x64.dll

6. Copy all DLLs to NikolaChess/lib/

7. Commit and push:
   git add lib/
   git commit -m "Add Windows x64 runtime libraries (protected)"
   git push origin main

8. Trigger GitHub Actions to build other platforms:
   gh workflow run release.yml
```

---

## CI Workflow Status

The GitHub Actions workflow (`release.yml`) will:

1. **Build protected runtime libraries** on:
   - Ubuntu (Linux x64)
   - Ubuntu ARM (Linux ARM64)
   - macOS 14 (ARM64)
   - macOS 15 (x64)
   - Windows (x64)

2. **Apply protection**:
   - Strip symbols
   - Remove metadata
   - Verify no internal symbols exposed

3. **Upload to release**:
   - All 22 runtime libraries
   - Platform binaries

---

## Current Status

| Platform | Built | Location |
|----------|-------|----------|
| Linux x64 | ✅ FULL | releases/v3.20.0/linux-x64/ |
| Linux ARM64 | ❌ | CI needed |
| macOS x64 | ❌ | CI needed |
| macOS ARM64 | ❌ | CI needed |
| Windows x64 | ❌ | Build locally or CI |

---

## Quick Start (Windows with Claude)

```powershell
# In Claude Code on Windows, run:

# 1. Setup
git clone https://github.com/star-ga/nikolachess-source.git
cd nikolachess-source/runtime-rust

# 2. Build
cargo build --release

# 3. Protect & copy
llvm-strip --strip-all target/release/mind_runtime.dll
mkdir -p ../output
foreach ($b in "cpu","cuda","directx","oneapi","rocm","webgpu") {
    cp target/release/mind_runtime.dll "../output/mind_${b}_windows-x64.dll"
}

# 4. Push
cd ../../NikolaChess
cp ../nikolachess-source/output/*.dll lib/
git add lib/ && git commit -m "Windows x64 libs" && git push

# 5. Trigger CI for other platforms
gh workflow run release.yml
```
