# QZmini paste stability simulation (Level 0/1, deformation Level 1.5 v2)

**Status: implemented, NOT hardware-validated.** The model runs on every loaded G-code
(sliced or from the G-code Editor) and is exposed as the *Stability* view of the preview
plus a *Stability* card. All material numbers shipped in the presets are **[hypothesis]**
starting values; they must be replaced by measured ones (§4) before trusting a prediction.

## 1. What it computes

Source: `src/libslic3r/QuasiZero/QzStabilityModel.{hpp,cpp}` (pure C++17, no
dependencies; tested by `tests/qzmini/test_qz_stability.cpp`).

1. **Plastic collapse, layer by layer.** After each layer *k* is finished, every layer
   *j ≤ k* carries the weight above it, `σ_j = ρ·g·(z_top − z_j)`, against a strength
   that grows with its age, `σ_p,j = k_p·(τ0 + A_thix·age_j)` [Roussel 2018; Perrot 2016;
   Suiker 2018 eqs. 76–92]. Layers close to the bed are laterally confined and carry
   `1/(1−ν)` more [Suiker 2018 eqs. 79–80] — this is what moves the critical layer
   above the base, as seen in the cylinder collapse video. Utilization
   `U_j = σ_j/σ_p,j`; collapse when `max U ≥ 1`. The peak utilization of each layer
   over the whole print is what the *Stability* colour map shows (0 → 1 = collapse).
2. **Closed forms**: `H_max = σ_p0 / (ρg − k_p·A_thix / l̇)` and the critical vertical
   speed `l̇_crit = k_p·A_thix/(ρg)` above which plastic collapse is inevitable for tall
   parts.
3. **Buckling indicators** (free straight wall, single bead of the mean width):
   `H_cr = (7.837·D0/(ρ g h))^{1/3}` with `D0 = E0 h³/12(1−ν²)` [Suiker 2018], and the
   same problem with age-dependent stiffness solved by Rayleigh–Ritz (validated against
   the classical 7.837). Corrugation and cylinder-shell helpers exist in the module for
   Level 1 geometry classification (not yet wired to the toolpath).
4. **Recommended layer time**: the uniform time-scale factor that keeps the whole print
   below `1/safety_factor`.

Layer records come straight from the processed moves: real process time per layer
(dwells, refill pauses included), bead height/width from the metadata.

## 1b. Level 1.5 — deformation animation ("Show deformation" in the Stability card)

A kinematic view of the stack driven by the model, synchronised with the player
(`qz_deformation_state` / `qz_deform_point`):

- **Squash / bulge**: each layer shortens by `ε = ε_max·((U − U_y)/(1 − U_y))²`
  (U_y = 0.5, ε_max = 0.35 [hyp]) and spreads in plan about its centroid by `1 + 0.6·ε`;
  layers stack on the shortened ones below. The barrel appears where the utilization
  band is — above the bed, as in the collapse video.
- **Sway**: the stack is a heavy column with the real plan inertia of every layer
  (minimum second moment of area of the toolpath section, age-dependent E). Its load
  factor λ_cr amplifies an initial imperfection (0.2 % of height [hyp]) by
  `1/(1 − 1/λ_cr)` along the weak axis, mode `1 − cos(πz/2H)`. λ_cr ≤ 1 = buckling
  collapse (hinge at the base).
- **Fold**: once the model reaches collapse (plastic: hinge at the critical layer;
  buckling: base), the layers above rotate about the hinge edge towards the weak axis,
  up to 75° over two layer times [hyp]; the hinge layer crushes.

It is a visualisation of the analytical model, **not** a nonlinear FEM: shapes are mode
shapes and hinge kinematics, not equilibrium solutions. Level 2 (staged beam/shell
model on the toolpath, Karamba-style) remains future work.

## 1c. Level 1.5 v2 — grid-resolved, history-aware stack simulation

Source: `src/libslic3r/QuasiZero/QzStackSim.{hpp,cpp}` (pure C++17; tested by
`tests/qzmini/test_qz_stacksim.cpp`). Since v2 the *Show deformation* view is driven by
this simulator instead of the per-layer state of §1b; the per-layer functions stay in the
model (and in the tests) as the analytical reference.

- **Spatial resolution.** A plan grid (2 mm cells, enlarged automatically for very large
  jobs) carries the nominal material height after every deposition step, so the load on a
  point of layer *i* is the *local* column above it. An irregular part (a half ring printed
  on a full ring, an overhanging wing) loads different zones of the same layer differently;
  the uniform-ring case reproduces the §1 layer model exactly (unit test).
