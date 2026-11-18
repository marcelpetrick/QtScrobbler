# Contribution and execution rules

## Current initiative

Read `documents/plan.md` before changing this repository. The authorized initiative is Qt 6.12,
dependency review, working CLI/GUI behavior, at least 99% line and 95% branch coverage, and
native mutation testing. The percentages are targets until reports demonstrate them.

The initial commit contains the plan and this file only. Then execute the plan in atomic
commits. **Every implementation commit must update `documents/plan.md`**: mark completed checklist
items, record commands/results and measured numbers, describe what is left, and retain
blockers. Never mark a package complete on partial evidence. Keep this file current when
commands or contribution rules change; it need not change in every implementation commit.

## Scope and working method

- Inspect current code and call sites before editing; preserve public behavior with tests.
- Work autonomously on `main`. The owner authorizes continuous pushes after green checks.
  No merge, tag or release publication without explicit authorization. Local atomic commits
  and pushes are authorized; inspect actual GitHub Actions results rather than claiming green CI.
- Keep dependency, mechanical formatting, test infrastructure, and behavioral fixes separate.
- Use conventional commit subjects and explain why in the body; no tool/AI attribution.
- Do not adopt every unrelated TODO or the Python playbook's version-bump policy automatically.
  A repository-local pre-commit hook already bumps `CLIENT_VERSION` in `src/lib/common.h`
  on every commit. Execute that hook before the full gate so the tested source version
  matches the commit, then disable only that already-executed hook for the commit invocation
  (`git -c core.hooksPath=/dev/null commit`) to avoid a second bump. Significant new product
  features bump the minor version; ordinary fixes, tests and docs bump the patch.
  The initial documentation-only planning commit intentionally excluded this automatic bump.
- Use the applicable `updateDependencies` skill for dependency work: verify upstream stable
  versions, pin exact tested resolutions, run the complete gate and commit only after it passes.
- Use a project prefix or reproducible environment; do not upgrade the user's OS globally.
- The owner explicitly authorized subagents for this initiative. Use subagents where useful
  for independent investigations, tests, CI/docs, and review. Assign disjoint file ownership,
  keep shared CMake/plan integration coordinated by the primary agent, and verify returned
  evidence. Subagents must not commit, push, or mutate shared build directories independently.

## Build and test commands

The default complete gate is `./localPipeline.sh --noRun --noOpen --report-dir <new-dir>`.
Provision the pinned SDK, Python tools, native LLVM/Mull environment and zlib prefix using
`documents/DEPENDENCIES.md`; install the qualified private native call-removal correction, source its generated
`env.sh`, and set
`QTSCROBBLER_QT_PREFIX` and `ZLIB_ROOT` explicitly. Formatting, static/docs checks,
ASan/UBSan, separate 99% line and 95% branch coverage, the native mutation ratchet
(`MUTATION_MINIMUM`, currently 77%, owner floor 65%, goal 95%) and extracted-package qualification are
mandatory in this command. Raise the ratchet when tests kill more mutants; never lower
it without the owner's decision. Missing tools/reports and failed stages fail.
Test, mutation and package processes run through `tools/run_offline.py`; Linux network
namespaces, `unshare`, `ip` and `dbus-daemon` are mandatory and fail closed.

`--quick` is an explicit build/test diagnostic subset for incremental atomic implementation
commits while quality targets remain open. It never qualifies final completion.
`--baseline` runs all stages but labels incomplete coverage/mutation as measurements;
it is not release qualification. Record which mode was actually run in `documents/plan.md`.
Use `QTSCROBBLER_JOBS=4` to bound parallelism and `QTSCROBBLER_BUILD_DIR` for an isolated
normal build. Instrumented builds use separate coverage/sanitizer/mutation directories.
Full reports must start fresh; keep previous reports as historical evidence.

