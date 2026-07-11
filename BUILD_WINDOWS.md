# Building Quasizero Slicer for Windows x64

Quasizero Slicer is a fork of OrcaSlicer and uses the unmodified upstream build
system. Base: upstream tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`,
branch `quasizero-slicer-mvp`.

## Option A — GitHub Actions (recommended, reproducible)

The fork workflow `.github/workflows/quasizero_build.yml` runs on every push to
`quasizero-slicer-mvp` (or manually via *Actions → Quasizero Build → Run workflow*).
It reuses OrcaSlicer's own chain (`build_check_cache.yml` → `build_deps.yml` →
`build_orca.yml`) on `windows-latest` and produces these artifacts:

- `OrcaSlicer_Windows_<ver>_x64_portable` — portable build directory (zip)
- `OrcaSlicer_Windows_<ver>_x64` — NSIS installer `.exe` (via `cpack -G NSIS`)

The first run builds the full dependency tree (~1.5–2 h); later runs reuse the
deps cache and take ~30–45 min.

## Option B — Local build from a fresh checkout

Prerequisites (as upstream):
1. Visual Studio 2022 with the "Desktop development with C++" workload
2. CMake 3.31.x (CMake 4.x removed pre-3.5 policies; the build script sets
   `CMAKE_POLICY_VERSION_MINIMUM=3.5` as a mitigation)
3. Git
4. gettext tools on PATH (for `scripts/run_gettext.bat`), e.g. from
   https://mlocati.github.io/articles/gettext-iconv-windows.html
5. NSIS (only for the installer): `choco install nsis`

Commands (x64 Native Tools Command Prompt for VS 2022):

    git clone https://github.com/<your-account>/OrcaSlicer.git QuasizeroSlicer
    cd QuasizeroSlicer
    git checkout quasizero-slicer-mvp

    :: 1) dependencies + application (first time; deps take 1-2 h)
    build_release_vs2022.bat

    :: subsequent rebuilds of the application only:
    build_release_vs2022.bat slicer

    :: 2) installer (optional)
    cd build
    cpack -G NSIS

Outputs:
- Portable application: `build\OrcaSlicer\` (contains `orca-slicer.exe`)
- Installer: `build\OrcaSlicer_Windows_Installer_*.exe`

## Baseline record (honesty note)

The unmodified upstream `v2.4.2` builds with exactly the commands above; this is
the same pipeline upstream uses for its release binaries (`build_all.yml`).
This fork was authored in a constrained Linux sandbox (no MSVC, 45 s process cap)
in which the full wxWidgets/OpenCASCADE application cannot be compiled; the
upstream baseline build and the Quasizero Windows build are therefore performed
on the CI runners above, not in the authoring sandbox. The QuasiZero core
modules and their unit/golden tests compile and run independently of the full
application (see `tests/qzmini/standalone/` and `TEST_REPORT.md`).

## Running the test suites

Full suites (Linux/CI): `cd build && ctest --output-on-failure`
QZmini core suite without the dependency tree:

    bash tests/qzmini/standalone/run_standalone.sh