- **Memory.** Every (layer, cell) remembers the peak load it has seen; the plastic squash is
  irreversible and accumulates layer after layer. The peak can only occur when the column
  above a cell grows, so it is searched at those steps only (this keeps the build of a
  480 000-segment job around one second).
- **Settlement.** Each point drops by the squash of all the material below it, so barrels
  and dents show where they belong. The height reported in the card is the mean deformed
  top of the top layer's material (a ring has no material at its centroid).
- **Sway** as in §1b (imperfection amplified by the buckling load factor along the weak
  axis). The fold direction is fixed for the whole simulation (weak axis of the hinge
  layer), so it never swings as layers are added.
- **Collapse kinematics** (redesigned on the cocoa-paste cylinder of `IMG_4657`, frames
  25.5–29.7 s: 25 s of silent growth, one-sided bulge of the lower third over ~2 s, the
  wall bends over a zone of 8–10 layers, the upper part swings over in ~1 s until its rim
  meets the bed, the arch settles and widens). Each frame runs a phase machine:
  *Stable* → *Pre-failure* when the remembered load/strength of the hinge zone passes 0.6
  (the bulge becomes asymmetric towards the side the stack will fold to, a lean ramps
  smoothly to 1.5°) → *Failure* at t_collapse (the axis bends with constant curvature over
  a distributed hinge zone of 0.8 section diameters, a quarter below the critical layer
  and the rest above, rigid continuation above; the inner side compresses like an
  accordion — offset capped at 0.8 R, never an inversion — the outer side opens; the angle
  grows as 1.5°·exp((t − t_c)/0.4 s): long accumulation, brief event) → *Collapsed* when
  the lowest bead bottom of the rotated part reaches the bed (contact angle by bisection
  every frame; over the next second the arch settles: nothing below the bed, the part on
  the bed squashes and spreads along the fold, +8° absorbed; the hinge zone crushes with a
  bell profile and the settlement below is recomputed) → *Post-collapse* while more
  layers are printed. Buckling collapses put the hinge zone at the base.
- **After the collapse — the bead keeps coming** (`QzBeadSim`, `QzBeadSim.{hpp,cpp}`,
  `tests/qzmini/test_qz_beadsim.cpp`). The head follows its nominal path; the bead
  extruded from then on is a chain of particles emitted at the nozzle at the real cadence
  (one per bead width of path) with position-based dynamics: gravity, strong viscous
  damping, inextensible links along the chain, a weak bending smoothing of the hanging
  part, a little smooth lateral noise at emission, collision with a height field of
  everything already deposited (the settled stack plus the strand itself once at rest —
  frozen particles are stamped into the field, flattened ×1.35), friction on contact. The
  strand hangs from the nozzle, descends at the extrusion speed, lands under the head and
  is laid along the projected path; over the edge of the pile it drapes down (a step
  taller than 1.5 bead heights is a wall: the strand is stopped against it, never lifted
  onto it); rings pile up strand after strand. A travel (XY jump > 1.5 bead widths or a
  z jump > 2.5 bead heights) lets the strand go and starts a new one; a plain layer change
  keeps it continuous; when the path ends the strand stays hanging from the stopped head.
  Deterministic for a given seed; snapshots every 20 s make scrubbing backwards cheap.
  Coverage: the first 15 min of print time after the collapse (`max_duration`), later
  the head goes on without a strand. The bead is drawn as one continuous tube per chain,
  the attached one up to the nozzle, coloured at the bottom of the load/strength ladder
  (it carries no load). The height-field sketch of dropped strands remains the fallback
  when the bead simulator cannot run (no collapse, no segments after it).
- **Colour.** The deformed beads are coloured by the local load/strength they *remember*
  up to the play instant (Stability palette) — the same per-cell field that drives the
  squash, so colour and geometry agree; it is ≥ the instantaneous value and never resets.
  For the per-cell field the memory is exact once a cell has reached its peak; before
  that it is the instantaneous value (a small dip is possible between load increments
  for fast-curing materials — the per-layer field of the Stability view is exact).
- **Range.** With *Show deformation* on, every layer of the layers range up to the player
  position is drawn (the "sequential slider applies only to top layer" preference does
  not hide the layers below), coloured as above.
