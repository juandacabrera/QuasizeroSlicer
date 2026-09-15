# Building Quasizero Slicer for Windows x64

Quasizero Slicer is a fork of OrcaSlicer and uses the unmodified upstream build
system. Base: upstream tag `v2.4.2`, commit `8500fcdccaa10b5099ac20d252af3a7c560046f1`,
branch `pro` (private repository `QuasizeroSlicer-dev`; the public LITE fork carries `main`).

## Option A — GitHub Actions (recommended, reproducible)

The fork workflow `.github/workflows/quasizero_build.yml` runs the standalone
tests and the Linux compile gate on every push to `pro` (the private integration
branch: LITE + PRO) and `feature/**`; the Windows build runs on demand (*Actions → Quasizero Build → Run
workflow*, inputs: build Windows yes/no, runner `windows-latest` or `self-hosted`,
Linux gate yes/no). It reuses OrcaSlicer's own chain (`build_check_cache.yml` →
`build_deps.yml` → `build_orca.yml`) and produces these artifacts:

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

    git clone --recurse-submodules https://github.com/juandacabrera/QuasizeroSlicer-dev.git QuasizeroSlicer
    cd QuasizeroSlicer
    git checkout pro
    git submodule update --init engine

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

Editions. The simulation engine (private repository `qz-sim-engine`) is the `engine/`
submodule; your GitHub account needs read access to it. When it is checked out CMake
builds the **PRO** edition (deformation view), otherwise the **LITE** edition (Stability
panel only) — the configure log says which (`Quasizero: PRO edition` / `LITE edition`).
To build LITE from the same checkout, to test what the public edition does, add
`-DQZ_PRO=OFF` to the CMake configure (the `.bat` passes no such option: run
`cmake -S . -B build-lite -DQZ_PRO=OFF ...` with the same generator and prefix path as
the script, or toggle `QZ_PRO` in the VS Code CMake Tools cache view).

## Option C — Your own Windows PC: local builds in VS Code and a self-hosted CI runner

Since the repository went private, GitHub-hosted runners are metered (Free plan:
2 000 min/month, Windows counts double, 500 MB of artifacts). The workflow now runs
only the standalone tests and the Linux compile gate on every push; the Windows build
is on demand (*Actions → Quasizero Build → Run workflow*) and can run on a
**self-hosted runner** — your own PC — with unlimited minutes. The same toolchain
serves local builds from VS Code.

### C1. Machine requirements

- Disk: about 30 GB free on an SSD (Visual Studio ~8 GB if not installed, sources
  ~2 GB, compiled dependencies ~10 GB, application build ~8–10 GB); 40 GB is
  comfortable. Path without spaces or accents, e.g. `C:\dev\QuasizeroSlicer`.
- RAM: 16 GB recommended (8 GB works with fewer parallel jobs; MSVC linking in
  Release is the peak). CPU: the more cores the better.
- Time: dependencies 1–2 h the first time only; full application build 30–45 min;
  incremental rebuilds minutes.

### C2. Install the toolchain (once)

1. Visual Studio 2022 Community with the **Desktop development with C++** workload
   (includes MSVC v143 and the Windows SDK).
2. CMake 3.31.x (https://cmake.org/download/, add to PATH). Not 4.x.
3. Git for Windows (https://git-scm.com).
4. gettext for Windows on PATH (https://mlocati.github.io/articles/gettext-iconv-windows.html).
5. Optional: NSIS for the installer (`choco install nsis`), Strawberry Perl if the
   deps step asks for it (`choco install strawberryperl`) — install Chocolatey first
   (https://chocolatey.org/install) if you want the CI steps to work unchanged.
6. Optional: VS Code with the *C/C++* and *CMake Tools* extensions.

### C3. Clone and build

Open **x64 Native Tools Command Prompt for VS 2022** (Start menu):

    cd C:\dev
    git clone --recurse-submodules https://github.com/juandacabrera/QuasizeroSlicer-dev.git QuasizeroSlicer
    cd QuasizeroSlicer
    git checkout pro
    git submodule update --init engine    :: the private simulation engine (PRO)

    build_release_vs2022.bat            :: first time: deps (1-2 h) + application
    build_release_vs2022.bat slicer     :: afterwards: application only

The portable application is in `build\OrcaSlicer\orca-slicer.exe`. In VS Code
open the folder; the CMake Tools extension picks up `build\` (kit: Visual Studio
Community 2022 Release - amd64) and lets you build the `OrcaSlicer` target and
run the QZmini tests (`ctest -R qz` in `build\`). The engine has its own tests:
`cmake -S engine -B engine\build && cmake --build engine\build && ctest --test-dir engine\build`
(or `bash engine/run_tests.sh` in Git Bash / WSL, g++ only).

The CI of the private repository needs two secrets (*Settings → Secrets and variables →
Actions*), each a fine-grained personal access token:
- `QZ_ENGINE_TOKEN`: *Contents: Read-only* on `QuasizeroSlicer-dev` and `qz-sim-engine`.
  Without it the checkout of `engine/` fails and the workflow stops at "Verify the
  simulation engine submodule".
- `QZ_PUBLIC_TOKEN`: *Contents: Read and write* **and** *Workflows: Read and write* on the
  public `QuasizeroSlicer`. The Sync LITE workflow pushes the LITE snapshot with it after
  every push to `pro`; GitHub refuses a push that changes a file under `.github/workflows/`
  unless the token has the Workflows permission.

### C4. Register the PC as a GitHub Actions runner

1. Repository → *Settings → Actions → Runners → New self-hosted runner* → Windows x64.
   GitHub shows the exact commands; they download the runner into a folder you
   choose (e.g. `C:\actions-runner`), configure it with a one-time token and start it.
   Keep the default label `self-hosted` (the workflow uses it) and add `windows`.
2. Install it as a service when asked (`./svc.sh` on Linux; on Windows the
   configuration script offers *Run as service*), so it starts with the PC.
3. Requirements on the PC: the toolchain of C2, Chocolatey (the deps step runs
   `choco install strawberryperl`), and the runner's user must be able to write to
   its work folder. Actions caches (`actions/cache`) also work on self-hosted
   runners — the dependency cache is restored from GitHub, so after the first run
   builds take ~30–45 min.
4. Run it: *Actions → Quasizero Build → Run workflow*, set *Runner for the Windows
   build* = `self-hosted`. The artifacts (portable zip, installer) appear on the run
   page as usual. Security note: a self-hosted runner executes the repository's
   workflows on your PC; keep the repository private and do not enable it for pull
   requests from forks.
5. Housekeeping: *Settings → Actions → General → Artifact and log retention*: set
   7 days to stay inside the 500 MB artifact quota.

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
