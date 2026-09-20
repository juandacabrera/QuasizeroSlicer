#!/usr/bin/env bash
# Quasizero Slicer — build and run the QZmini core test suite without the
# full OrcaSlicer dependency tree. Requires only g++ (C++17).
set -euo pipefail
cd "$(dirname "$0")/../../.."

mkdir -p build-qztests
src_dir="src/libslic3r/QuasiZero"

sources=("$src_dir/QzVolumetricModel.cpp")
for name in QzGcodeStateMachine QzFirmwareAdapter QzRefillPlanner QzShortSegmentAnchor QzStabilityModel QzSkeleton; do
    if [ -f "$src_dir/$name.cpp" ]; then
        sources+=("$src_dir/$name.cpp")
    fi
done

tests=(tests/qzmini/test_*.cpp)

# the engine client (src/slic3r/Utils/QzEngine.cpp) has no GUI dependency: it is tested here
# against the real engine process when engine/ is checked out (QZ_SIM_TEST_EXE)
g++ -std=c++17 -O1 -Wall -Wextra -I src -I tests/qzmini -I deps_src \
    "${sources[@]}" \
    src/slic3r/Utils/QzEngine.cpp \
    "${tests[@]}" \
    tests/qzmini/standalone/main.cpp \
    -o build-qztests/qz_tests

if [ -f engine/run_tests.sh ]; then
    echo "== Quasizero simulation engine (engine/) =="
    bash engine/run_tests.sh
    echo
    echo "== Quasizero Slicer (LITE tree) + engine client against engine/build-tests/qz-sim =="
    QZ_SIM_TEST_EXE="$PWD/engine/build-tests/qz-sim" ./build-qztests/qz_tests
else
    echo "== Quasizero Slicer (LITE tree; engine/ not checked out: client tests without a process) =="
    ./build-qztests/qz_tests
fi