- **Rendering** (`QzSkeleton`, `QzSkeleton.{hpp,cpp}`, `tests/qzmini/test_qz_skeleton.cpp`).
  Consecutive extrusion segments that share an endpoint (no travel between, same layer)
  form one bead = polyline of *shared* centre-line nodes; the simulator poses the nodes
  (a node shared by two segments gets one position) and the tubes are re-skinned as
  continuous strips — one ring per node with orientation continuity, so a bead bends,
  squashes and falls without breaking into per-move blocks. Octagonal section (square
  beyond 40 000 visible strands), rebuilt whenever the player position or time changes;
  the nominal toolpaths are rendered masked so libvgcode keeps its deferred updates.
  Acceptance test: the number of connected mesh components equals the number of visible
  beads under any displacement (`connected_components`).
- **Player.** With the deformation view on, the moves slider runs in slow motion (1/6 of
  the layer pace) from 0.5 s before t_collapse to 4.5 s after it, then resumes.

Options (`QzSimOptions`, `QzBeadOptions`, all **[hyp]**): squash starts at U = 0.5 and
reaches 35 % at U = 1, bulge gain 0.6 (×1.2 on the compressed side), pre-failure from 0.6
of the hinge-zone load/strength, imperfection 0.2 % of height, sway cap 25 %, hinge zone
0.8 diameters (25 % below the critical layer), lean 1.5°, fold time constant 0.4 s, cap
150°, settling 1 s / +8°, hinge crush +45 %, inner offset cap 0.8 R; bead: dt 1/120 s,
damping 0.85 per step, 8 constraint passes, bending 0.1, laid-strand share 0.2, noise
0.2 of the nozzle speed, rest 0.5 s, flatten ×1.35, wall 1.5 bead heights, 20 000
particles, 900 s.

## 2. Where it shows

- **Preview → view type "Stability"**: extrusions coloured by load/strength **as it stands
  at the layer selected by the vertical slider** (top of the range = k), with memory: every
  layer j ≤ k shows the peak it has seen up to step k, so the colour of a layer never
  fades when the print moves on and the play shows the field building up from the bed.
  At the last layer this equals the whole-print peak. Legend fixed 0–1; the status bar
  shows the value of the bead under the horizontal slider ("Load/strength, this bead"),
  which for the layer being deposited is ~0 — the card's "At layer k: x % on layer j" is
  the maximum over the stack.
- **Stability card** (left stack, above Process): verdict (Stable / below margin /
  Collapse predicted at height, layer, minute), critical layer, live load at the layer
  the vertical slider shows, vertical speed vs critical speed, max plastic height,
  free-wall buckling height, buckling factor of the real section, layer-time factor,
  and the **Show deformation** toggle (Level 1.5).
- **Material preset → "QZmini paste stability"**: yield stress [Pa], structuration rate
  [Pa/min], elastic modulus [kPa], stiffening rate [kPa/min], Poisson, yield factor.
  Density comes from `filament_density`.
- **Printer → QZmini → Paste stability simulation**: enable, safety factor, bed confinement.

## 3. Material database (starting values)

Literature ranges, to anchor expectations. Values are tagged **[lit: source]** or
**[hypothesis]**; wet properties, as printed.

| Material class | ρ [kg/m³] | τ0 at deposition [Pa] | A_thix [Pa/min] | E0 [kPa] | Ė [kPa/min] | Notes / sources |
|---|---|---|---|---|---|---|
| Printable concrete (Weber 145-2, TU/e) | 2100 | c0 = 2600 (cohesion) | 63.6 (c) | 39.5 | 1.705 | ν = 0.24, φ = 20° — [lit: Wolfs, Bos, Salet 2018 via Vantyghem et al. 2020, Table 1] |
| Printable concretes, general | 2000–2300 | 1500–1800 typical (0.16–6.8 k range) | 6–120 | 30–100 | 1–3 | [lit: reviews, PMC10650098; static yield table] |
| Geopolymer mortars (fly ash) | 1900–2100 | 600–1000 initial → 800–3000 | strongly thixotropic | ~30–80 [hyp] | — | [lit: PMC13306987; Panda et al.] |
| Earth / cob (raw) | 1800–2000 | 2000–20000 | slow (drying only) | 50–500 [hyp] | — | [lit: Perrot 2018 "couple of kPa to tens of kPa"]; 3 % alginate → 1 m wall self-supporting in 0.1 h |
| Stoneware / porcelain paste (DIW) | 1700–1900 | ~1000–3000 (G′ 10⁵–10⁶ Pa small-strain) | small | 50–300 [hyp] | — | [lit: ACS Omega 2024 porcelain DIW; clay solids-fraction study] |
| Wood flour + methylcellulose (LDM) | wet 900–1200 [hyp]; dry 330–480 | 500–1500 [hyp] | small (drying) | 10–40 [hyp] | — | [lit: Rosenthal et al. 2018: 89 % wood dry mass, shrinkage 17–20 %, dry MOR 2.3–7.4 MPa] |
| Xanthan-bound biopastes (food-type) | 1000–1300 | 100–1000 for 0.5–1.5 % XG gels; more with 30 %+ filler | small | 5–30 [hyp] | — | [lit: XG paste rheology studies] |

