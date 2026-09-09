# Test Report — Quasizero Slicer MVP

Upstream base: `OrcaSlicer/OrcaSlicer` tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
Branch: `quasizero-slicer-mvp`.

## Honest scope statement

This branch was authored in a constrained Linux sandbox (2 cores, 3.8 GB RAM, <10 GB disk,
45 s process cap, no MSVC) in which the full wxWidgets/OpenCASCADE application cannot be
compiled. Consequently:

- The **QuasiZero core modules** (volumetric model, G-code state machine, firmware adapters,
  refill planner) are compiled and their tests executed **in this environment** — evidence below.
- The **full application** (GUI glue, config schema, preview summary, presets, branding) is
  compiled by the fork CI workflow (`.github/workflows/quasizero_build.yml`) on `windows-latest`,
  which also produces the portable zip and NSIS installer. The manual validation workflow of
  section 15 of the specification (launch, slice, preview markers, per-printer checks,
  screenshots) must be executed on that build and recorded here before any release.
- **No hardware validation is claimed.** Labels used: Unit tested / Offline G-code validated /
  Experimental / Slice-only / Unverified.

## Unit + golden tests (executed, this environment)

Command: `bash tests/qzmini/standalone/run_standalone.sh` (also registered with ctest as
`qzmini_core_tests`). Result: **29 tests, 0 failures.**

Coverage against the specification: relative extrusion (M83), absolute extrusion (M82), G92 E
resets, positive extrusion, retraction/unretraction pairing (incl. partial unretract), multiple
layers, one refill, multiple refills, prime after refill (incl. budget carry-over), park and
return, preservation of XYZ modes / E modes / feedrate, threshold crossing near a layer
boundary, failure when no safe pause exists before usable capacity, files requiring zero / one /
two refills, firmware adapter pause commands, volume conservation, calibration equations,
E100 and line-test calibration, geometry-inconsistency surfacing, refill count formula.

```
PASS no_refill_below_threshold
PASS single_refill_above_threshold
PASS multiple_refills_large_model
PASS sequence_content_marlin
PASS modes_and_feedrate_preserved_across_refill
PASS absolute_e_mode_refill_restores_logical_e
PASS threshold_crossing_near_layer_boundary
PASS failure_when_no_safe_pause_before_capacity
PASS prime_included_in_next_cycle_budget
PASS firmware_adapters_pause_commands
PASS volume_conservation_through_transform
PASS relative_extrusion_m83
PASS absolute_extrusion_m82
PASS g92_e_reset_no_physical_motion
PASS retract_unretract_pair_not_counted
PASS partial_unretract
PASS xyz_modes_and_positions
PASS g28_home
PASS layer_change_comments_detected
PASS travel_vs_extruding_and_arcs
PASS pause_commands_detected
PASS comments_and_metadata_ignored_for_motion
PASS barrel_area_35mm
PASS equations_match_specification
PASS line_test_seed_values
PASS e100_calibration
PASS ml_e_roundtrip
PASS geometry_inconsistency_is_surfaced_not_reconciled
PASS refill_count_formula
== 29 tests, 0 failures ==
```

## Sample G-code verification (executed, this environment)

Fixtures generated through the real refill engine (`tests/qzmini/standalone/gen_samples.cpp`):
straight calibration line (0.24 ml), ~100 ml (below 120 threshold), ~130 ml (one refill),
~260 ml (two refills), in `samples/qzmini/`. Verified by an **independent Python parser**
(`tests/qzmini/verify_gcode.py`): no unintended heating, G28 preserved, no thermal waits,
refill comments present, plunger reset before pause, old plunger depth not physically restored,
logical E consistency, safe return order, volume agreement within ±1 ml (±0.05 ml for the line).

```
PASS samples/qzmini/qz_sample_calibration_line.gcode  (refills=0, deposited=0.24 ml)
PASS samples/qzmini/qz_sample_no_refill.gcode  (refills=0, deposited=100.00 ml)
PASS samples/qzmini/qz_sample_one_refill.gcode  (refills=1, deposited=131.00 ml)
PASS samples/qzmini/qz_sample_two_refills.gcode  (refills=2, deposited=262.00 ml)
```

## Stability model and stack simulation (executed, 2026-09-09)

Same runner, current tree: `bash tests/qzmini/standalone/run_standalone.sh` →
**56 tests, 0 failures** (29 MVP + short-segment anchoring / subdivision + 13 stability +
5 stack-simulation; the two newest check that the load/strength field shown at layer k
keeps the peak memory — it dips between load increments for a fast-curing paste with
pauses — and that the deformed view's per-strand colour equals that field on a ring). The stack-simulation tests check that a uniform ring reproduces the
layer model exactly, that the bulge band sits above the bed and the top settles, that an
irregular part (half ring on a full ring) loads the base unevenly, and that after a
collapse the earlier layers fold about the hinge while later strands fall on the pile
deterministically. An additional timing check on a synthetic 480 000-segment,
400-layer job: simulation build ≈ 1.0 s, one playback frame ≈ 20 ms (this environment,
2 cores). The GUI glue of the deformation view (`GCodeViewer`, `GLCanvas3D`, libvgcode
legend) is compiled by the Windows build, not here — **not yet built or visually checked**.

## Static/structural checks (executed)

- All Quasizero vendor JSON profiles parse; names/paths consistent; machine limits inherited
  from upstream Artillery/BBL profiles.
- `qz_refill` filter present in all 8 G-code pipeline compositions.
- 19 `qzmini_*` config keys registered in schema, GCodeConfig and printer preset key list.
- Spanish catalog extended (+74 entries) and gettext template updated.

## Not yet executed (pending CI/Windows build)

- Full upstream Catch2 suites (`ctest` on the built tree) — run by CI.
- Manual validation workflow (spec §15) incl. screenshots, A1 mini / P1S slice-only checks.
- Any hardware validation (Artillery X2 first target).

## Known limitations

See README_QUASIZERO.md; chief items: no hardware validation yet; provisional plunger
calibration seed (0.277 mm/E); G29 omitted pending probe-clearance check; Bambu profiles
slice-only; arcs never split; macOS icon not rebranded.
