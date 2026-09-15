# Licenses and Attribution

**Quasizero Slicer is based on OrcaSlicer.** This attribution is shown in the About dialog.

- Quasizero Slicer is a fork of [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer)
  at tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
- License: **GNU Affero General Public License, version 3** (`LICENSE.txt`, unchanged).
  Complete corresponding source for every distributed build is available from the repository
  hosting this branch (`pro` in the development repository; `main` in the public LITE
  repository).
- Editions. **Quasizero Slicer LITE** is this tree without `engine/`: AGPLv3 throughout,
  including the Stability panel (`src/libslic3r/QuasiZero/QzStabilityModel.*`) and the hook
  interface `src/slic3r/GUI/QuasiZero/QzProHooks.hpp`. **Quasizero Slicer PRO** adds
  `src/slic3r/GUI/QuasiZero/QzProView.*` and the Quasizero simulation engine (`engine/`,
  repository `qz-sim-engine`, Quasizero's own work, dual-licensed: see `engine/LICENSE`).
  A PRO build compiles the engine into the slicer, so any *distributed* PRO binary is a
  work based on OrcaSlicer and is covered by the AGPLv3 as a whole, engine included.
  PRO builds are therefore internal (development, testing, DevBeta under agreement) until
  the engine runs as the separate `qz-sim` process behind its JSON interface, which is the
  arm's-length arrangement for a commercial edition.
- Upstream lineage and notices preserved: OrcaSlicer (SoftFever and OrcaSlicer contributors),
  BambuStudio (Bambu Lab), PrusaSlicer (Prusa Research), Slic3r (Alessandro Ranellucci) and the
  third-party libraries listed in the About dialog and `resources/data/`.
- Upstream copyright statements in source headers and the About dialog are retained. New
  Quasizero files carry AGPLv3 headers naming Quasizero and referencing the upstream base.

## Trademarks and logos

- OrcaSlicer, Bambu Lab, Prusa and other third-party logos are **not** used as Quasizero assets.
  All Quasizero branding (icon, splash, About artwork) derives solely from Quasizero's own
  logomark and wordmark. Bambu bed *textures* (logo artwork) were deliberately not copied into
  the Quasizero vendor library; only geometric bed models are referenced.
- Printer names (Artillery, Bambu Lab) are used solely to identify compatible hardware.

## Networking

No proprietary networking binaries are added or redistributed beyond what upstream OrcaSlicer
lawfully ships. No authentication mechanism is bypassed, forged or reverse engineered.
