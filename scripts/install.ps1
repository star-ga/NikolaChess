# NikolaChess Installer for Windows
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# Usage: irm https://nikolachess.com/install.ps1 | iex
#
# This script installs MIND compiler and NikolaChess runtime libraries

$ErrorActionPreference = "Stop"

$VERSION = "3.21.0"
$MINDC_VERSION = "0.1.7"
$INSTALL_DIR = "$env:USERPROFILE\.nikolachess"
$BIN_DIR = "$INSTALL_DIR\bin"
$LIB_DIR = "$INSTALL_DIR\lib"

$BASE_URL = "https://github.com/star-ga/mind/releases/download/v$MINDC_VERSION"
$RUNTIME_URL = "https://github.com/star-ga/NikolaChess/releases/download/v$VERSION"

function Write-Info { param($msg) Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

function Print-Banner {
    Write-Host ""
    Write-Host "  _   _ _ _         _        _____ _" -ForegroundColor Blue
    Write-Host " | \ | (_) | _____ | | __ _ / ____| |" -ForegroundColor Blue
    Write-Host " |  \| | | |/ / _ \| |/ _`` | |    | |__   ___  ___ ___" -ForegroundColor Blue
    Write-Host " | .`` | |   < (_) | | (_| | |    | '_ \ / _ \/ __/ __|" -ForegroundColor Blue
    Write-Host " | |\  | | |\ \___/| | (_| | |____| | | |  __/\__ \__ \" -ForegroundColor Blue
    Write-Host " |_| \_|_|_| \_\   |_|\__,_|\_____|_| |_|\___||___/___/" -ForegroundColor Blue
    Write-Host ""
    Write-Host "                 Installer v$VERSION" -ForegroundColor White
    Write-Host ""
}

function Detect-Platform {
    $arch = [System.Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")

    if ($arch -eq "AMD64") {
        $script:PLATFORM = "windows-x64"
    } elseif ($arch -eq "ARM64") {
        $script:PLATFORM = "windows-arm64"
    } else {
        Write-Err "Unsupported architecture: $arch"
    }

    Write-Info "Detected platform: $PLATFORM"
}

function Detect-GPU {
    $script:GPU_BACKENDS = @()

    # Check for NVIDIA GPU
    try {
        $nvidia = Get-WmiObject Win32_VideoController | Where-Object { $_.Name -like "*NVIDIA*" }
        if ($nvidia) {
            $script:GPU_BACKENDS += "cuda"
            Write-Success "NVIDIA GPU: $($nvidia.Name)"
        }
    } catch { }

    # Check for AMD GPU
    try {
        $amd = Get-WmiObject Win32_VideoController | Where-Object { $_.Name -like "*AMD*" -or $_.Name -like "*Radeon*" }
        if ($amd) {
            $script:GPU_BACKENDS += "rocm"
            Write-Success "AMD GPU: $($amd.Name)"
        }
    } catch { }

    # DirectX is always available on Windows
    $script:GPU_BACKENDS += "directx"
    Write-Success "DirectX available"

    if ($GPU_BACKENDS.Count -eq 1) {
        Write-Info "No dedicated GPU detected, installing CPU runtime"
    }
}

function Download-File {
    param($url, $dest)

    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
        return $true
    } catch {
        return $false
    }
}

function Install-Mindc {
    Write-Info "Installing MIND compiler..."

    New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

    $archive = "mindc-$PLATFORM.zip"
    $tempFile = "$env:TEMP\$archive"
    $url = "$BASE_URL/$archive"

    Write-Info "Downloading from $url"

    if (Download-File $url $tempFile) {
        Expand-Archive -Path $tempFile -DestinationPath $BIN_DIR -Force
        Remove-Item $tempFile -Force

        # Find and rename mindc executable if needed
        $mindc = Get-ChildItem -Path $BIN_DIR -Filter "mindc*" -Recurse | Select-Object -First 1
        if ($mindc -and $mindc.FullName -ne "$BIN_DIR\mindc.exe") {
            Move-Item $mindc.FullName "$BIN_DIR\mindc.exe" -Force
        }

        Write-Success "Installed mindc to $BIN_DIR\mindc.exe"
    } else {
        Write-Err "Failed to download mindc"
    }
}

function Install-Runtime {
    Write-Info "Installing runtime libraries..."

    New-Item -ItemType Directory -Force -Path $LIB_DIR | Out-Null

    # Install CPU runtime
    $cpuLib = "mind_cpu_$PLATFORM.dll"
    if (Download-File "$RUNTIME_URL/$cpuLib" "$LIB_DIR\$cpuLib") {
        Write-Success "Installed CPU runtime"
    } else {
        Write-Warn "CPU runtime not available for download"
    }

    # Install GPU runtimes
    foreach ($backend in $GPU_BACKENDS) {
        $libFile = "mind_${backend}_$PLATFORM.dll"
        if (Download-File "$RUNTIME_URL/$libFile" "$LIB_DIR\$libFile") {
            Write-Success "Installed $backend runtime"
        } else {
            Write-Warn "$backend runtime not yet available"
        }
    }
}

function Setup-Environment {
    Write-Info "Configuring environment..."

    # Add to user PATH
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")

    if ($userPath -notlike "*\.nikolachess\bin*") {
        $newPath = "$BIN_DIR;$userPath"
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Success "Added $BIN_DIR to PATH"
    } else {
        Write-Info "PATH already configured"
    }

    # Set MIND_LIB_PATH
    [Environment]::SetEnvironmentVariable("MIND_LIB_PATH", $LIB_DIR, "User")
    Write-Success "Set MIND_LIB_PATH=$LIB_DIR"

    # Update current session
    $env:Path = "$BIN_DIR;$env:Path"
    $env:MIND_LIB_PATH = $LIB_DIR
}

function Verify-Installation {
    Write-Host ""
    Write-Info "Verifying installation..."

    $mindcPath = "$BIN_DIR\mindc.exe"
    if (Test-Path $mindcPath) {
        try {
            $version = & $mindcPath --version 2>&1
            Write-Success "mindc: $version"
        } catch {
            Write-Success "mindc installed"
        }
    } else {
        Write-Warn "mindc not found"
    }

    $libs = Get-ChildItem -Path $LIB_DIR -Filter "*.dll" -ErrorAction SilentlyContinue
    if ($libs) {
        Write-Success "Runtime libraries: $($libs.Count) installed"
    } else {
        Write-Warn "Runtime libraries may be missing"
    }
}

function Print-Success {
    Write-Host ""
    Write-Host "===========================================================" -ForegroundColor Green
    Write-Host "  Installation Complete!" -ForegroundColor Green
    Write-Host "===========================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  To use NikolaChess, restart your terminal or run:"
    Write-Host ""
    Write-Host "    refreshenv" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Then build NikolaChess:"
    Write-Host ""
    Write-Host "    git clone https://github.com/star-ga/NikolaChess" -ForegroundColor Cyan
    Write-Host "    cd NikolaChess" -ForegroundColor Cyan
    Write-Host "    mindc build --release" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Or download pre-built binaries:"
    Write-Host ""
    Write-Host "    https://github.com/star-ga/NikolaChess/releases" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Documentation: https://nikolachess.com/docs"
    Write-Host "  Source: https://github.com/star-ga/NikolaChess"
    Write-Host ""
}

function Main {
    Print-Banner
    Detect-Platform
    Detect-GPU
    Install-Mindc
    Install-Runtime
    Setup-Environment
    Verify-Installation
    Print-Success
}

Main
