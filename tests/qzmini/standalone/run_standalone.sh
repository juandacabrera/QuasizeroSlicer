#!/usr/bin/env bash
# Quasizero Slicer — build and run the QZmini core test suite without the
# full OrcaSlicer dependency tree. Requires only g++ (C++17).
set -euo pipefail
cd "$(dirname "$0")/../../.."
mkdir -p build-qztests
SRC="src/libslic3r/QuasiZero"
g++ -std=c++17 -O1 -Wall -Wextra -I src -I tests/qzmini \
    "$SRC/QzVolumetricModel.cpp" \
    $( [ -f "$SRC/QzGcodeStateMachine.cpp" ] && echo "$SRC/QzGcodeStateMachine.cpp" ) \
    $( [ -f "$SRC/QzFirmwareAdapter.cpp" ]  && echo "$SRC/QzFirmwareAdapter.cpp" ) \
    $( [ -f "$SRC/QzRefillPlanner.cpp" ]    && echo "$SRC/QzRefillPlanner.cpp" ) \
    tests/qzmini/test_*.cpp \
    tests/qzmini/standalone/main.cpp \
    -o build-qztests/qz_tests
./build-qztests/qz_tests