### Juanda's pastes (initial values shipped in the presets — all [hypothesis])

| Preset | Recipe | ρ (preset) | τ0 [Pa] | A_thix [Pa/min] | E0 [kPa] | Ė [kPa/min] | Basis |
|---|---|---|---|---|---|---|---|
| Biocomposite Ultra High Density | cocoa husk 50 g + xanthan 5 g + water 100 ml, 1 day rest | 1.50 g/cm³ | 550 | 2.4 | 30 | 0.36 | Fitted so the Level 0 model collapses at ≈35 layers of 3 mm with a critical band 12–24 mm above the bed, like the video (double wall Ø≈60 mm, 3 mm/s). Two-speed cylinder test will pin τ0 and A_thix. |
| Biocomposite Sawdust | sawdust + CMC | 1.20 g/cm³ | 800 | 3.0 | 20 | 0.20 | Wood-flour LDM pastes are stiff but light; CMC gels dry-strengthen slowly. |
| Biocomposite High Density | denser filler blend | 1.40 g/cm³ | 1200 | 3.0 | 40 | 0.30 | Placeholder between the two. |
| (planned) wool powder + CMC | — | ~1.1 [hyp] | 400–800 [hyp] | 2–4 [hyp] | 10–20 [hyp] | — | Fibrous, low density: expect buckling (not yield) to govern thin walls. |

## 4. Calibration protocol with the QZmini (one afternoon)

1. **Density**: weigh a full syringe of known ml (the counter gives the volume).
2. **Test A — cylinder to collapse, two speeds**: Ø60 mm, double wall, 3 mm layers,
   at 3 mm/s and at 6 mm/s. Film with a ruler in plane. Record collapse layer, time,
   and the height of the bulge centre. `qz_identify_two_cylinder_tests()` (also in
   `qz_stability_model.py`) returns τ0 and A_thix.
3. **Test B — straight free wall, two speeds**: single bead, 100–150 mm long, until it
   buckles sideways. `H_cr` → E0 (closed form); the second speed → stiffening rate.
4. Enter the values in the material preset, re-run the preview, compare the predicted
   collapse layer with the real one. Note temperature/humidity and paste batch.

## 5. Limitations (honest)

- Level 0 knows height, time and material — not the shape of the layer. A closed
  cylinder and a straight wall get the same plastic verdict; only the buckling
  indicator (single-bead free wall) hints at geometry. Level 1 (per-segment geometry:
  free wall length, curvature, closed loops, corrugation) is the next step.
- Strength growth is linear; drying-driven build-up depends on humidity, bead size and
  exposure. Re-calibrate per batch.
- Not an FEM: the deformed geometry (§1b, §1c) is kinematic — mode shapes, a distributed
  hinge with a prescribed fold law, a settled height field — not an equilibrium solution;
  no adhesion failure, no imperfections beyond the confinement/knockdown factors. The
  post-collapse bead is a particle chain on a height field: no bead-to-bead adhesion, no
  self-collision between hanging strands, a cliff stops the strand instead of letting it
  slide down, the pile is stamped (no angle of repose), and it falls on the *settled*
  stack from the start (the fold itself takes ~3 s; the strand needs longer than that to
  reach the pile). Only the first 15 min after the collapse are simulated.
- No prediction here is hardware-validated. Treat the card as a warning system, not a
  guarantee.

## 6. References

Suiker 2018 (Int J Mech Sci 137) · Wolfs & Suiker 2019 (IJAMT 104) · Wolfs, Bos, Salet
2018 (CCR 106) · Roussel 2018 (CCR 112) · Perrot, Rangeard, Pierre 2016 (Mater Struct
49) · Perrot, Rangeard, Courteille 2018 (CBM, earth) · Kruger, Zeranka, van Zijl 2019
(CBM 224) · Chang et al. 2021 (CACAIE) · Vantyghem, Ooms, De Corte 2020 (FEM techniques,
arXiv 2009.06907) · Rosenthal et al. 2018 (Eur J Wood Prod 76) · Karamba3D buckling
simulation for 3DCP (Witteveen+Bos / TU/e / NTU).
