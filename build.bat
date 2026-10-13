@echo off
REM ============================================================================
REM Cleaner Windows Server 2008 - Build Script (MinGW-w64 / GCC)
REM Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026 (www.nanoquity.com)
REM ============================================================================

echo [1/2] Compiling Win32 Resources (Icon, Manifest, VersionInfo)...
windres src/resource.rc -O coff -o src/resource.o
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Resource compilation failed!
    exit /b 1
)

echo [2/2] Compiling High-Performance Native x64 Binary (Windows Server 2008 R2 Target)...
g++ -std=c++17 -O3 -static -mwindows -municode ^
    -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 ^
    src/main.cpp src/scanner.cpp src/resource.o ^
    -o CleanerWinServer2008.exe ^
    -lcomctl32 -lshlwapi -lshell32 -lole32 -lgdi32 -lcomdlg32 -ladvapi32 -s

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] C++ compilation failed!
    exit /b 1
)

del /q src\resource.o 2>nul
echo [SUCCESS] Built CleanerWinServer2008.exe successfully!
