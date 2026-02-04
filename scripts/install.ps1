# NikolaChess Windows Installer
# Copyright (c) 2026 STARGA, Inc. All rights reserved.
#
# Usage: irm https://nikolachess.com/install.ps1 | iex
#    or: .\install.ps1

$ErrorActionPreference = "Stop"

$VERSION = "3.21.0"
$MINDC_VERSION = "0.1.7"
$INSTALL_DIR = "$env:USERPROFILE\.nikolachess"
$BIN_DIR = "$INSTALL_DIR\bin"
$LIB_DIR = "$INSTALL_DIR\lib"

$MINDC_URL = "https://github.com/star-ga/mind/releases/download/v$MINDC_VERSION"
$RUNTIME_URL = "https://github.com/star-ga/NikolaChess/releases/download/v$VERSION"

function Write-Info { param($msg) Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red; exit 1 }

function Show-Banner {
    Write-Host ""
    Write-Host "  _   _ _ _         _        _____ _                   " -ForegroundColor Blue
    Write-Host " | \ | (_) | _____ | | __ _ / ____| |                  " -ForegroundColor Blue
    Write-Host " |  \| | | |/ / _ \| |/ _`` | |    | |__   ___  ___ ___ " -ForegroundColor Blue
    Write-Host " | . `` | |   < (_) | | (_| | |    | '_ \ / _ \/ __/ __|" -ForegroundColor Blue
    Write-Host " | |\  | | |\ \___/| | (_| | |____| | | |  __/\__ \__ \" -ForegroundColor Blue
    Write-Host " |_| \_|_|_| \_\   |_|\__,_|\_____|_| |_|\___||___/___/" -ForegroundColor Blue
    Write-Host ""
    Write-Host "                 Installer v$VERSION" -ForegroundColor White
    Write-Host ""
}

function Detect-GPU {
    $script:GPU_BACKENDS = @()

    # Check for NVIDIA
    try {
        $nvidia = Get-WmiObject Win32_VideoController | Where-Object { $_.Name -match "NVIDIA" }
        if ($nvidia) {
            $script:GPU_BACKENDS += "cuda"
            Write-Success "NVIDIA GPU: $($nvidia.Name)"
        }
    } catch {}

    # Check for AMD
    try {
        $amd = Get-WmiObject Win32_VideoController | Where-Object { $_.Name -match "AMD|Radeon" }
        if ($amd) {
            $script:GPU_BACKENDS += "rocm"
            Write-Success "AMD GPU: $($amd.Name)"
        }
    } catch {}

    if ($script:GPU_BACKENDS.Count -eq 0) {
        Write-Info "No supported GPU detected, installing CPU runtime only"
    }
}

function Install-Mindc {
    Write-Info "Installing MIND compiler..."

    New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

    $archive = "mindc-windows-x64.zip"
    $downloadUrl = "$MINDC_URL/$archive"
    $tempFile = "$env:TEMP\$archive"

    try {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $tempFile -UseBasicParsing
        Expand-Archive -Path $tempFile -DestinationPath $BIN_DIR -Force
        Remove-Item $tempFile -Force
        Write-Success "Installed mindc to $BIN_DIR\mindc.exe"
    } catch {
        Write-Err "Failed to download mindc: $_"
    }
}

function Install-Runtime {
    Write-Info "Installing runtime libraries..."

    New-Item -ItemType Directory -Force -Path $LIB_DIR | Out-Null

    # Always install CPU runtime
    $cpuDll = "mind_cpu_windows-x64.dll"
    try {
        Invoke-WebRequest -Uri "$RUNTIME_URL/$cpuDll" -OutFile "$LIB_DIR\$cpuDll" -UseBasicParsing
        Write-Success "Installed CPU runtime"
    } catch {
        Write-Warn "CPU runtime not available"
    }

    # Install GPU runtimes
    foreach ($backend in $script:GPU_BACKENDS) {
        switch ($backend) {
            "cuda" { $dll = "mind_cuda_windows-x64.dll" }
            "rocm" { $dll = "mind_rocm_windows-x64.dll" }
        }

        try {
            Invoke-WebRequest -Uri "$RUNTIME_URL/$dll" -OutFile "$LIB_DIR\$dll" -UseBasicParsing
            Write-Success "Installed $backend runtime"
        } catch {
            Write-Warn "$backend runtime not available"
        }
    }
}

function Install-Nikola {
    Write-Info "Installing NikolaChess binary..."

    $package = "nikola-windows-x64-v$VERSION.zip"
    $tempFile = "$env:TEMP\$package"

    try {
        Invoke-WebRequest -Uri "$RUNTIME_URL/$package" -OutFile $tempFile -UseBasicParsing
        Expand-Archive -Path $tempFile -DestinationPath $BIN_DIR -Force
        Remove-Item $tempFile -Force
        Write-Success "Installed NikolaChess to $BIN_DIR"
    } catch {
        Write-Warn "Could not download NikolaChess package, downloading individual files..."
    }
}

function Setup-Environment {
    Write-Info "Configuring environment..."

    $userPath = [Environment]::GetEnvironmentVariable("PATH", "User")

    if ($userPath -notlike "*$BIN_DIR*") {
        [Environment]::SetEnvironmentVariable("PATH", "$BIN_DIR;$userPath", "User")
        Write-Success "Added $BIN_DIR to PATH"
    } else {
        Write-Info "PATH already configured"
    }

    # Set library path
    [Environment]::SetEnvironmentVariable("MIND_LIB_PATH", $LIB_DIR, "User")
    Write-Success "Set MIND_LIB_PATH=$LIB_DIR"
}

function Show-Success {
    Write-Host ""
    Write-Host "=======================================================" -ForegroundColor Green
    Write-Host "  Installation Complete!" -ForegroundColor Green
    Write-Host "=======================================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Restart your terminal, then run:" -ForegroundColor White
    Write-Host ""
    Write-Host "    nikola-windows-x64.exe" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Or use with a chess GUI (Arena, Cutechess, etc.)" -ForegroundColor White
    Write-Host ""
    Write-Host "  Documentation: https://nikolachess.com/docs" -ForegroundColor White
    Write-Host "  Source: https://github.com/star-ga/NikolaChess" -ForegroundColor White
    Write-Host ""
}

# Main
Show-Banner
Detect-GPU
Install-Mindc
Install-Runtime
Install-Nikola
Setup-Environment
Show-Success
