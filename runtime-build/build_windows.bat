@echo off
REM Build MIND Runtime Libraries for Windows
REM Run this in a Visual Studio Developer Command Prompt

setlocal enabledelayedexpansion

echo Building MIND Runtime Libraries for Windows x64...

set COMMON_FLAGS=/O2 /GS /DYNAMICBASE /NXCOMPAT /W4 /sdl /DWIN32_LEAN_AND_MEAN
set LINK_FLAGS=/DLL /LTCG /OPT:REF /OPT:ICF

REM Build each backend as .dll
for %%b in (cpu cuda rocm webgpu directx) do (
    echo Building mind_%%b_windows-x64.dll...
    cl %COMMON_FLAGS% /LD runtime_cpu.c /Fe:mind_%%b_windows-x64.dll /link %LINK_FLAGS%
)

REM Build base runtime
echo Building mind_runtime_windows-x64.dll...
cl %COMMON_FLAGS% /LD runtime_cpu.c /Fe:mind_runtime_windows-x64.dll /link %LINK_FLAGS%

echo.
echo Build complete! Libraries:
dir /b *.dll

echo.
echo Note: Windows protection uses different APIs (IsDebuggerPresent, CheckRemoteDebuggerPresent, etc.)
