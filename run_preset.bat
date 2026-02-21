@echo off

REM Check if a preset name was provided
if "%1"=="" (
    echo Usage: run_preset.bat ^<preset-name^>
    echo Available: msvc-debug, msvc-release, mingw-debug, mingw-release, clang-debug, clang-release
    exit /b 1
)

REM Set up MSVC environment for MSVC or Clang presets
if "%1"=="msvc-debug" (
    echo Setting up MSVC environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if "%1"=="msvc-release" (
    echo Setting up MSVC environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if "%1"=="clang-debug" (
    echo Setting up MSVC environment for Clang...
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if "%1"=="clang-release" (
    echo Setting up MSVC environment for Clang...
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

echo Using preset: %1

REM Configure
cmake --preset "%1"
if errorlevel 1 exit /b 1

REM Build
if "%1"=="msvc-debug" cmake --build build/msvc-debug
if "%1"=="msvc-release" cmake --build build/msvc-release
if "%1"=="mingw-debug" cmake --build build/mingw-debug
if "%1"=="mingw-release" cmake --build build/mingw-release
if "%1"=="clang-debug" cmake --build build/clang-debug
if "%1"=="clang-release" cmake --build build/clang-release
