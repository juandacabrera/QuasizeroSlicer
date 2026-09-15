#!/usr/bin/env bash
# Quasizero: export the LITE edition of a PRO commit as one snapshot commit.
#
#   scripts/quasizero/qz_export_lite.sh <pro-ref> <lite-branch> [<parent-ref>]
#
# The LITE tree is the tree of <pro-ref> without the paths listed in
# .github/qz_pro_paths.txt (as of <pro-ref>). The snapshot commit is put on <lite-branch>:
# its parent is the current <lite-branch> head when the branch exists, otherwise
# <parent-ref> (default: the OrcaSlicer v2.4.2 base, so a first export reads as
# "OrcaSlicer v2.4.2 + Quasizero Slicer LITE"). Nothing is done when the LITE tree did not
# change. Works from a shallow clone as long as <pro-ref> and the parent are present
# (git fetch --depth 1 origin <sha>). The working tree and the normal index are untouched.
set -euo pipefail
src=${1:?usage: qz_export_lite.sh <pro-ref> <lite-branch> [<parent-ref>]}
dst=${2:?usage: qz_export_lite.sh <pro-ref> <lite-branch> [<parent-ref>]}
base=${3:-8500fcdccaa10b5099ac20d252af3a7c560046f1}
cd "$(git rev-parse --show-toplevel)"

src_sha=$(git rev-parse --verify "$src^{commit}")
tmp_index=$(mktemp)
trap 'rm -f "$tmp_index"' EXIT
export GIT_INDEX_FILE=$tmp_index
git read-tree "$src_sha"
git show "$src_sha:.github/qz_pro_paths.txt" | while IFS= read -r p; do
    [ -z "$p" ] && continue
    case "$p" in \#*) continue ;; esac
    git ls-files -z -- "$p"
done | git update-index -z --force-remove --stdin
tree=$(git write-tree)
unset GIT_INDEX_FILE

if git show-ref --verify --quiet "refs/heads/$dst"; then
    parent=$(git rev-parse "refs/heads/$dst"); old_ref=$parent
else
    parent=$(git rev-parse --verify "$base^{commit}"); old_ref=""
fi
if [ "$(git rev-parse "$parent^{tree}")" = "$tree" ]; then
    echo "LITE tree unchanged since $(git rev-parse --short "$parent"); nothing to export"
    exit 0
fi
subject=$(git log -1 --format=%s "$src_sha")
message=$(printf 'lite: %s\n\nQuasizero Slicer LITE snapshot of %s (PRO-only paths removed, see .github/qz_pro_paths.txt).\n' "$subject" "$src_sha")
commit=$(printf '%s' "$message" | git commit-tree "$tree" -p "$parent")
git update-ref "refs/heads/$dst" "$commit" "$old_ref"   # compare-and-swap on the branch head
echo "$dst -> $(git rev-parse --short "$commit") (parent $(git rev-parse --short "$parent"), tree of $(git rev-parse --short "$src_sha"))"
