# The paste parameters of the Stability model — what they are and how to get them

The material preset (Filament settings → *QZmini paste stability*) holds six numbers. They
are the input of every prediction the Stability card makes (Level 0/1 verdict, critical
speed, the PRO collapse animation). This page says what each one is in the language of the
literature, how it is measured, and — for alpha testers — which ones you should measure
and which ones you should leave alone. Every field also carries this information as a
tooltip in the slicer.

| In the slicer | Scientific name, symbol, unit | Meaning for the print | Who sets it |
|---|---|---|---|
| Yield stress at deposition | static yield stress of the fresh paste, τ₀ (Pa) | the stress the paste bears without flowing the moment it leaves the nozzle; a layer at the bottom carries the weight of the stack above it — when that weight per area reaches the strength of the layer (yield factor × τ₀), the layer squashes and the print folds | **measured** (cylinder collapse test) |
| Structuration rate | thixotropic structuration rate, A_thix (Pa/min) | how fast the yield stress grows with the age of a layer: flocculation, water loss, setting of the binder. τ(t) = τ₀ + A_thix · t. This is why printing slower (more time per layer) lets you go higher, up to the point where a layer dries so much that the next one does not bond | **measured** (the same test at two speeds) |
| Elastic modulus at deposition | Young's modulus of the fresh paste, E₀ (kPa) | stiffness of the wet bead. It decides the second way a wall fails: not by squashing but by leaning sideways and buckling (thin, tall, straight walls) | measured when possible; defaults are order-of-magnitude |
| Stiffening rate | dE/dt, Ė (kPa/min) | growth of the modulus with layer age, same logic as the structuration rate | defaults; second-speed wall test when possible |
| Poisson ratio | ν (–) | lateral expansion under compression; 0.3–0.45 for pastes, 0.5 = incompressible | **leave the default** (low sensitivity) |
| Yield criterion factor | ratio compressive strength / shear yield stress: √3 = 1.732 (von Mises), 2 (Tresca) | which failure criterion converts a shear yield stress into the vertical strength of a layer | **leave the default** |

Density (the *filament density* field, g/cm³) is the seventh number: the weight of the
stack comes from it. Weigh a known volume of paste (the syringe volume is known).

`Yield stress = 0` means "material not characterised": the Stability card switches off
for that material instead of guessing.

## Where the numbers come from

These are the parameters of the buildability models of 3D concrete printing, transferred
to biopastes: Roussel's thixotropy model (τ(t) = τ₀ + A_thix·t) with Perrot's
buildability criterion for the plastic collapse, and Suiker/Wolfs' linearly stiffening
elastic wall for buckling (see `QZMINI_STABILITY.md` §6 for the references). Papers give
values for concretes, geopolymers, clays and a few earth and wood-flour pastes
(`QZMINI_STABILITY.md` §3); for a new biopaste they only bracket the order of magnitude —
τ₀ from a few hundred to a few thousand pascals, A_thix from 1 to 100 Pa/min, E₀ from 10 to
100 kPa. The numbers must be measured per recipe, and re-measured per batch when the
water content or the resting time changes.

Three ways to measure, from cheapest to most rigorous:

1. **Cylinder collapse test with the printer (what we ask testers for).** Print a
   hollow cylinder (Ø 60 mm, double wall, 3 mm layers) at a fixed speed until it folds;
   film it with a ruler in the plane of the wall and a clock in view. Collapse height and
   time give τ₀ once the density is known; the same test at a second speed (or with a
   pause per layer) separates A_thix from τ₀. The slicer's calibration session (in
   preparation) computes both from the two videos; until then Quasizero does it from the
   recorded heights and times (`qz_identify_two_cylinder_tests` in the stability model).
   The straight free-wall test (a single 100–150 mm bead until it buckles sideways, two
   speeds) gives E₀ and Ė the same way.
2. **Bench tests.** Static yield stress by the slump or the mini-cone test (a known volume
   of paste released on a plate; height and spread → τ₀ with the Roussel–Coussot formula),
   or by pushing a small plunger into the paste with a kitchen scale (penetrometer). Young's
   modulus by compressing a short cylinder of paste between two plates with a scale and a
   caliper: force / area over shortening / height at small strain.
3. **Rheometer.** Vane geometry at a low constant shear rate: the peak stress is τ₀; the
   peak after t minutes of rest is τ₀ + A_thix·t. Oscillatory sweep in the linear range:
   the storage modulus G′ gives E = 2·G′·(1 + ν). This is the reference method and the one
   to quote in a paper.

## What an alpha tester actually sends (no parameter fitting needed)

You do not have to turn measurements into parameters. Send the raw data and Quasizero
fits the model: the material sheet (recipe, water content, resting time, wet density),
the cylinder collapse test at two speeds and the free-wall test (videos with ruler and
clock, the G-code, the collapse layer/height/time you observed), plus temperature and
humidity. `PRO_TESTER_GUIDE.md` lists the exact items. In return you get the values for
your material preset — and every session widens the dataset the models are calibrated on.

## What the model does with them, in one paragraph

Each printed layer is born with strength τ₀ and stiffness E₀ and gains A_thix and Ė per
minute of age. Layer by layer the slicer computes the weight of everything above each
layer, compares it with that layer's strength (Level 0: plastic collapse, the *load /
strength* colours of the Stability view) and with the buckling load of the wall it belongs
to (Level 1: geometry — free wall length, curvature, closed loops). The first layer whose
load reaches its strength sets the collapse height and time; the PRO engine then plays the
kinematics of that failure. Everything downstream — critical speed, layer-time factor,
maximum height — is the same model read backwards.
