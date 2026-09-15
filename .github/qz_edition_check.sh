#!/usr/bin/env bash
# Quasizero: which edition is this tree, and is it consistent?
#  - PRO  (a .gitmodules declaring engine/): the submodule must be checked out, otherwise
#    CMake would silently build the LITE edition.
#  - LITE (no engine/ submodule declared): none of the PRO-only paths listed in
#    .github/qz_pro_paths.txt may exist and no source may name the engine's classes -
#    the leak guard of the public repository.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -f .gitmodules ] && grep -q 'path = engine' .gitmodules; then
    if [ -f engine/src/QzStackSim.cpp ]; then
        echo "PRO edition: engine/ checked out at $(git -C engine rev-parse --short HEAD 2>/dev/null || echo '?')"
    else
        echo "::error::engine/ submodule is declared but not checked out. Add the repository secret QZ_ENGINE_TOKEN (fine-grained PAT with Contents: read on qz-sim-engine and this repository)."
        exit 1
    fi
    exit 0
fi
echo "LITE edition: no engine/ submodule declared - running the leak guard"
bad=0
while IFS= read -r p; do
    [ -z "$p" ] && continue
    case "$p" in \#*) continue ;; esac
    if [ -e "$p" ]; then
        echo "::error::PRO-only path present in a LITE tree: $p"
        bad=1
    fi
done < .github/qz_pro_paths.txt
# engine classes must not be named by any LITE source (comments included: nothing of the
# engine belongs here)
if grep -rlE 'QzStackSim|QzSkeleton|QzBeadSim|QzTubeMesher|QzJob' --include='*.cpp' --include='*.hpp' --include='*.h' src tests 2>/dev/null | grep .; then
    echo "::error::LITE sources name simulation-engine classes (see the files above)"
    bad=1
fi
if [ "$bad" -ne 0 ]; then exit 1; fi
echo "LITE tree clean"
