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

## Option C — Your own Windows PC: the development environment (two editions, one checkout)

This is the day-to-day setup. It exists for two reasons: GitHub-hosted runners of the
private repository are metered (Free plan: 2 000 min/month, Windows counts double, and
a Windows build from scratch is ~4 h of runner time), and a CI round trip for a small
UI change takes hours whereas an incremental local rebuild takes minutes. The same PC
can also serve as a **self-hosted runner**, so the on-demand CI builds run on it with
unlimited minutes (C6).

One checkout gives both editions, from two build directories:

| Flavour | Build dir | CMake | What it is |
|---|---|---|---|
| **PRO integrated** | `build\` | `QZ_PRO=ON` (automatic when `engine/` is present) | one executable with the deformation view compiled in — the "everything in one Orca-based tool" edition; also the fallback if the external-engine design is ever dropped |
| **LITE** | `build-lite\` | `QZ_PRO=OFF` | what the public repository ships: Stability panel only; the PRO extension arrives through the separate, licensed `qz-sim` engine when installed |
| **Engine** | `engine\build\` | — | `qz-sim.exe` (+ its tests), pure C++17, builds in minutes |

Both slicer flavours share the compiled dependencies (`deps\build\OrcaSlicer_dep`), so
the 1–2 h dependency build happens once.

### C1. Machine requirements

- Disk: about 40 GB free on an SSD (Visual Studio ~8 GB, sources ~2 GB, compiled
  dependencies ~10 GB, each application build ~8–10 GB). Path without spaces or accents,
  e.g. `C:\dev\QuasizeroSlicer`.
- RAM: 16 GB recommended (8 GB works with fewer parallel jobs; MSVC linking in Release is
  the peak). CPU: the more cores the better.
- Time: dependencies 1–2 h the first time only; full application build 30–45 min;
  incremental rebuild of a GUI change 2–10 min.

### C2. Install the toolchain (once)

1. Visual Studio 2022 Community with the **Desktop development with C++** workload (MSVC
   v143 + Windows SDK).
2. CMake 3.31 or 4.x (https://cmake.org/download/, add to PATH). The build script and the
   presets set `CMAKE_POLICY_VERSION_MINIMUM=3.5`, which CMake 4 needs for this tree.
3. Git for Windows (https://git-scm.com). Sign in once when Git asks (Git Credential
   Manager): your account has access to the two private repositories.
4. gettext for Windows on PATH (https://mlocati.github.io/articles/gettext-iconv-windows.html)
   — the build script runs `scripts\run_gettext.bat`.
5. Strawberry Perl (needed by the OpenSSL step of the dependency build):
   `choco install strawberryperl` after installing Chocolatey (https://chocolatey.org/install),
   or the installer from https://strawberryperl.com. Keep `C:\Program Files\CMake\bin` before
   `C:\Strawberry\c\bin` in PATH (CMake refuses to configure otherwise).
6. Optional: NSIS for installers (`choco install nsis`); 7-Zip for the portable zip.
7. VS Code with the *C/C++* and *CMake Tools* extensions (recommended: the repository
   carries `CMakePresets.json`, so VS Code offers the two flavours as presets).

### C3. Clone and build the dependencies (once)

Open **x64 Native Tools Command Prompt for VS 2022** (Start menu):

    cd C:\dev
    git clone --recurse-submodules https://github.com/juandacabrera/QuasizeroSlicer-dev.git QuasizeroSlicer
    cd QuasizeroSlicer
    git submodule update --init engine    :: the private simulation engine (PRO)
    build_release_vs2022.bat deps         :: 1-2 h, once

`deps\build\OrcaSlicer_dep` is then reused by every build below; rebuild it only when
`deps\` changes upstream.

### C4. Build the two editions

PRO integrated (same as the script; `build\`):

    build_release_vs2022.bat slicer       :: full build, 30-45 min
    build\OrcaSlicer\orca-slicer.exe      :: the portable application (folder build\OrcaSlicer)

LITE (`build-lite\`), with the presets:

    cmake --preset win-lite
    cmake --build --preset win-lite
    cmake --build --preset win-lite-install
    build-lite\OrcaSlicer\orca-slicer.exe

(the same for PRO with `win-pro`, `win-pro-install`; the presets are what VS Code's
CMake Tools shows in its status bar: pick the configure preset, then *Build*.)

Engine (`qz-sim.exe` and its 31 tests):

    cmake -S engine -B engine\build -G "Visual Studio 17 2022" -A x64
    cmake --build engine\build --config Release
    ctest --test-dir engine\build -C Release --output-on-failure
    engine\build\Release\qz-sim.exe --help

### C5. The daily loop (this is where the hours go back)

1. Edit sources (VS Code). For the simulation engine edit `engine\src\...`; the PRO
   integrated build compiles those files directly, no copy step.
2. Rebuild only the application target and refresh the portable folder:

       cmake --build --preset win-pro-app        :: or win-lite-app
       cmake --build --preset win-pro-install    :: copies exe + resources into build\OrcaSlicer

   A one-file GUI change takes 2–10 min (compile + link); resources (icons, profiles,
   `resources\`) need only the install step.
3. Run `build\OrcaSlicer\orca-slicer.exe`. Logs: `%APPDATA%\QuasizeroSlicer\log`
   (`--log-level 4`) if something misbehaves.
4. Tests: engine — `ctest --test-dir engine\build -C Release`; slicer core (QZmini
   modules) — `bash tests\qzmini\standalone\run_standalone.sh` from Git Bash or WSL with
   g++ installed (the CI runs the same script on every push, so skipping it locally is
   acceptable for GUI-only changes).
5. Commit on `pro` (or a `feature/...` branch) and push. The push runs the tests and the
   Linux compile gate (~40 min with the dependency cache; add `[skip gate]` to the commit
   subject for docs-only changes — never quote that marker in a commit body, the filter
   reads the whole message) and republishes the LITE edition in the public repository,
   which builds the public LITE portable for free. PRO portables come from your PC.

### C6. Register the PC as a GitHub Actions runner (on-demand PRO builds without minutes)

1. Repository → *Settings → Actions → Runners → New self-hosted runner* → Windows x64.
   GitHub shows the exact commands; they download the runner into a folder you choose
   (e.g. `C:\actions-runner`), configure it with a one-time token and start it. Keep the
   default label `self-hosted` (the workflow uses it) and add `windows`.
2. Install it as a service when the configuration script offers *Run as service*, so it
   starts with the PC.
3. Repository → *Settings → Secrets and variables → Actions → Variables*: add
   `SELF_HOSTED` = `true`. The upstream build steps then skip `choco install`
   (strawberryperl, nsis) and use what is installed on the PC.
4. Run it: *Actions → Quasizero Build → Run workflow*, *Runner for the Windows build* =
   `self-hosted`. Artifacts (portable zip, installer) appear on the run page as usual.
   The dependency cache is restored from GitHub's cache on the first run, so builds take
   ~30–45 min from then on.
5. Security: a self-hosted runner executes the repository's workflows on your PC; keep the
   repository private and never enable it for pull requests from forks.
6. Housekeeping: *Settings → Actions → General → Artifact and log retention* = 7 days
   (500 MB artifact quota).

### C7. Secrets of the private repository (already created)

- `QZ_ENGINE_TOKEN`: fine-grained PAT, *Contents: Read-only* on `QuasizeroSlicer-dev` and
  `qz-sim-engine`. Without it the checkout of `engine/` fails and the workflow stops at
  "Verify the simulation engine submodule".
- `QZ_PUBLIC_TOKEN`: fine-grained PAT, *Contents: Read and write* **and** *Workflows: Read
  and write* on the public `QuasizeroSlicer`. The Sync LITE workflow pushes the LITE
  snapshot with it after every push to `pro`; GitHub refuses a push that changes a file
  under `.github/workflows/` unless the token has the Workflows permission.

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
