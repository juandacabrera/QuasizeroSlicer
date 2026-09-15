; Quasizero Slicer sample G-code (offline synthetic fixture)
; Printer: QZmini @ Artillery Sidewinder X2 3.0 nozzle (Marlin)
; Material: QZ Sawdust Biomaterial - Cold Extrusion
; STATUS: Offline G-code validated. NOT hardware validated.
; ===== QZmini cold-extrusion start (Quasizero Slicer, Marlin) — PROVISIONAL =====
G90 ; absolute XYZ
M83 ; relative E
M302 S0 ; allow cold extrusion (QZmini syringe drive)
M220 S100
M221 S100
G28 ; home all axes (preserved)
G1 Z10 F600
G1 X10 Y10 F3000
G92 E0
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;3
G1 Z3 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;6
G1 Z6 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;9
G1 Z9 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;12
G1 Z12 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;15
G1 Z15 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; QZ_REFILL_BEGIN
; QZ_REFILL_INDEX=1
; QZ_REFILL_VOLUME_ML=124.80
; QZ firmware family: Marlin — Offline G-code validated; hardware validation pending (Artillery Sidewinder X2)
; PAUSE_PRINTING
;TYPE:Custom
M400 ; wait for queued moves to complete
M104 S0 ; QZ: keep hot-end target at 0 (cold extrusion)
M302 S0 ; QZ: allow cold extrusion (Marlin; ignored by firmware without it)
; QZ_SAVED_X=36.000 Y=20.000 Z=15.000 E=468.28320 F=3000 XYZ_MODE=ABS E_MODE=REL(M83)
G91 ; relative for Z lift
G1 Z20.000 F600 ; lift
G90 ; absolute XY for park
G0 X10.000 Y10.000 F3000 ; park for refill
M83 ; relative E for plunger reset
G1 E-468.28320 F1800 ; retract plunger by material consumed this cycle
M0 ; pause, wait for user
; QZ: user refills or replaces the syringe, then resumes.
; QZ: the plunger is NOT advanced back to its previous depth; new material replaced the old.
M104 S0 ; QZ: cancel any pause-time preheat, keep target 0
M302 S0 ; QZ: allow cold extrusion on resume (avoid waiting to cool)
G1 E3.75227 F120 ; prime after refill
G92 E468.28320 ; normalize logical E (relative mode)
G0 X36.000 Y20.000 F3000 ; return above print
G1 Z15.000 F600 ; lower to printing height
G1 F3000 ; restore feedrate
; FEATURE: Internal solid infill
; QZ_REFILL_END
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;18
G1 Z18 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;21
G1 Z21 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;24
G1 Z24 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;27
G1 Z27 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;30
G1 Z30 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E19.5118 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E19.5118 F1200
G0 X28 Y20 F3000
; QZ_REFILL_BEGIN
; QZ_REFILL_INDEX=2
; QZ_REFILL_VOLUME_ML=120.60
; QZ firmware family: Marlin — Offline G-code validated; hardware validation pending (Artillery Sidewinder X2)
; PAUSE_PRINTING
;TYPE:Custom
M400 ; wait for queued moves to complete
M104 S0 ; QZ: keep hot-end target at 0 (cold extrusion)
M302 S0 ; QZ: allow cold extrusion (Marlin; ignored by firmware without it)
; QZ_SAVED_X=28.000 Y=20.000 Z=30.000 E=917.05460 F=3000 XYZ_MODE=ABS E_MODE=REL(M83)
G91 ; relative for Z lift
G1 Z20.000 F600 ; lift
G90 ; absolute XY for park
G0 X10.000 Y10.000 F3000 ; park for refill
M83 ; relative E for plunger reset
G1 E-452.52367 F1800 ; retract plunger by material consumed this cycle
M0 ; pause, wait for user
; QZ: user refills or replaces the syringe, then resumes.
; QZ: the plunger is NOT advanced back to its previous depth; new material replaced the old.
M104 S0 ; QZ: cancel any pause-time preheat, keep target 0
M302 S0 ; QZ: allow cold extrusion on resume (avoid waiting to cool)
G1 E3.75227 F120 ; prime after refill
G92 E917.05460 ; normalize logical E (relative mode)
G0 X28.000 Y20.000 F3000 ; return above print
G1 Z30.000 F600 ; lower to printing height
G1 F3000 ; restore feedrate
; FEATURE: Internal solid infill
; QZ_REFILL_END
; FEATURE: Internal solid infill
G1 X28 Y200 E19.5118 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E19.5118 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E19.5118 F1200
; QZ total consumed: 260 ml, refills: 2
; QZmini cold end — PROVISIONAL
M400
G92 E0
G91
G1 Z5 F600
G90
G1 X10 Y290 F3000
M106 S0
M104 S0
M140 S0
M84
