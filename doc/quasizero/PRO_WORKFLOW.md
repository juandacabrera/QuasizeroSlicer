# Quasizero PRO — how the pieces fit and how to work with them

State on 2026-09-20. Three repositories, two editions, one licence scheme.

| Piece | Repository | Visibility | Licence | What it is |
|---|---|---|---|---|
| Quasizero Slicer LITE | `juandacabrera/QuasizeroSlicer` (`main`) | public | AGPLv3 | OrcaSlicer fork: QZmini presets, refill assist, Stability panel (Level 0/1), **client of the PRO engine** + activation UI |
| Quasizero Slicer (development) | `juandacabrera/QuasizeroSlicer-dev` (`pro`) | private | AGPLv3 tree + private submodule | the single source of truth: LITE files + the PRO integrated build (`QZ_PRO=ON`) |
| Quasizero simulation engine | `juandacabrera/qz-sim-engine` (`main`) | private | Quasizero, dual-licensed | `qz-sim` (library, batch CLI, `serve` process), `qz-license`, the PRO add-on installer |

Two ways to run the PRO simulation, same physics, same drawing:

- **PRO integrated** — `build\` with `QZ_PRO=ON`: the engine compiled into one executable. Development, tests, and the all-in-one edition to fall back to if the external-engine design is ever dropped (publishing the engine under the AGPL is then the only step).
- **LITE + engine** — the public slicer starts `qz-sim serve` as a separate process (protocol `qz-sim-serve/1`, `src/slic3r/Utils/QzEngine.hpp`) and draws what it answers. The engine refuses to load a job unless a licence file grants `pro`. This is what testers and, later, customers get: the slicer is free software, the engine is Quasizero's.

## 1. Daily development

One checkout of `QuasizeroSlicer-dev` with the `engine/` submodule (BUILD_WINDOWS.md, option C):

1. Edit. Engine changes go in `engine/src` (a submodule: commit and push them in the engine repository, then commit the new submodule pointer in the slicer). Slicer changes go in `src/`.
2. Rebuild the flavour you are testing: `cmake --build --preset win-pro-app` + `win-pro-install`, or `win-lite-app` + `win-lite-install`; the engine with `cmake --build engine\build --config Release`.
3. To test the external path on your PC: copy `engine\build\Release\qz-sim.exe` to `%LOCALAPPDATA%\Quasizero\qz-sim\` (or point the slicer at it with *Locate qz-sim…*), run `build-lite\OrcaSlicer\orca-slicer.exe`, load a G-code with a characterised material and open the Stability card.
4. Tests: `ctest --test-dir engine\build -C Release` (engine, 40) and `bash tests/qzmini/standalone/run_standalone.sh` (slicer 56 + engine 40 in a PRO tree; the client test drives a real `qz-sim serve`).
5. Push to `pro`: tests + Linux compile gate (PRO) in the private repo; `sync_lite.yml` republishes the LITE snapshot to the public `main`, whose CI builds the free Windows LITE portable and installer. Docs-only commits: `[skip gate]` in the subject.
6. Push to the engine's `main`: Linux tests, then the Windows build with the add-on installer (`qz-sim-addon-windows-<version>` artifact) and the licence tool (`qz-license-tools-windows-<version>`, Quasizero only).

Shared files (identical copies in both repositories, checked at configure time of a PRO build): `QzStabilityModel.{hpp,cpp}`, `QzSimTypes.hpp`, `QzSkeleton.{hpp,cpp}`. Edit them in one place and copy to the other before committing.

## 2. Keys and licences

The engine verifies licences against the public key compiled in from `engine/keys/qz_public.key`. **The key committed today is a development key made in the authoring sandbox.** Before the first real distribution:

1. On your PC, in the engine checkout: `build\Release\qz-license.exe keygen keys` → `keys\qz_private.key` (secret: keep it on this PC and in an offline backup; it is git-ignored) and `keys\qz_public.key`.
2. Commit `keys\qz_public.key`, push; rebuild the engine (locally and in CI). Every licence issued with the old key stops working — intended.

Issuing a licence (a few seconds):

    qz-license issue --key keys\qz_private.key --licensee "Ada Lovelace" --email ada@example.org --days 90 --features pro,calibration --note "alpha tester, cohort 1" --out Ada.qzl
    qz-license verify Ada.qzl

Fields are signed: `id` (auto, `QZ-<date>-<hex>`, printed in reports), `licensee`, `email`, `issued`, `expires` (inclusive), `features` (`pro` = deformation view; `calibration` = calibration sessions, once implemented), `note`. Renewal = issue a new file. Revocation = expiry (short terms for testers: 60–90 days) or a key rotation for everyone.

What it gives: control of who, until when and for what; traceability (the file names its owner); an expiry-and-renew loop that you can tie to the delivery of calibration data. What it does not give: protection against a determined reverse engineer — no local licence scheme does. For an alpha that is the right trade-off.

## 3. What a tester receives and does

1. Downloads the **Quasizero Slicer LITE** portable (public repository, Actions artifacts, or a release once you cut one).
2. Runs **QuasizeroPRO-addon-<version>-setup.exe** (from the engine repository's artifact): installs `qz-sim.exe` to `%LOCALAPPDATA%\Quasizero\qz-sim\`; the wizard offers to select the `.qzl` licence right away.
3. Starts the slicer, loads a job with a characterised material and opens the Stability card in the preview: it shows *Quasizero PRO — licensed to Ada Lovelace, until 2026-12-19* and the *Show deformation* checkbox. If the licence was not selected in the installer: *Activate licence…* on the same card.
4. Reports (until the calibration session exists in the slicer): material sheet, print parameters, the test geometries printed until failure, and the videos (see `PRO_TESTER_GUIDE.md`).

If the engine is missing or the licence invalid, the card says exactly which (not installed / not running / no licence / expired on … / does not include PRO) with the matching button; the rest of the slicer works as LITE.

## 4. Calibration sessions (next implementation step)

Goal: a dataset consistent across testers, usable first to fit the physical parameters per material and later to learn residual corrections. Planned pieces:

- **Session export** from the PRO slicer: the job document the engine received (material, layers, geometry descriptors, segments), the G-code, the predicted collapse (layer, height, time, mode) and a form (`session.json`) the tester fills: material batch (recipe, water content, density, cylinder-collapse test heights at 0/5/15/30/60 min rest), environment (temperature, humidity), print parameters (nozzle, layer height, width, speed, layer time, pauses), observation (collapse layer/height/time, mode, notes), attachments (photos, video with fixed camera, scale and clock).
- **Test geometries**: hollow cylinder (buildability limit), thin straight wall (buckling), tapered cone with overhang, one free geometry — printed until failure or completion.
- **Storage**: a private data repository (`qz-calibration-data`) with one folder per session and a versioned schema; a script that validates a session and computes prediction residuals.
- **Modelling**: per-material fit of ρ, τ₀, A_thix, E₀, ξ_E by least squares on the collapse tests (works from the first session); machine learning on residuals only past ~50–200 documented prints, with leave-one-geometry-out and leave-one-material-out validation so that the model is judged on what it must do — extrapolate.

## 5. Reverting to one integrated, free tool (if ever decided)

Nothing to undo in the slicer: the PRO integrated build already is that tool. Steps: publish the engine repository (or copy `engine/src` into the public tree) under the AGPLv3, remove the licence gate from the build (`QZSIM_REQUIRE_LICENCE` option to add at that point) and let the public CI build with `QZ_PRO=ON`. The client and the protocol stay useful (Grasshopper, scripts).
