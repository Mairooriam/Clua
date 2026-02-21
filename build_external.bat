@echo off
REM Create build directory if it doesn't exist (relative to open62541)
if not exist deps\build (
    mkdir deps\build
)

REM Change to open62541 build directory
cd deps\build

REM Run CMake configure step with Release build type and Ninja generator
cmake -G "Ninja"  -DCMAKE_BUILD_TYPE=Debug ..

REM Build open62541 in Release mode using Ninja
ninja

