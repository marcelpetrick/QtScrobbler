# Qt Scrobbler

Initially a clone/fork from the SourceForge project by Tomasz Mon, Robert Keevil and others.
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

- Qt >= 6.x (tested with Qt 6.11.1)
- A C++17 capable compiler (GCC 10+ or Clang 12+)
- `qmake` pointing to Qt6 (verify with `qmake --version`)

On Arch/Manjaro `qmake` already resolves to Qt6.
On Debian/Ubuntu install `qt6-base-dev` and use `qmake6`.

---

## How to build

### most simple: run localPipeline.sh ..

```sh
..
============ Local Pipeline Summary ============
 QtScrobbler v0.13.2
Qt6 Check              : PASS Qt 6.11.1 at /usr/bin/qmake6
Build Tools            : PASS make, g++, pkg-config present; 20 compile jobs
Build: library         : PASS libscrobble.a (672K)
Translations (.qm)     : PASS .qm files generated
Build: GUI             : PASS qtscrob (576K)
Build: CLI             : PASS scrobbler (340K)
Smoke: CLI             : PASS --help exited 0 — usage text printed
Smoke: GUI binary      : PASS /home/mpetrick/repos/QtScrobbler/src/qt/qtscrob — 576K
Translations           : PASS de: 79/0 untranslated;  pl: 79/0 untranslated — total: 158 strings, 0 untranslated
Unit Tests             : SKIP No QTest suite — add src/tests/ to enable
Launch App             : PASS qtscrob started (detached)
================================================
```

### Full build (library + GUI + CLI)

```sh
cd src
qmake
make -j$(nproc)
```

Binaries are written to `src/qt/` (GUI: `qtscrob`) and `src/cli/` (CLI: `scrobbler`).

### GUI only

```sh
cd src/qt
qmake
make -j$(nproc)
```

### CLI only

```sh
cd src/cli
qmake
make -j$(nproc)
```

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
