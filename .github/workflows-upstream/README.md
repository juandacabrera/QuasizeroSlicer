# Upstream workflows, parked

OrcaSlicer's own GitHub Actions workflows and Dependabot configuration, moved out of
`.github/workflows/` in the private Quasizero development repository: they serve the
upstream project (issue triage bots, release publishing, nightly multi-platform builds,
profile and locale checks on pull requests to `main`, daily shellcheck) and in a private
repository they only fail (`permissions: {}` checkouts cannot read a private repository)
or spend the GitHub Free minutes. Nothing here is modified; move a file back to
`.github/workflows/` to re-enable it. The public Quasizero Slicer LITE repository keeps
the upstream set as it is.

Kept active: `quasizero_build.yml` and the reusable `build_check_cache.yml`,
`build_deps.yml`, `build_orca.yml` it calls.
