# Quasizero Slicer — Implementation Plan

Upstream base: OrcaSlicer/OrcaSlicer, tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
Branch: `quasizero-slicer-mvp`. License: GNU AGPLv3 (unchanged).

## Integration points (from repository inspection)

| Concern | Upstream mechanism | Quasizero integration |
|---|---|---|
| Printer/material/process presets | `resources/profiles/<Vendor>.json` + `<Vendor>/machine|filament|process` JSON inheritance | New `Quasizero` vendor library; no upstream profile edits |
| Config schema | `src/libslic3r/PrintConfig.cpp` (`this->add("key", type)`) | Namespaced `qzmini_*` keys appended in one clearly-marked block |
| G-code post transformation | Streaming passes in `GCode::do_export` (model: `GCode/FanMover.*`) | New `src/libslic3r/QuasiZero/` module, invoked as a final pass when `qzmini_refill_enable` is set |
| Pause markers in Preview | `GCodeProcessor::Reserved_Tags` — comment `; PAUSE_PRINTING` creates `CustomGCode::PausePrint` items (layer-slider + preview markers) | Refill sequences emit the reserved tag plus `; QZ_REFILL_*` block comments |
| Material usage | `GCodeProcessor` `PrintEstimatedStatistics::total_volumes_per_extruder` (mm³) | ml = mm³/1000; displayed via `SlicedInfo` panel (`src/slic3r/GUI/Plater.cpp`) |
| Machine G-code hooks | `machine_start_gcode`, `machine_end_gcode`, `machine_pause_gcode` per machine JSON | QZ cold-extrusion start/end G-code per printer variant |
| Localization | `_L()` gettext wrappers; catalogs under `localization/i18n` | EN source strings + ES translations for every new string |
| Tests | Catch2 (vendored `tests/catch2`), suites under `tests/` | New `tests/qzmini` suite + standalone harness runnable without the full dependency tree |
| Windows packaging | `build_release_vs2022.bat`, `.github/workflows/build_all.yml` (windows-latest job → portable zip + NSIS-style installer) | Fork CI workflow `quasizero_build.yml`, branding applied to existing pipeline |

## New module layout (pure STL core, GUI-free, unit-testable standalone)

```
src/libslic3r/QuasiZero/
  QzVolumetricModel.hpp/.cpp   # barrel geometry, E<->ml, virtual filament diameter, consistency checks
  QzGcodeStateMachine.hpp/.cpp # G90/G91, M82/M83, G92, XYZEF tracking, retraction-safe deposition accounting
  QzFirmwareAdapter.hpp/.cpp   # Marlin / Klipper / RRF / Prusa / Bambu pause-resume strategies
  QzRefillPlanner.hpp/.cpp     # event planning at safe boundaries, sequence emission, hard failure when unsafe
src/slic3r/GUI/QuasiZero/
  QzCalibrationDialog.*        # E100 / line-test calibration wizard
  QzRefillAssistPanel.*        # Refill Assist controls
```

Glue kept minimal: one call site in `GCode.cpp` (post pass), one in `Plater.cpp` (SlicedInfo ml lines),
Tab page registration for QZmini printer options, About-dialog attribution line.

## Refill correctness rules (enforced by state machine + tests)
- Deposition volume counts only net positive E after retract/unretract pairing.
- Insertion only at non-extruding boundaries (travel or layer change); never split G1 extrusions or G2/G3 arcs.
- Sequence: M400-equivalent → save state → relative Z lift → absolute XY park → M83 → slow plunger reset by consumed E → firmware pause → (resume) optional prime → G92 logical E restore → return XY → lower Z → restore modes/feedrate.
- Never physically re-advance plunger to pre-refill depth; only logical E is restored.
- If threshold+reserve exceeded with no safe boundary: abort export with error.

## Honest environment limits
This branch is authored in a sandbox that cannot compile the full wxWidgets/OpenCASCADE application
(2 cores / 3.8 GB RAM / <10 GB disk, 45 s process cap, Linux only). Therefore:
- The QuasiZero core modules compile and their tests run here (evidence in TEST_REPORT.md).
- The full Windows x64 build is produced by the adapted GitHub Actions workflow on the fork.
- Full-app integration code (GUI, GCode.cpp glue) is CI-compiled, not sandbox-compiled.
- No hardware validation is claimed anywhere. Labels: Unit tested / Offline G-code validated / Experimental / Slice-only.

## Phases
0. This plan (done) → 1. CI/build baseline → 2. Branding + Quasizero vendor presets →
3. Volumetric model + material summary + calibration UI → 4. Refill Assist engine + adapters + preview + tests →
5. Docs, samples, test report, push, CI Windows build.
