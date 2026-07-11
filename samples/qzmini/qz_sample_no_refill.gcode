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
G1 X20 Y200 E18.7614 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E18.7614 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E18.7614 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E18.7614 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E18.7614 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;6
G1 Z6 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E18.7614 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E18.7614 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E18.7614 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E18.7614 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E18.7614 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;9
G1 Z9 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E18.7614 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E18.7614 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E18.7614 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E18.7614 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E18.7614 F1200
; CHANGE_LAYER
;AFTER_LAYER_CHANGE
;12
G1 Z12 F600
G0 X20 Y20 F3000
; FEATURE: Internal solid infill
G1 X20 Y200 E18.7614 F1200
G0 X24 Y20 F3000
; FEATURE: Internal solid infill
G1 X24 Y200 E18.7614 F1200
G0 X28 Y20 F3000
; FEATURE: Internal solid infill
G1 X28 Y200 E18.7614 F1200
G0 X32 Y20 F3000
; FEATURE: Internal solid infill
G1 X32 Y200 E18.7614 F1200
G0 X36 Y20 F3000
; FEATURE: Internal solid infill
G1 X36 Y200 E18.7614 F1200
; QZ total consumed: 100 ml, refills: 0
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
