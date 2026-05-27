# Qt Scrobbler

Initially a clone/fork from the SourceForge project by Tomasz Mon, Robert Keevil and others. Now maintained by Marcel Petrick.  
The code has since been adapted for Qt5 and is now being ported to **Qt6**.  
QTScrobbler ships both a multiplatform GUI and a CLI version.  

Primary supported platform: **Linux**.  
macOS and Windows support is not actively maintained.

Optional MTP support requires `libmtp-dev` and `pkg-config`:

```sh
sudo apt-get install libmtp-dev pkg-config   # Debian/Ubuntu
sudo pacman -S libmtp pkgconf                # Arch/Manjaro
```

---

## Requirements

- Qt >= 6.8 (tested with Qt 6.11.1)
- CMake >= 3.28
- A C++20 capable compiler (GCC 12+ or Clang 14+)
- `cmake` and `pkg-config`

On Arch/Manjaro all of the above are available via `pacman`.  
On Debian/Ubuntu 22.04+ install `qt6-base-dev qt6-tools-dev qt6-tools-dev-tools cmake`.

---

## How to build

### most simple: run localPipeline.sh

```sh
./localPipeline.sh --noRun
============ Local Pipeline Summary ============
 QtScrobbler v0.13.12
Qt6 Check              : PASS Qt 6.11.1 at /usr/bin/qmake6
Build Tools            : PASS cmake, g++, pkg-config present; 20 compile jobs
CMake configure        : PASS configured in /path/to/QtScrobbler/build
CMake build            : PASS libscrobble.a (556K); qtscrob; scrobbler
Build: GUI             : PASS qtscrob (584K)
Build: CLI             : PASS scrobbler (340K)
Smoke: CLI             : PASS --help exited 0 — usage text printed
Smoke: GUI binary      : PASS /path/to/QtScrobbler/build/qtscrob — 584K
Translations           : PASS de: 79/0 untranslated;  pl: 79/0 untranslated — total: 158 strings, 0 untranslated
Unit Tests             : SKIP No QTest suite — add src/tests/ to enable
Launch App             : SKIP Suppressed by --noRun
================================================
```

### Full build (out-of-tree, all targets)

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

Binaries are written to `build/` (GUI: `build/qtscrob`, CLI: `build/scrobbler`).

---

## How to run

### GUI

```sh
./src/qt/qtscrob
# Optional flags:
#   -c PATH   path to configuration file
#   -v LEVEL  verbosity (0=error … 3=trace)
```

### CLI

```sh
./src/cli/scrobbler --help
./src/cli/scrobbler -f /path/to/.scrobbler.log
```

---

## Qt6 port status

See [`port_to_qt6.md`](port_to_qt6.md) for the full compatibility assessment and list of required changes.

---

# last.fm

Last.fm stopped support for groups and closed all existing ones.
GitHub issues are the place to exchange ideas and report bugs.
