# NikolaChess Installation Script for Windows
# Copyright (c) 2025-2026 STARGA, Inc. All rights reserved.
#
# Usage: irm https://nikolachess.com/install.ps1 | iex
# Or:    .\install.ps1 [-Cuda] [-Version "X.X.X"]
#
# 100% Pure MIND - Supercomputer-Class Chess Engine

param(
    [switch]$Cuda,
    [switch]$DirectX,
    [switch]$Local,
    [string]$Version = "3.21.0"
)

$ErrorActionPreference = "Stop"

$InstallDir = "$env:USERPROFILE\.nikolachess"
$GitHubRelease = "https://github.com/star-ga/NikolaChess/releases/download/v$Version"

function Write-Banner {
    Write-Host @"
===========================================================

  _   _ _ _         _        _____ _
 | \ | (_) | _____ | | __ _ / ____| |
 |  \| | | |/ / _ \| |/ _` | |    | |__   ___  ___ ___
 | . ` | |   < (_) | | (_| | |    | '_ \ / _ \/ __/ __|
 | |\  | | |\ \___/| | (_| | |____| | | |  __/\__ \__ \
 |_| \_|_|_| \_\   |_|\__,_|\_____|_| |_|\___||___/___/

        Supercomputer-Class Chess Engine
                   Version $Version

===========================================================
"@ -ForegroundColor Cyan
}

function Write-Log {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[!] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[X] $Message" -ForegroundColor Red
    exit 1
}

function Get-Platform {
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    if ($arch -eq "X64") {
        return "windows-x64"
    } elseif ($arch -eq "Arm64") {
        return "windows-arm64"
    } else {
        Write-Error "Unsupported architecture: $arch"
    }
}

function Test-NvidiaGPU {
    try {
        $nvidia = Get-CimInstance -ClassName Win32_VideoController | Where-Object { $_.Name -like "*NVIDIA*" }
        if ($nvidia) {
            Write-Log "NVIDIA GPU detected: $($nvidia.Name)"
            return $true
        }
    } catch {}
    return $false
}

function New-Directories {
    Write-Log "Creating installation directories..."

    $dirs = @(
        "$InstallDir\bin",
        "$InstallDir\lib",
        "$InstallDir\models",
        "$InstallDir\book",
        "$InstallDir\syzygy"
    )

    foreach ($dir in $dirs) {
        if (-not (Test-Path $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
    }
}

function Install-Local {
    Write-Log "Installing from local build..."

    $LocalSrc = "C:\Users\$env:USERNAME\nikolachess-source"
    $Platform = Get-Platform

    # Copy runtime DLLs
    $runtimePath = "$LocalSrc\runtime-build"
    if (Test-Path $runtimePath) {
        Get-ChildItem "$runtimePath\*.dll" | ForEach-Object {
            Copy-Item $_.FullName "$InstallDir\lib\"
            Write-Log "Installed $($_.Name)"
        }
    }
}

function Install-Remote {
    Write-Log "Downloading NikolaChess v$Version..."

    $Platform = Get-Platform

    # Download CPU runtime
    $cpuUrl = "$GitHubRelease/mind_cpu_$Platform.dll"
    try {
        Invoke-WebRequest -Uri $cpuUrl -OutFile "$InstallDir\lib\mind_cpu_$Platform.dll"
        Write-Log "Downloaded CPU runtime"
    } catch {
        Write-Error "Failed to download runtime: $_"
    }

    # Download CUDA runtime if requested
    if ($Cuda -and (Test-NvidiaGPU)) {
        $cudaUrl = "$GitHubRelease/mind_cuda_$Platform.dll"
        try {
            Invoke-WebRequest -Uri $cudaUrl -OutFile "$InstallDir\lib\mind_cuda_$Platform.dll"
            Write-Log "Downloaded CUDA runtime"
        } catch {
            Write-Warning "CUDA runtime not available"
        }
    }

    # Download DirectX runtime if requested
    if ($DirectX) {
        $dxUrl = "$GitHubRelease/mind_directx_$Platform.dll"
        try {
            Invoke-WebRequest -Uri $dxUrl -OutFile "$InstallDir\lib\mind_directx_$Platform.dll"
            Write-Log "Downloaded DirectX runtime"
        } catch {
            Write-Warning "DirectX runtime not available"
        }
    }

    # Download NikolaChess (zip archive)
    $nikolaUrl = "$GitHubRelease/nikola-$Platform-v$Version.zip"
    try {
        Invoke-WebRequest -Uri $nikolaUrl -OutFile "$env:TEMP\nikola.zip"
        Expand-Archive -Path "$env:TEMP\nikola.zip" -DestinationPath "$InstallDir\bin" -Force
        Remove-Item "$env:TEMP\nikola.zip" -Force
        Write-Log "Downloaded NikolaChess"
    } catch {
        Write-Error "Failed to download NikolaChess: $_"
    }

    # Download mindc compiler (zip archive)
    $mindcUrl = "$GitHubRelease/mindc-$Platform.zip"
    try {
        Invoke-WebRequest -Uri $mindcUrl -OutFile "$env:TEMP\mindc.zip"
        Expand-Archive -Path "$env:TEMP\mindc.zip" -DestinationPath "$InstallDir\bin" -Force
        Remove-Item "$env:TEMP\mindc.zip" -Force
        Write-Log "Downloaded MIND compiler"
    } catch {
        Write-Warning "MIND compiler not available for download"
    }
}

function Set-EnvironmentPath {
    Write-Log "Setting up PATH..."

    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $binPath = "$InstallDir\bin"

    if ($userPath -notlike "*$binPath*") {
        [Environment]::SetEnvironmentVariable("Path", "$binPath;$userPath", "User")
        Write-Log "Added $binPath to PATH"
    }

    [Environment]::SetEnvironmentVariable("NIKOLACHESS_HOME", $InstallDir, "User")
    [Environment]::SetEnvironmentVariable("MIND_LIB_PATH", "$InstallDir\lib", "User")

    # Update current session
    $env:Path = "$binPath;$env:Path"
    $env:NIKOLACHESS_HOME = $InstallDir
    $env:MIND_LIB_PATH = "$InstallDir\lib"
}

function Show-Success {
    Write-Host @"

===========================================================
  Installation Complete!
===========================================================

  Installation directory: $InstallDir

  To start using NikolaChess:

    1. Open a new terminal (PowerShell or Command Prompt)

    2. Run the engine:
       nikola

    3. Or use with a chess GUI (Arena, CuteChess, etc.)
       Engine path: $InstallDir\bin\nikola.exe

  Documentation: https://nikolachess.com/docs
  Source: https://github.com/star-ga/NikolaChess

  Supercomputer-Class Chess Engine
  100% Pure MIND Language

"@ -ForegroundColor Green
}

# Main
Write-Banner

$Platform = Get-Platform
Write-Log "Detected platform: $Platform"

if (Test-NvidiaGPU) { $Cuda = $true }

New-Directories

if ($Local) {
    Install-Local
} else {
    Install-Remote
}

Set-EnvironmentPath
Show-Success
