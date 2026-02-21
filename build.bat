@echo off
REM Create build directory if it doesn't exist (relative to parent of misc)
if not exist build (
    mkdir build
)

REM Change to build directory
cd build

REM Run CMake configure step with Debug build type and Ninja generator

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ../clua

REM Build the project in Debug mod2"e using MSBuild
cmake --build . --config Debug

REM Go back to misc directory
cd ..\