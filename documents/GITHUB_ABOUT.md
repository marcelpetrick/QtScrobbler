# GitHub About proposal

Current public description observed on 2026-11-17:
“Resurrected hard fork of the QtScrobble-project from Sourceforge. Updated from Qt4 to 5 and now 6.11!”

Current topics: `cplusplus`, `lastfm`, `qmake`, `qt`, `qt6`, `scrobble`, `usb`.
The Qt version and qmake emphasis are stale; CMake is the qualified build path.

## Recommended description

A Qt 6.12 desktop app and CLI for reviewing portable-player listening histories and submitting tracks to configured scrobbling services, with log, iPod, and MTP import.

Short alternative: Review and scrobble portable-player listening histories with a Qt 6.12 desktop app and CLI.

Technical alternative: A GPLv2 C++20/Qt 6.12 scrobbler with GUI and CLI frontends, log/iPod/MTP parsers, and native Qt Test, LLVM coverage, and Mull tooling.

## Ten discovery topics

`scrobbler`, `lastfm`, `audioscrobbler`, `portable-audio`, `ipod`, `mtp`,
`qt6`, `cplusplus`, `desktop-application`, `command-line-tool`.

Swap candidates: `qt`, `cmake`, `music`, `linux`, `qt-test`.
These describe the product and ecosystem; none claims achieved coverage, mutation
quality, live-service compatibility, or verified hardware support.

## Evidence

| Claim | Repository evidence |
| --- | --- |
| Qt 6.12 / C++20 | [CMake configuration](../CMakeLists.txt), [dependency qualification](DEPENDENCIES.md) |
| Desktop and CLI | [GUI implementation](../src/qt/src/qtscrob.cpp), [CLI implementation](../src/cli/app.cpp) |
| Track review and submission | [GUI tests](../tests/test_gui.cpp), [submission contracts](../tests/test_submit.cpp) |
| Log / iPod / MTP import | [parsers](../src/lib/CMakeLists.txt), [architecture](ARCHITECTURE.md) |
| GPLv2 | [license](../LICENSE) |
| Native tests / coverage / mutation tooling | [quality commands](QUALITY.md), [mutation qualification](MUTATION_TESTING.md) |

The proposal is recorded for review; public About metadata has not been changed.
