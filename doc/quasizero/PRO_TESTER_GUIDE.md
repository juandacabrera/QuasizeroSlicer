# Quasizero PRO — alpha tester guide

Thank you for testing. You will run two things: the free **Quasizero Slicer LITE** and the
**Quasizero PRO add-on**, a small separate program that computes the collapse simulation.
Your licence file (`.qzl`) unlocks it for you until the date written in it.

## Install (Windows, 5 minutes)

1. Unzip the Quasizero Slicer LITE portable anywhere (or run its installer). Start it once.
2. Run `QuasizeroPRO-addon-<version>-setup.exe`. It needs no administrator rights: it
   installs the engine for your Windows user only. When it asks for the licence file,
   browse to the `.qzl` you received. (You can also skip and activate later.)
3. Start the slicer, load a print with a Quasizero paste profile, slice, and open the
   **Preview**. In the **Stability** card you should read *Quasizero PRO — licensed to
   <your name>, until <date>*. Tick **Show deformation**.

If the card says *engine not installed*: use *Locate qz-sim…* and select
`%LOCALAPPDATA%\Quasizero\qz-sim\qz-sim.exe`. If it says *no licence installed* or
*licence expired*: *Activate licence…* and select your `.qzl` (or ask us for a renewed
one). Everything else in the slicer works without the add-on.

## What the simulation shows

The Stability card predicts, layer by layer, how close the paste is to yielding under its
own weight (Level 0/1); with the PRO engine the preview also plays the predicted collapse:
the wall bulges, folds over a hinge zone, meets the bed and settles, and the bead extruded
afterwards falls on the pile. These are **model predictions from a handful of material
parameters**, not measurements — the point of the alpha is to compare them with your
prints so the models can be calibrated.

## What we ask you to record (per material batch and per test print)

Material batch: recipe or product name, water content if known, wet density (weigh a
known volume), and the cylinder collapse test heights at 0, 5, 15, 30 and 60 minutes of
rest (the test is described in `QZMINI_STABILITY.md`; a phone video with a ruler in the
frame is enough). Room temperature and humidity.

Print: nozzle diameter, layer height, line width, speed, layer time (from the slicer),
any pauses; the geometry (hollow cylinder, thin wall, cone, or your own); the outcome —
layer/height/time when it started to lean or fold, whether it folded (plastic) or
buckled (elastic wave along the wall), and the slicer's prediction next to it; a video
from a fixed camera with something of known size in frame and the print time visible.

Send the material sheet, the G-code, the slicer's prediction (a screenshot of the
Stability card at the end of the print) and the video to Quasizero. Renewed licences are
issued on receipt of a complete session. Your name and email are inside your licence
file: please do not share it.

## Known limits of this alpha

Collapse kinematics were tuned on one recording of one material; timings inside the
collapse are not yet calibrated to real time; the bead after the collapse is a particle
chain, not a fluid; no self-collision between strands; Level 0/1 assume a straight wall
of uniform section per layer. Report anything that looks physically wrong — that is
exactly what we need.
