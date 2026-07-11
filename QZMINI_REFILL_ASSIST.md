# QZmini Refill Assist

Automatic syringe-refill pauses. Controls: Printer settings → **QZmini** (`qzmini_*` keys).
Defaults: threshold 120 ml, nominal capacity 150 ml, usable capacity 120 ml.

## How events are planned

The exported G-code stream is transformed by a state-aware engine
(`src/libslic3r/QuasiZero/QzRefillPlanner.*`) that tracks G90/G91, M82/M83, G92 E resets,
XYZ/E/F state, retract/unretract pairs (never counted as deposition), layer transitions,
travel moves, arcs and existing pauses. When the consumed volume of the current cycle reaches
the threshold, the event is inserted at the next **safe boundary** — a completed travel move or
a layer transition. Extrusion segments and G2/G3 arcs are never split. The margin between the
threshold and the usable capacity is the safety reserve that allows moving the event to the next
safe boundary. **If the reserve is exhausted with no safe boundary, export fails with a clear
error instead of producing unsafe G-code.**

## Generated sequence (wrapped in identifiable comments)

```
; QZ_REFILL_BEGIN
; QZ_REFILL_INDEX=1
; QZ_REFILL_VOLUME_ML=120.05
; PAUSE_PRINTING              <- creates the Preview pause marker
M400                          <- wait for queued moves
; QZ_SAVED_X=.. Y=.. Z=.. E=.. F=.. XYZ_MODE=.. E_MODE=..
G91 / G1 Z<lift> / G90        <- relative Z lift, back to absolute
G0 X<park> Y<park>            <- park
M83                           <- relative E; never assume absolute E-<total> is correct
G1 E-<consumed> F<slow>       <- plunger reset by the material consumed this cycle
M0 (adapter-specific)         <- pause; user refills, then resumes
G1 E<prime> F<slow>           <- optional prime in the park area
G92 E<saved logical E>        <- restore ONLY the logical E coordinate
M82                           <- only if the print used absolute E
G0 X<saved> Y<saved>          <- return, Z still lifted
G1 Z<saved>                   <- lower to printing height
G1 F<saved>                   <- restore feedrate
; QZ_REFILL_END
```

**The plunger is never physically advanced back to its pre-refill depth** — the consumed
material has been replaced by new material; only the logical coordinate is restored.

## Firmware adapters (no universal macro)

| Family | Pause | Status |
|---|---|---|
| Marlin (Artillery X2) | `M0` (or machine profile pause G-code) | Offline G-code validated; hardware pending. Cold extrusion via `M302 S0` in start G-code. Validate that `M0` resumes correctly from your screen/host before production; `M600` is deliberately not assumed suitable. |
| Klipper | `PAUSE` macro | Unverified. A slicer command cannot universally override `min_extrude_temp`; set `min_extrude_temp: 0` in the extruder section of `printer.cfg` and define PAUSE/RESUME macros (example below). |
| RepRapFirmware | `M226` | Unverified template |
| Prusa/Buddy | `M601` | Unverified template |
| Bambu Lab | `M400 U1` | EXPERIMENTAL / SLICE-ONLY; cold-extrusion acceptance by stock firmware unverified |

Strategy override: `qzmini_pause_strategy` = `auto` / `M0` / `M25` / `M600` / `custom`
(+ `qzmini_pause_custom_gcode`).

### Klipper example (`printer.cfg`)

```ini
[extruder]
min_extrude_temp: 0        # REQUIRED for QZmini cold extrusion — validate on your machine

[gcode_macro PAUSE]
rename_existing: BASE_PAUSE
gcode:
    BASE_PAUSE
[gcode_macro RESUME]
rename_existing: BASE_RESUME
gcode:
    BASE_RESUME
```
The QZ sequence performs its own parking, plunger reset and return; keep the macros minimal.

## Preview

Each refill appears as a pause marker in the sliced Preview (layer slider + toolpath), via the
reserved `; PAUSE_PRINTING` tag. Disable with *Show refill events in Preview*.

## Verification

`python3 tests/qzmini/verify_gcode.py` re-checks exported samples with an independent parser:
no unintended heating, G28 preserved, no thermal waits, markers present, reset before pause,
no physical depth restore, E-state consistency, safe return order and volume agreement.
