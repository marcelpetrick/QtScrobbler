# GitHub About

## Published description

QtScrobbler is an independent fork of the original SourceForge QTScrobbler, ported first to Qt 5 and then to Qt 6.12 and maintained for Linux as a desktop app and CLI for importing and scrobbling portable-player listening histories.

## Alternative descriptions

**Short.** Scrobble portable-player listening history from a Linux desktop app or CLI with this independent fork of the original SourceForge QTScrobbler, ported to Qt 5 and then Qt 6.12.

**Technical.** This independent GPLv2 fork of the original SourceForge QTScrobbler was ported to Qt 5 and then Qt 6.12 and is maintained on Linux, with GUI and CLI tools for importing log, iPod, and MTP listening histories.

## Ten discovery topics

`scrobbler`, `audioscrobbler`, `lastfm`, `portable-audio`, `ipod`, `mtp`,
`linux`, `qt6`, `cplusplus`, `command-line-tool`.

Swap candidates include `qt`, `cmake`, `music`, and `desktop-application`.
These describe the product and ecosystem; none claims achieved coverage, mutation
quality, live-service compatibility, or verified hardware support.

## Evidence

| Claim | Repository evidence |
| --- | --- |
| Original SourceForge fork and Qt ports | [Original project](https://qtscrob.sourceforge.net/), import commit `919faaf`, Qt 5 port commit `9286e99`, [changelog](CHANGELOG) |
| Qt 6.12 and maintained Linux build | [CMake configuration](../CMakeLists.txt), [dependency qualification](DEPENDENCIES.md), [README](../README.md) |
| Desktop and CLI | [GUI implementation](../src/qt/src/qtscrob.cpp), [CLI implementation](../src/cli/app.cpp) |
| Track review and submission | [GUI tests](../tests/test_gui.cpp), [submission contracts](../tests/test_submit.cpp) |
| Log / iPod / MTP import | [parsers](../src/lib/CMakeLists.txt), [architecture](ARCHITECTURE.md) |
| GPLv2 | [license](../LICENSE) |
| Native tests / coverage / mutation tooling | [quality commands](QUALITY.md), [mutation qualification](MUTATION_TESTING.md) |

The GitHub API confirmed the description and all ten topics on 2026-11-18.
