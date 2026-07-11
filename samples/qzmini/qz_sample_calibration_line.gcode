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
G1 Z3 F600
G0 X100 Y100 F3000
M83
G1 X120 Y100 E0.9 F1200 ; 20 mm calibration line (4 mm wide, 3 mm high)
; QZ total consumed: 0.239855 ml, refills: 0
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
