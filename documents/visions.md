# QtScrobbler product vision

QtScrobbler remains a GPLv2 native Qt application for importing portable-player
listening histories, inspecting and editing tracks, and submitting them to configured
scrobbling services through its graphical interface or command line.

The owner's requested initiative is to:

- Upgrade to an actually provisioned and exercised stable Qt 6.12 SDK.
- Review dependencies and record exact tested versions and reproducible provisioning.
- Preserve working GUI and CLI import, edit, timestamp, settings, submit and cancel flows.
- Provide deterministic offline tests without credentials or real-device access.
- Demonstrate at least 99% line coverage and 95% raw branch coverage separately over
  maintained first-party Linux production code, including CLI, GUI and MTP.
- Qualify native mutation testing, retain all outcomes and maintain at least 65%
  assertion kills with the current 77% ratchet and pursue the 80% goal (lowered from 95% by the owner on 2026-11-26); unexplained crashes and timeouts do not count as kills.
- Run the same complete native quality pipeline locally and in GitHub Actions, with
  release packaging and extracted-artifact qualification. Missing checks must fail.
- Keep setup, usage, architecture, API, dependency and quality documentation current,
  with truthful badges and a screenshot captured from the running application.
- Work on main in atomic conventional commits, update plan.md with each implementation
  commit, push after green checks, and inspect actual GitHub Actions outcomes.
- Finish an evidence-based review and repository About review, with all work pushed
  and a clean working tree.

Containers and GHCR are outside this initiative. GPLv2 is preserved. Merge, tag and
release publication require explicit authorization. Simulated device and loopback
HTTP tests must not be represented as real hardware or live-service qualification.

## Acceptance trace

plan.md is the execution record and requirements trace: P1 covers dependencies and
Qt; P2–P5 cover test isolation and product contracts; P6 and P7 cover measured coverage
and mutation; P8 covers the complete pipeline and documentation; P9 covers packaging,
review, actual Actions and final qualification. An unchecked criterion remains open
until commands and retained reports demonstrate it. Historical evidence stays historical.
