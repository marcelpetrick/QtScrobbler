# Native API contracts

The native implementation is internal to QtScrobbler; no stable binary plugin API
is promised. Doxygen indexes the actual declarations and signal signatures.

`Parse` publishes entries by value and completes open/clear operations through
`open_finished` and `clear_finished`. Callers must inspect the success argument.
Log and iPod import operate on user-selected paths. Clearing is destructive and
requires a successful import; tests always use disposable fixture copies.

`Scrobble` owns the configuration, selected parser, cache and service submission
objects. It forwards parser completion and tracks per-site submission state.
A successful network response is required before an entry is marked submitted.
Timestamp recalculation and duration lookup operate on the current imported list.

`Conf` loads and saves settings, including service credentials. Tests replace HOME
and XDG paths before constructing a Qt application. Logs and reports must not include
passwords. A custom settings filename is used as an exact filename.

`DBCache` owns a unique SQLite connection. The cache is ready only after `init()`
returns true. Its injectable directory and clock support isolated expiry tests.
Destroying an instance releases its queries before removing the connection.

`Submit` performs handshakes and bounded batch submission using Qt's network event
loop. Its managers own outstanding replies. Failures retain unsent tracks and emit
completion. Cancellation aborts outstanding operations without reporting success.

`GetTrackInfo` obtains missing duration metadata. Its injectable endpoint is used
by offline loopback tests. Requests and callbacks must remain associated with their
original track, including cancellation and overlapping lookups.

`QTScrob` owns the main table and auxiliary windows; public slots handle imports,
edits, settings and progress. Tests drive real widgets and signals through Qt's
event loop. The command line follows the same Scrobble contracts and must wait for
missing durations before submitting eligible tracks.