Current direct checks:

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --no-tests=error
timeout 5 ./build/scrobbler --help
```

Check executable provenance: the initial environment has a broken `~/.local/bin/ctest`
wrapper while `/usr/bin/ctest` works. Prefer a consistent verified CMake/CTest toolchain;
do not silently substitute a different Qt version when the requested version is unavailable.
Replace these instructions with the qualified preset/pipeline commands as they are implemented.

## Public CI, badges and releases

- No public failures: before pushing, run the local gate that matches the change
  (at least `--quick` on the exact staged snapshot; the complete gate before release).
  Never push while a qualifying CI run is in progress on `main`: the workflow's
  concurrency group cancels it. Batch local commits and push after it finishes.
- A failed or cancelled run on `main` is the first priority; inspect its log with
  `gh run view <id> --log-failed`, fix the cause, and confirm the next run passes.
- README badges must be green from real results: CI on `main`, the Release workflow,
  the latest release and the measured coverage values from the newest report.
- Releases: only publish commits whose CI run on `main` passed (pretested). Tag
  `v<CLIENT_VERSION>` on that commit; `release.yml` refuses a tag without a successful
  CI run on its commit or a tag that differs from `src/lib/common.h`. It only builds,
  runs the unit tests, packages, smoke-tests and publishes. Mutation testing and the
  other full-gate stages stay in CI and local qualification; never add them to the
  release workflow (owner decision: too much overhead per release).
- Deleting workflow runs or releases cannot be undone; ask the owner first.
- `/tmp` on the development host is a 16 GB RAM-backed tmpfs. Put complete-gate
  build, report and `TMPDIR` directories on disk (for example
  `~/.cache/qtscrobbler-runs/<version>`); a full run filled `/tmp` and aborted.
  Remove superseded build directories when a newer snapshot replaces them.
- Put `/path/to/cmake-4.5.0/bin` first on `PATH`; `~/.local/bin/ctest` is a broken
  wrapper on the development host and fails the CTest stage.

## Resuming a session or switching agents

- Read the newest handoff section at the top of `documents/plan.md` first.
  It records active local gate directories, the prepared index and the
  release order. Check `git status --short --branch`, `git log origin/main..`
  and `gh run list --workflow CI --branch main --limit 5`. Update both handoff
  files when priorities, exact evidence, or resume commands change.
- Run long complete gates detached (`setsid nohup <run_dir>/run.sh`) with a
  `run.sh` that records the environment and appends `EXIT=<status>` to the log.
  Gates attached to an agent session die with it and yield no evidence.
- Releases v0.13.85 and v0.14.0 are published and verified. A tag push runs
  the Release workflow of the tagged commit; to publish an older qualified tag
  with a corrected workflow, cancel that run before it publishes and dispatch
  `gh workflow run release.yml --ref main -f tag=vX.Y.Z`. Never push while a
  qualifying main CI run is active.
- On the development host, the provisioned tools live in `/tmp` (tmpfs), so they
  are lost on reboot: Qt `/tmp/qtscrobbler-qt/6.12.0/gcc_64`, CMake
  `/tmp/qtscrobbler-cmake/cmake-4.5.0-linux-x86_64/bin`, zlib
  `/tmp/qtscrobbler-zlib-1.4-qualified`, LLVM/Mull correction
  `/tmp/qtscrobbler-mull-callfix-qualified2/env.sh`. If any is missing, provision it
  again following `documents/DEPENDENCIES.md`.
- For the 0.14.0 qualification, use the private C++23-capable Clazy 1.17.1 and
  Cppcheck 2.22.0 prefixes listed in the plan and dependency documentation.
  For Clazy, set `QTSCROBBLER_CLAZY_EXTRA_ARGS="-resource-dir=$("$CC" --print-resource-dir)"`
  after sourcing the qualified LLVM environment. Keep its loader paths.
- Exact staged or committed snapshot gate. Use a new directory each time and
  keep the source, temporary data, reports, and instrumented builds on disk.
  On this host the original runner used by 0.13.76 needs a short `TMPDIR`.
  Remove `--quick` for the complete release qualification.

  ```sh
  snapshot=$(git write-tree)  # Use a commit SHA to qualify a committed tree.
  run_dir=$(mktemp -d "$HOME/.cache/qs-gate.XXXXXX")
  mkdir "$run_dir/source" "$run_dir/tmp"
  git archive "$snapshot" | tar -x -C "$run_dir/source"
  . /tmp/qtscrobbler-mull-callfix-qualified2/env.sh
  export PATH="/tmp/qtscrobbler-cmake/cmake-4.5.0-linux-x86_64/bin:/tmp/qtscrobbler-aux-tools-current/bin:$HOME/.cache/qtscrobbler-python-probe-3152/quality-tools/bin:$PATH"
  export QTSCROBBLER_QT_PREFIX=/tmp/qtscrobbler-qt/6.12.0/gcc_64
  export ZLIB_ROOT=/tmp/qtscrobbler-zlib-1.4-qualified QTSCROBBLER_JOBS=4
  export QTSCROBBLER_BUILD_DIR="$run_dir/build" TMPDIR="$run_dir/tmp"
  cd "$run_dir/source"
  ./localPipeline.sh --quick --noRun --noOpen --report-dir "$run_dir/reports" > "$run_dir/log" 2>&1
  ```

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
  unexecuted files, raw branch semantics and exclusions. Do not game thresholds with broad exclusions,
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
Do not claim green CI, Qt 6.12 runtime, device compatibility, coverage, or mutation success
without actually observing the corresponding checks. Final completion includes package and
GUI/CLI workflow evidence, not just compilation.

The exact Python tooling resolution is in `requirements-quality.txt`; upstream version
review and SDK provisioning are documented in `documents/DEPENDENCIES.md`. The Linux Qt
SDK requires its ICU archive. The system Qt 6.11 installation is below the new minimum.

## Owner's final product workflow

- There was no `documents/visions.md` in the initial checkout. Record the explicit requested vision,
  trace its requirements in `documents/plan.md`, implement them, and audit each against evidence.
- Preserve GPLv2. The owner explicitly withdrew the generic instruction to adopt GPLv3.
- Do not add Docker/GHCR workflows, container checks or Docker documentation. Use native tools.
- Add real README badges, setup/testing/quality/usage documentation and a screenshot captured
  from the running Qt UI. Never use a mockup as product evidence.
- Run `reviewBranch`, fix verified findings, then run `githubAbout` before completion.
- Completion requires the whole vision, 99% line and 95% branch coverage, mutation evidence,
  complete native pipeline, actual green Actions, pushed work and a clean working tree.
- If there is a genuine external blocker, document precise evidence and continue independent
  tasks; do not claim that an unavailable check passed or quietly drop a requirement.
