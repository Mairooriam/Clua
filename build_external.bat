@echo off
REM Create build directory if it doesn't exist (relative to open62541)
if not exist deps\build (
    mkdir deps\build
)

REM Change to open62541 build directory
cd deps\build

REM Run CMake configure step with Release build type and Ninja generator
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ../clua/deps

REM Build open62541 in Release mode using Ninja
ninja

REM Go back to misc directory
cd ..\misc