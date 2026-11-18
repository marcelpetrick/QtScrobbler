# Private Python 3.15.2 toolchain

The selected tooling interpreter is CPython **3.15.2**, the newest stable release
verified on 2026-11-17. This interpreter runs provisioning and quality scripts;
it is not an application runtime dependency. Do not replace the operating system's
Python. GitHub Actions selects the same exact interpreter with the immutable
`actions/setup-python` v7.1.0 commit
`6d4fd22b20465ccc0f7354476588e8df7c9ca0e0`. Both workflows run Qt provisioning from a
virtual environment installed from the bootstrap and quality locks.

Sources:

- [Official Python release](https://www.python.org/downloads/release/python-3152/)
- [Official source archive](https://www.python.org/ftp/python/3.15.2/Python-3.15.2.tar.xz)
- [Official Sigstore bundle](https://www.python.org/ftp/python/3.15.2/Python-3.15.2.tar.xz.sigstore)
- [setup-python release](https://github.com/actions/setup-python/releases/tag/v7.1.0)
- [setup-python exact tag reference](https://api.github.com/repos/actions/setup-python/git/ref/tags/v7.1.0)

The source archive SHA-256 is
`8b5536d280142ecf6cdfe15d12a21ba1c65e300bbf7b28a8d0cdb849575797de`.
The downloaded bytes matched the official Sigstore bundle's SHA2_256 subject
digest. This digest comparison is not a claim of full Sigstore signature or
certificate verification.

## Private source build on Linux

Use a new dedicated work directory on a filesystem with adequate free space.
The local `/tmp` is a shared tmpfs; do not fill it while instrumented builds run.
A native build and installation require approximately 0.5 GB, plus the quality
environment and its package cache. The observed four-worker source build took
about three minutes; runner timings can differ.

Ubuntu build inputs for the modules used by provisioning and quality scripts
are `build-essential`, `libssl-dev`, `zlib1g-dev`, `libbz2-dev`,
`libreadline-dev`, `libsqlite3-dev`, `libffi-dev`, `liblzma-dev`, `libzstd-dev`,
and `libexpat1-dev`. Install these only in a reproducible runner or existing
project build environment. Other optional standard-library modules can require
additional native development packages; their absence is not proof that a
required module works.

The following example uses only a private installation. Run it from the repository
root, with a new absolute directory substituted for `python_work`.

```sh
python_work=/absolute/private/path/qtscrobbler-python-3.15.2
mkdir -p "$python_work"
export python_work
python3 - <<'PY'
import hashlib
import os
from pathlib import Path
import tarfile
import urllib.request

work = Path(os.environ['python_work'])
archive = work / 'Python-3.15.2.tar.xz'
url = 'https://www.python.org/ftp/python/3.15.2/Python-3.15.2.tar.xz'
with urllib.request.urlopen(url, timeout=60) as response:
    archive.write_bytes(response.read())
expected = '8b5536d280142ecf6cdfe15d12a21ba1c65e300bbf7b28a8d0cdb849575797de'
if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
    raise SystemExit('Python source checksum mismatch')
with tarfile.open(archive) as source:
    source.extractall(work, filter='data')
PY
mkdir "$python_work/build"
(
  cd "$python_work/build"
  ../Python-3.15.2/configure --prefix="$python_work/install" --with-ensurepip=install
  make -j4
  ./python -m test -j4 test_ssl test_hashlib test_sqlite3 test_tarfile test_subprocess test_venv
  make -j4 install
)
"$python_work/install/bin/python3" -m venv "$python_work/quality-tools"
"$python_work/quality-tools/bin/python" -m pip install --no-deps -r requirements-bootstrap.txt
"$python_work/quality-tools/bin/python" -m pip install --no-deps --no-build-isolation -r requirements-quality.txt
"$python_work/quality-tools/bin/python" -m pip check
export PATH="$python_work/quality-tools/bin:$PATH"
```

No PGO or experimental JIT is enabled: this is a standard GIL-enabled interpreter
for deterministic tooling. The private `python3` and `python3.15` executables refer
to the new interpreter; no system executable or package is overwritten.

## Exact bootstrap and package compatibility

[requirements-bootstrap.txt](../requirements-bootstrap.txt) pins the environment
bootstrap to `pip==26.3`, `setuptools==84.2.1`, `wheel==0.48.0`, and
`packaging==26.3`, `setuptools-scm==10.3.4`, and `vcs-versioning==2.6.0`.
All six matched their current stable PyPI `info.version` on
2026-11-17. Install this file before the separate
[quality resolution](../requirements-quality.txt). Bootstrap/build tooling is
listed explicitly rather than implied by the interpreter's bundled pip version.
The SCM helpers are required by the three source-wheel builds.
Use `--no-deps` for both locks and run `pip check` after installation. Use
`--no-build-isolation` after installing the bootstrap so source builds use
these exact installed backends rather than independently resolving floating
build requirements.

All 31 quality entries installed successfully under Python 3.15.2, and
`pip check` reported no broken requirements. `inflate64`, `pybcj`, and `pyppmd`
required local source-wheel builds because the selected releases had no suitable
CPython 3.15 wheel; all three builds succeeded. They were then rebuilt with
`--no-build-isolation --no-cache-dir` against the six exact bootstrap tools,
installed from those local wheels and checked again. A real py7zr archive
write/extract roundtrip also passed. A binary-only installation is
therefore not the qualified installation mode. Native build dependencies must
remain available while creating this environment.

Observed executables: aqtinstall 3.3.0, gcovr 8.8, clang-format 23.1.5 and
clang-tidy 23.1.4. Imports exercised SSL, hashes, SQLite, zlib/bzip2/lzma/zstd,
ctypes, tar handling, subprocesses, venv, and the compiled 7-Zip helper modules.
Host libraries used by this prototype remain separately inventoried; for example,
its Python SSL module uses OpenSSL 3.6.5 and its SQLite module uses SQLite 3.53.4.
This Python update does not claim those libraries were upgraded or bundled.

## Retained prototype evidence

The private prototype is
`~/.cache/qtscrobbler-python-probe-3152/install`, with quality environment
`~/.cache/qtscrobbler-python-probe-3152/quality-tools`.

- Configure/build/install completed successfully; module check reported 116
  modules, zero missing modules and zero import failures on this host.
- Six selected CPython suites passed: **2,027 tests run, 92 skipped**, 19.7 seconds.
  Platform/resource skips remain skips; this was not the full CPython suite.
- Independent project Python tooling diagnostic: **119 tests passed**, 15.483
  seconds. This is separate from the C++/GUI and full native pipeline gates.
- The exact bootstrap installed successfully, all 31 locked quality dependencies
  installed, executable/import checks passed, and `pip check` passed.

Logs are `configure.log`, `build.log`, `install.log`, `tests.log`,
`bootstrap-full.log`, `requirements.log`, `pinned-wheel-builds.log`,
`pinned-wheel-install.log`, `pinned-requirements-check.log`, and
`project-tooling-tests.log` below
`~/.cache/qtscrobbler-python-probe-3152`. Fresh whole-project and actual Actions
qualification still apply to the integrated change.
