# Supported Printers

All QZmini profiles retain the original machine's bed geometry, kinematics, axis limits, homing
direction, printable area and firmware family from the validated upstream OrcaSlicer v2.4.2
profiles. No QZmini XYZ tool offset is invented: none has been supplied or measured. Machine
acceleration/jerk limits are inherited unchanged; the QZ process preset uses conservative values
(300 mm/s² / jerk 5) far below every machine limit and marked provisional.

## QZmini @ Artillery Sidewinder X2 — primary target

- Firmware: Marlin. Bed 300×300 mm, height 400 mm, limits from upstream Artillery profile.
- Cold extrusion enabled in start G-code with `M302 S0`.
- Pause: `M0` (machine profile). **Status: Offline G-code validated. NOT hardware validated.**
- G29 bed leveling intentionally omitted until QZmini probe clearance is physically verified.
- Connectivity: local export; standard print-host integrations as provided by upstream.

## QZmini @ Bambu Lab A1 mini — EXPERIMENTAL / SLICE-ONLY

- Bed 180×180 mm, height 180 mm, limits from upstream BBL profile.
- **Not hardware validated.** Stock firmware acceptance of cold extrusion is UNVERIFIED and may
  reject E moves below its minimum extrusion temperature.
- Use: slice + export to file / SD. Network sending: only officially supported mechanisms
  (Bambu Connect, user-enabled Developer Mode, supported LAN). No authentication bypass,
  token forging, Bambu Studio impersonation, or proprietary re-enablement — not included,
  not planned.

## QZmini @ Bambu Lab P1S — EXPERIMENTAL / SLICE-ONLY

- Bed 256×256 mm, limits from upstream BBL profile (incl. bed exclusion area).
- Same restrictions and policy as the A1 mini.

## Nozzle variants

Every QZmini printer ships with 4.0 mm (primary) and 2.0 mm nozzle variants, matching the
physical QZmini nozzles. Default processes: 3 mm layers @ 4.0 nozzle, 1.5 mm layers @ 2.0 nozzle.

## Other firmware families

Klipper, RepRapFirmware and Prusa/Buddy refill adapters exist as documented, UNVERIFIED
templates (see QZMINI_REFILL_ASSIST.md). No printer preset ships for them yet.

## Validation vocabulary used across this project

*Unit tested* / *Offline G-code validated* / *Hardware validated* / *Experimental* /
*Slice-only* / *Unverified* / *Requires calibration*. A feature is never labelled
hardware validated unless the complete sequence was observed on a real matching printer.
