# QZmini Calibration

The QZmini drives a syringe plunger with the printer's E axis. The slicer must know how far the
plunger physically moves per commanded E unit. Everything else is derived:

```
barrel_area_mm2                       = PI × (barrel_inner_diameter_mm / 2)²
effective_volume_mm3_per_e            = barrel_area_mm2 × plunger_mm_per_e_unit
equivalent_virtual_filament_diameter  = sqrt(4 × effective_volume_mm3_per_e / PI)
e_units_per_plunger_mm                = 1 / plunger_mm_per_e_unit
```

Open the dialog via **Calibration → QZmini Calibration**.

## Step 1 — E100 mechanical test (primary calibration)

1. Install and prepare the QZmini plunger; mark its initial position.
2. Send a controlled move, e.g. `M83` then `G1 E100 F300` (slow).
3. Measure the physical plunger travel in mm.
4. Enter commanded E and measured travel: `plunger_mm_per_e_unit = measured_mm / 100`.
5. Press *Apply to printer settings*, then save the printer preset (standard preset save).

## Step 2 — Printed line test (provisional seed only)

Rectangular approximation of a deposited line: `volume ≈ length × width × height`, then
`mm³/E = volume / commanded E`. The preliminary Quasizero test (20 mm × 4 mm × 3 mm at E0.9)
gives ≈266.7 mm³/E ⇒ ≈0.277 mm/E ⇒ virtual diameter ≈18.43 mm. **This is only a development
seed** — the cord cross-section is not rectangular, deposition may deviate from nominal, and
compressibility/leakage are not modelled.

## Step 3 — Flow ratio (separate, last)

Fine-tune flow with the **filament flow ratio** in the biomaterial preset. Flow ratio is a final
calibration multiplier; never use it to hide mechanical calibration errors, and never bake
mechanical errors into an arbitrary filament diameter.

## Known inconsistency (do not reconcile silently)

Nominal capacity 150 ml vs. 35 mm barrel × 140 mm reported stroke ≈ **134.7 ml**. The software
treats nominal capacity, usable capacity and stroke as independent, shows a warning, and keeps
all three editable (Printer settings → QZmini).
