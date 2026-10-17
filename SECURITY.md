# Security Policy

Timing Graph Lab is a research and benchmarking tool, not a production service. Security
issues are still taken seriously, especially memory-safety bugs. For example, the
benchmark parses edge-list files given with `--input`, so a malformed file must never
cause out-of-bounds access.

## Supported versions

Only the latest commit on `main` receives fixes.

## Reporting a vulnerability

Please **do not open a public issue**. Use GitHub's private vulnerability reporting
instead: *Security → Report a vulnerability* on
<https://github.com/sc0210/timing-graph-lab/security/advisories/new>.

Please include the commit, the build type, the exact command and input file, and the
sanitizer output (an ASan or UBSan build from `build-asan` is ideal). You can expect an
acknowledgement within a week. Once a fix is available, the advisory will be published
with credit to you unless you prefer otherwise.
