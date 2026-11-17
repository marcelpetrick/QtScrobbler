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

- Qt >= 6.12 (qualified build: Qt 6.12.0)
- CMake >= 3.28
- A C++20 capable compiler (GCC 12+ or Clang 14+)
- `cmake` and `pkg-config`

On Arch/Manjaro all of the above are available via `pacman`.  
On Debian/Ubuntu, distribution Qt packages may be older than 6.12. Install the pinned
Qt SDK separately; see [dependency inventory](documents/DEPENDENCIES.md).

---

## How to build

### Run the local pipeline

```sh
QTSCROBBLER_QT_PREFIX=/path/to/Qt/6.12.0/gcc_64 \
QTSCROBBLER_JOBS=4 ./localPipeline.sh --noRun --noOpen --report-dir /tmp/qtscrobbler-reports
```

The pipeline builds the library, GUI and CLI, checks translations, runs the CLI help
smoke test and offline Qt Test/CTest suite, and retains logs and JUnit results.
Missing/failed tests or failed CLI smoke tests cause failure. Coverage and mutation
work toward the 95% targets is tracked in [plan.md](plan.md).

A [fictional example log](examples/README.md) is provided for safe imports from a temporary copy.

### Full build (out-of-tree, all targets)

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build build --parallel $(nproc)
```

Binaries are written to `build/` (GUI: `build/qtscrob`, CLI: `build/scrobbler`).

---

## How to run

### GUI

```sh
./build/qtscrob
# Optional flags:
#   -c PATH   path to configuration file
#   -v LEVEL  verbosity (0=error … 3=trace)
```

### CLI

```sh
./build/scrobbler --help
./build/scrobbler -f /path/to/.scrobbler.log
```

---

## Qt6 port status

See [`port_to_qt6.md`](port_to_qt6.md) for the full compatibility assessment and list of required changes.

---

# last.fm

Last.fm stopped support for groups and closed all existing ones.
GitHub issues are the place to exchange ideas and report bugs.
