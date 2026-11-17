# Contribution and execution rules

## Current initiative

Read `plan.md` before changing this repository. The authorized initiative is Qt 6.12,
dependency review, working CLI/GUI behavior, at least 95% line and branch coverage, and
native mutation testing. The percentages are targets until reports demonstrate them.

The initial commit contains the plan and this file only. Then execute the plan in atomic
commits. **Every implementation commit must update `plan.md`**: mark completed checklist
items, record commands/results and measured numbers, describe what is left, and retain
blockers. Never mark a package complete on partial evidence. Keep this file current when
commands or contribution rules change; it need not change in every implementation commit.

## Scope and working method

- Inspect current code and call sites before editing; preserve public behavior with tests.
- Work autonomously within the requested scope; no push, merge, tag, or release publication
  without explicit authorization. Local atomic commits are authorized.
- Keep dependency, mechanical formatting, test infrastructure, and behavioral fixes separate.
- Use conventional commit subjects and explain why in the body; no tool/AI attribution.
- Do not adopt every unrelated TODO or the Python playbook's version-bump policy automatically.
  A repository-local pre-commit hook already bumps `CLIENT_VERSION` in `src/lib/common.h`
  on every commit; retain it for implementation commits and rebuild before runtime validation.
  The initial documentation-only planning commit intentionally excluded this automatic bump.
- Use the applicable `updateDependencies` skill for dependency work: verify upstream stable
  versions, pin exact tested resolutions, run the complete gate and commit only after it passes.
- Use a project prefix or reproducible environment; do not upgrade the user's OS globally.
- Do not add agent delegation unless explicitly requested or required by an applicable skill.

## Build and test commands

Current gate: `./localPipeline.sh --noRun`. Missing/failed tests and CLI smoke failures are fatal. This baseline gate does not yet
enforce the final coverage/mutation targets. Retain evidence with
`./localPipeline.sh --noRun --noOpen --report-dir /tmp/qtscrobbler-reports`.
Use `QTSCROBBLER_JOBS=4` to bound build parallelism and `QTSCROBBLER_BUILD_DIR`
for an isolated build directory.

Current direct checks:

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --no-tests=error
timeout 5 ./build/scrobbler --help
```

Check executable provenance: the initial environment has a broken `~/.local/bin/ctest`
wrapper while `/usr/bin/ctest` works. Prefer a consistent verified CMake/CTest toolchain;
do not silently substitute a different Qt version when the requested version is unavailable.
Replace these instructions with the qualified preset/pipeline commands as they are implemented.

## Testing and measurement policy

- Use Qt Test, QSignalSpy, CTest and real Qt event loops for C++ behavior.
- Default tests are deterministic, offline, headless and bounded by timeouts. Isolate
  HOME/XDG settings, databases, files, clocks, credentials, device access and HTTP endpoints.
- The example scrobble log contains fictional data; tests and demos that clear or alter it
  operate on a temporary copy. Never consume real credentials or submit real tracks by default.
- Assert exact fields, signals, transitions, errors and collaborator arguments, including
  failure and boundary paths. Avoid tests that only execute methods or duplicate implementation.
- Join workers, drain deferred deletion and release SQL/network resources. Repeat lifecycle tests.
- Treat unexpected Qt warnings/crashes as failures; document narrow platform allowances.
- Cover maintained first-party production GUI/CLI/library/MTP code. Publish the denominator,
  unexecuted files, raw branch semantics and exclusions. Do not game 95% with broad exclusions,
  rounding, hidden files, or replacement of branch coverage by a different metric.
- Mutation reports must distinguish kills, survivors, uncovered mutants, invalid mutants,
  timeouts, crashes and infrastructure errors. Do not count unexplained timeouts/crashes as kills.
- Prove survivor fixes against the specific mutant and the original. Exclude only reviewed
  equivalents/unreachable code with reasons; retain scope/operator/tool fingerprints.
- Missing tests, missing tools, failed mandatory stages and stale/missing reports fail the gate.
  Optional live/device tests and optional launches must be labelled accurately.

## Review, documentation and completion

Review changed code against the integration branch with the newest merge-base among
main/master/develop. Read touched files and callers; audit removed guards separately.
Report verified findings only, grouped as Code or Architecture, with severity, failure
scenario, file/line evidence, and concrete fix. Resolve findings and give a supported verdict.

Keep README, dependency inventory, architecture, API and quality documentation aligned with
implemented behavior. Historical reports remain historical; append new measured results.
Do not claim green CI, Qt 6.12 runtime, device compatibility, 95% coverage, or mutation success
without actually observing the corresponding checks. Final completion includes package and
GUI/CLI workflow evidence, not just compilation.
