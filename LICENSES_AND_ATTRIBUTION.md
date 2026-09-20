# Licenses and Attribution

**Quasizero Slicer is based on OrcaSlicer.** This attribution is shown in the About dialog.

- Quasizero Slicer is a fork of [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer)
  at tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`.
- License: **GNU Affero General Public License, version 3** (`LICENSE.txt`, unchanged).
  Complete corresponding source for every distributed build is available from the repository
  hosting this branch (`pro` in the development repository; `main` in the public LITE
  repository).
- Editions. **Quasizero Slicer LITE** is this tree without `engine/`: AGPLv3 throughout,
  including the Stability panel (`src/libslic3r/QuasiZero/QzStabilityModel.*`), the toolpath
  skeleton and tube re-skin (`QzSkeleton.*`, `QzSimTypes.hpp`), the hook interface
  `src/slic3r/GUI/QuasiZero/QzProHooks.hpp` and the client of the external engine
  (`src/slic3r/Utils/QzEngine.*`, `src/slic3r/GUI/QuasiZero/QzProClient.*`). The client
  starts `qz-sim` as a separate process and exchanges documented JSON/binary messages with it
  (protocol `qz-sim-serve/1`); it contains no engine code. The engine itself (`qz-sim`,
  repository `qz-sim-engine`) is Quasizero's own work under its own licence: it is a separate
  program communicating at arm's length, in the sense of the GPL FAQ, and is what a PRO user
  installs and activates with a licence file. **PRO integrated** (`QZ_PRO=ON`) compiles the
  engine into the slicer (`src/slic3r/GUI/QuasiZero/QzProView.*`): any *distributed* binary of
  that build is a work based on OrcaSlicer and is covered by the AGPLv3 as a whole, engine
  included, so it stays internal (development, the all-in-one fallback edition) unless the
  engine is published under the AGPLv3 too.
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
