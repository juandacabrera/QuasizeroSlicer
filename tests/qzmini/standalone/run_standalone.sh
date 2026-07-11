#!/usr/bin/env bash
# Quasizero Slicer — build and run the QZmini core test suite without the
# full OrcaSlicer dependency tree. Requires only g++ (C++17).
set -euo pipefail
cd "$(dirname "$0")/../../.."

mkdir -p build-qztests
src_dir="src/libslic3r/QuasiZero"

sources=("$src_dir/QzVolumetricModel.cpp")
for name in QzGcodeStateMachine QzFirmwareAdapter QzRefillPlanner; do
    if [ -f "$src_dir/$name.cpp" ]; then
        sources+=("$src_dir/$name.cpp")
    fi
done

tests=(tests/qzmini/test_*.cpp)

g++ -std=c++17 -O1 -Wall -Wextra -I src -I tests/qzmini \
    "${sources[@]}" \
    "${tests[@]}" \
    tests/qzmini/standalone/main.cpp \
    -o build-qztests/qz_tests

./build-qztests/qz_tests
