# Quasizero Slicer — QZmini core test suite.
# Deliberately dependency-free (no Catch2, no libslic3r link): the same sources
# are also built by tests/qzmini/standalone/run_standalone.sh in environments
# without the OrcaSlicer dependency tree.
add_executable(qz_tests
    ../../src/libslic3r/QuasiZero/QzVolumetricModel.cpp
    ../../src/libslic3r/QuasiZero/QzGcodeStateMachine.cpp
    ../../src/libslic3r/QuasiZero/QzFirmwareAdapter.cpp
    ../../src/libslic3r/QuasiZero/QzRefillPlanner.cpp
    ../../src/libslic3r/QuasiZero/QzShortSegmentAnchor.cpp
    ../../src/libslic3r/QuasiZero/QzStabilityModel.cpp
    ../../src/libslic3r/QuasiZero/QzStackSim.cpp
    ../../src/libslic3r/QuasiZero/QzSkeleton.cpp
    test_qz_volumetric.cpp
    test_qz_state_machine.cpp
    test_qz_refill_planner.cpp
    test_qz_ssa.cpp
    test_qz_subdivider.cpp
    test_qz_stability.cpp
    test_qz_stacksim.cpp
    test_qz_skeleton.cpp
    standalone/main.cpp
)
target_include_directories(qz_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set_property(TARGET qz_tests PROPERTY CXX_STANDARD 17)
add_test(NAME qzmini_core_tests COMMAND qz_tests)
