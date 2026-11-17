# Current-state review

Reviewed 2026-11-17 using the reviewBranch skill. Resolved integration branch:
`main` / `origin/main`, merge-base `dd8c51c`, HEAD `dd8c51c`. The committed
three-dot diff is empty. At the owner's request the supplementary review covers
the pending working changes and relevant initiative tooling introduced since
`origin/master` at `c3fbd8f`.

## Verified findings

1. **HIGH · Code · tools/install_mutation_toolchain.py:70.** An unrelated nonempty
   prefix is accepted and archive extraction overwrites its files. A temporary,
   checksum-valid Debian fixture replaced an existing `usr/bin/clang-20`.
   Fix: reject unrecognized nonempty prefixes before writes and stage installation.
2. **MEDIUM · Code · localPipeline.sh:521.** Combining `--quick --baseline` exits
   successfully while skipping quality stages, contrary to baseline's all-stage
   contract. Reproduced through the disposable pipeline harness.
   Fix: reject conflicting modes independent of argument order.

## Resolution and verdict

Both findings are fixed. The installer stages new installations and rejects unrelated
prefixes; its six offline native-fixture tests pass. Pipeline modes are mutually
exclusive; all six conflicting argument orders fail before building or writing reports.
The complete pipeline harness passes nine tests. The previous corrupt-cache contract
now expects the earlier prefix rejection; fresh-download checksum failures remain
covered independently.

The supplementary runtime review found no verified introduced defect in the inspected
CLI, parser, storage, network, and controller changes. Removed iPod skip guards retain
complete-block bounds validation. The retry/source-cleanup bug found during continued
behavior work predates the initiative and is tracked in the execution plan.

These fixes resolve the two tooling findings. Final qualification still requires fresh
coverage, corrected native mutation accounting, package checks, and actual green CI;
this review does not approve a release.
