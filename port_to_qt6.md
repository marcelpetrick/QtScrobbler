# Qt6 Porting Assessment

## Current State

The project was originally written for Qt4 and later adapted for Qt5.
The local system ships **Qt 6.11.1** (Manjaro/Arch Linux) where `qmake` and `qmake6` both point to the Qt6 toolchain.
Primary target going forward: **Linux only** (Windows and macOS support is deprioritised).

---

## Incompatible Qt5 APIs (compilation blockers)

### 1. `QDateTime::toTime_t()` / `fromTime_t()` — removed in Qt6

Replacement: `toSecsSinceEpoch()` / `fromSecsSinceEpoch()`.

| File | Line(s) |
|------|---------|
| `src/lib/dbcache.cpp` | 152, 162 |
| `src/lib/submit.cpp` | 248 |
| `src/lib/libscrobble.cpp` | 41, 76 |
| `src/qt/src/qtscrob.cpp` | 617 |
| `src/lib/parse-mtp-win32.cpp` | 456, 462 (Windows-only, lower priority) |

### 2. `qSort()` — removed in Qt6

Replacement: `std::sort()` (from `<algorithm>`).

| File | Line |
|------|------|
| `src/lib/libscrobble.cpp` | 159 |

### 3. `QTextCodec` — moved out of QtCore into Qt5Compat

`QTextCodec::codecForLocale()` is no longer available without pulling in the `core5compat` module.
The single usage converts a C-string locale timezone name to `QString`.

Replacement: `QString::fromLocal8Bit(tzname[tzindex])` — no extra module needed.

| File | Lines |
|------|-------|
| `src/lib/libscrobble.cpp` | 85–86 |

### 4. `QLayout::setMargin()` — removed in Qt6

Replacement: `setContentsMargins(0, 0, 0, 0)`.

| File | Line |
|------|------|
| `src/qt/src/console.cpp` | 65 |
| `src/qt/src/help.cpp` | 35 |
| `src/qt/src/qtscrob.cpp` | 136 |

### 5. `CONFIG += x11` in qmake — QtX11Extras removed in Qt6

The `x11extras` module was dissolved into the xcb platform plugin.
The `CONFIG += x11` option is no longer valid and must be removed from `src/qt/qt.pro`.

### 6. `endl` in `QTextStream` — deprecated global symbol

Qt6 replaced the global `endl` stream manipulator with `Qt::endl`.
Using the unqualified form produces deprecation warnings and can be ambiguous when `<ostream>` headers are pulled in transitively.

| File | Affected lines |
|------|----------------|
| `src/qt/src/main.cpp` | 31, 33, 35, 37 |

---

## Deprecated (compiles but produces warnings)

### Old-style SIGNAL/SLOT macros

All `connect()` calls use the string-based macro syntax (`SIGNAL(...)` / `SLOT(...)`).
This syntax still compiles in Qt6 but:
- provides no compile-time type checking,
- is flagged by `clazy` and Qt's own `-Wzero-as-null-pointer-constant` tooling,
- is officially deprecated.

Recommended migration: pointer-to-member function syntax.

```cpp
// old
connect(btn, SIGNAL(clicked()), this, SLOT(open_log()));

// new
connect(btn, &QPushButton::clicked, this, &QTScrob::open_log);
```

Approximately 30 `connect()` calls spread across all source files need updating.

---

## Build system notes

The project uses **qmake** (`.pro` files).
Qt6 ships its own `qmake` (version 3.x) and it is the default on Arch/Manjaro, so existing `.pro` files continue to work after the API fixes above.
CMake is the officially preferred build system for new Qt6 projects; migration is desirable long-term but out of scope for the initial port.

---

## What does NOT need changing

- `QT += network sql xml` — all three modules exist in Qt6.
- `QNetworkAccessManager` / `QNetworkReply` API — unchanged.
- `QCryptographicHash` — unchanged.
- `QSqlQuery` / `QSqlDatabase` — unchanged.
- `QString`, `QUrl`, `QDateTime` (except `toTime_t`) — unchanged.
- `QFile::exists()` static overload — still available (minor style note only).
- `QString::fromStdWString()` — still available (Windows, secondary priority).
- MTP support via `libmtp` + `pkg-config` detection in `common.pri` — still works.

---

## Recommended modernisation steps (beyond strict Qt6 compatibility)

1. Migrate all `connect()` calls to the typed pointer-to-member syntax.
2. Migrate the build system from qmake to CMake (Qt6's recommended system).
3. Replace `#include <QtCore>` catch-all headers with specific includes.
4. Consider using `QDateTime::currentSecsSinceEpoch()` (Qt 5.8+) where a plain integer is needed, instead of `QDateTime::currentDateTime().toSecsSinceEpoch()`.
5. Drop dead Windows/MTP code paths if macOS and Windows support is officially abandoned.

---

## Build verification on Linux (Qt 6.11.1, Manjaro)

After applying the fixes above, the standard build should work:

```sh
cd src
qmake
make -j$(nproc)
```

To build only the GUI application:

```sh
cd src/qt
qmake
make -j$(nproc)
./qtscrob
```

To build only the CLI:

```sh
cd src/cli
qmake
make -j$(nproc)
./scrobbler --help
```


----

###  Follow-up modernisation (not yet done)

- [ ] Migrate all ~30 old-style SIGNAL/SLOT connect() calls to typed pointer-to-member syntax for compile-time safety
- [ ] Migrate build system from qmake to CMake (Qt6's officially recommended system)
- [ ] Replace #include <QtCore> catch-all headers with specific module includes
- [ ] Use QDateTime::currentSecsSinceEpoch() (shorter form) wherever only the integer timestamp is needed
- [ ] Decide whether to officially drop Windows/macOS support and remove dead MTP/Win32 code paths
