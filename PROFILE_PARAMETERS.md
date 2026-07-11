# Profile Parameters

## Preset naming

ASCII hyphens are used in preset names (`QZ Sawdust Biomaterial - Cold Extrusion`,
`QZmini 3 mm - Smooth Biomaterial`) instead of em dashes for maximum file-system and
profile-validator compatibility.

## Material: QZ Sawdust Biomaterial - Cold Extrusion

| Parameter | Value | Note |
|---|---|---|
| Nozzle temperature (all) | 0 °C | cold extrusion; no thermal waits |
| Bed temperature (all plates) | 0 °C | |
| Part-cooling fan | 0 % | electronics/controller cooling untouched |
| Filament diameter | 18.43 mm | **provisional** equivalent virtual diameter from the line-test seed; recalibrate (QZMINI_CALIBRATION.md) |
| Density | 1.20 g/cm³ | provisional; used only for weight display |
| Flow ratio | 1.0 | final calibration multiplier, tune last |
| Pressure advance | disabled | not validated for paste |
| Retraction (filament override) | 0 | no ordinary retractions |
| Max volumetric speed | 300 mm³/s | provisional ceiling above the 240 mm³/s working point |
| filament_type | PLA | UI grouping only; no thermal semantics are used |

## Process: QZmini 3 mm - Smooth Biomaterial

| Parameter | Value | Note |
|---|---|---|
| Nozzle diameter (machine) | 3.0 mm | see note below |
| Layer height | 3.0 mm | initial layer 3.0 mm |
| Line width (all) | 4.0 mm | |
| Speeds | 20 mm/s (first layer 15) | travel 60 mm/s |
| Acceleration | 300 mm/s² (travel 500) | **provisional**, well below machine limits |
| Jerk | 5 mm/s | **provisional**, below machine limits |
| Walls / top / bottom | 2 / 2 / 2 | |
| Infill | 20 % rectilinear | provisional |
| Prime tower / supports / brim | off | skirt 1 loop as prime line |
| Retraction | 0 (machine level) | |

**Nozzle diameter discrepancy:** the project context lists the QZmini physical nozzle as 4.0 mm
while the specified process preset targets 3.0 mm. Both values are kept editable; the machine
variant ships as “3.0 nozzle” per the specification, and this is flagged as requiring
verification. Do not treat either value as validated.

## Printer keys (namespaced, printer scope)

`qzmini_enable`, `qzmini_barrel_inner_diameter` (35), `qzmini_nominal_syringe_capacity_ml` (150),
`qzmini_usable_syringe_capacity_ml` (120), `qzmini_usable_plunger_stroke_mm` (140, unverified),
`qzmini_plunger_mm_per_e_unit` (0.277, provisional seed), `qzmini_refill_enable`,
`qzmini_refill_threshold_ml` (120), `qzmini_park_x/y` (10/10), `qzmini_park_z_lift` (20),
`qzmini_plunger_reset_enable` (on), `qzmini_plunger_reset_feedrate` (300 mm/min),
`qzmini_prime_after_refill_enable` (off), `qzmini_prime_after_refill_ml` (1),
`qzmini_prime_feedrate` (120 mm/min), `qzmini_pause_strategy` (auto),
`qzmini_pause_custom_gcode`, `qzmini_refill_show_in_preview` (on).

Derived, computed at runtime and shown in the calibration dialog (never stored):
`e_units_per_plunger_mm`, `effective_volume_mm3_per_e`,
`equivalent_virtual_filament_diameter_mm`. `flow_ratio` maps to the standard
`filament_flow_ratio` of the biomaterial preset.

The 150 ml nominal vs 134.7 ml (35 mm × 140 mm) geometric contradiction is surfaced as a
warning wherever these values are used and is never silently reconciled.
