# Quasizero Slicer

Quasizero Slicer is a biomaterial-oriented fork of [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer)
for printers retrofitted with the **Quasizero QZmini** syringe-plunger cold-extrusion system.

**Quasizero Slicer is based on OrcaSlicer.** OrcaSlicer is based on PrusaSlicer and BambuStudio.
License: **GNU AGPLv3** (unchanged from upstream). See `LICENSES_AND_ATTRIBUTION.md`.

Upstream base: tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
Development branch: `quasizero-slicer-mvp`.

## What it adds

- **QZmini volumetric model** (`src/libslic3r/QuasiZero/`): converts commanded E units ↔ physical
  plunger travel ↔ millilitres of biomaterial. Contradictory hardware values are surfaced as
  warnings, never silently reconciled.
- **Quasizero preset library** (`resources/profiles/Quasizero/`): printers
  `QZmini @ Artillery Sidewinder X2` (primary target), `QZmini @ Bambu Lab A1 mini` and
  `QZmini @ Bambu Lab P1S` (both **Experimental / Slice-only**), material
  `Biocomposite Sawdust` and process `QZmini 3 mm - Smooth Biomaterial`.
  Selecting a QZmini printer auto-selects the compatible material and process.
- **Cold extrusion**: 0 °C nozzle/bed, no thermal waits, no purge tower/AMS/nozzle-wipe,
  part-cooling off, G28 homing preserved, machine limits inherited from the original printers.
- **Material requirement display**: the Preview legend shows a *QZmini Biomaterial* section with
  material required, model deposition, prime/purge, initial syringe fill, refills during print,
  usable volume per cycle, safety reserve and nominal capacity — all in ml.
- **QZmini Refill Assist**: state-aware G-code transformation inserting adapter-based pause
  sequences at safe boundaries (see `QZMINI_REFILL_ASSIST.md`). Refill events appear as pause
  markers in Preview.
- **QZmini Calibration** dialog (Calibration menu): E100 mechanical test, printed-line seed,
  derived values, one-click apply to the printer preset (`QZMINI_CALIBRATION.md`).
- **English and Spanish** strings for every new feature.

## Quick start

1. Build or download the Windows x64 build (`BUILD_WINDOWS.md`).
2. Select printer `QZmini @ Artillery Sidewinder X2 4.0 nozzle`; the QZ biomaterial and process
   presets load automatically.
3. Calibrate: *Calibration → QZmini Calibration* (E100 test), apply, save the printer preset.
4. Import an STL/3MF, slice, read the ml summary in Preview.
5. Enable *Auto-pause for refill* (Printer settings → QZmini), re-slice, export G-code.

## Validation status (truthful labels)

| Component | Status |
|---|---|
| QZmini volumetric model + refill engine | **Unit tested** (29 tests) |
| Marlin refill sequences (Artillery X2) | **Offline G-code validated** — *not* hardware validated |
| Artillery X2 cold start/end G-code | **Provisional** — not hardware validated |
| Bambu A1 mini / P1S profiles | **Experimental / Slice-only** |
| Klipper / RRF / Prusa adapters | **Unverified templates** |
| Windows x64 build | produced by CI (`.github/workflows/quasizero_build.yml`) |

See `TEST_REPORT.md` for evidence and `SUPPORTED_PRINTERS.md` for per-printer detail.

## Known limitations

- **Paste stability simulation (Stability view + card) is a Level 0/1 model, not hardware-validated.** Material values shipped in the presets are hypotheses; calibrate with the cylinder/wall collapse tests in `QZMINI_STABILITY.md` before trusting a prediction.

- No physical hardware validation has been performed yet on any printer.
- Bambu Lab network sending is untouched upstream functionality; QZmini Bambu profiles are
  slice/export-only. No authentication bypass or proprietary re-enablement is included or planned.
- G2/G3 arcs are never split for refill insertion (by design in the MVP).
- Automatic bed leveling (G29) is omitted from the QZ Artillery start G-code until probe
  clearance with the QZmini assembly is physically verified.
- The provisional plunger calibration (0.277 mm/E) comes from a single preliminary line test.
- macOS `Icon.icns` still contains upstream artwork (Windows is the deliverable platform).
