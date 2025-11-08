@echo off
REM Create build directory if it doesn't exist (relative to parent of misc)
if not exist build (
    mkdir build
)

REM Change to build directory
cd build

REM Run CMake configure step with Debug build type and Ninja generator

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ../clua

REM Build the project in Debug mode using Ninja
ninja

REM Go back to misc directory
cd ..\