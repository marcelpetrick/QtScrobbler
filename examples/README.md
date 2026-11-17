# Example scrobble log

`scrobble.log` contains three fictional tracks in Audioscrobbler 1.1 format. It contains
no account details and must not be submitted to a real service.

The first three lines specify the format, UTC timestamps, and generating client. Each
track has eight **tab-separated** fields: artist, album, title, track number, duration
in seconds, played flag (`L`), Unix timestamp in seconds, and optional MusicBrainz ID.
The IDs are all-zero placeholders, not real recording IDs. The example
timestamps are fixed at 2026-11-17 08:00, 08:05 and 08:10 UTC.

The parser discovers a file named `.scrobbler.log` inside a directory, rather than a
file named `scrobble.log`. Work on a disposable copy because clearing a parsed log
deletes it:

```sh
example_dir=$(mktemp -d)
cp examples/scrobble.log "$example_dir/.scrobbler.log"
```

In the GUI, open that copied `.scrobbler.log` through the log-file dialog. Do not press
Submit with a real account configured. Automated parser tests import the copy without
network access or credentials. The CLI performs submission as part of its normal file
workflow, so this example is not a safe live CLI command until a fake endpoint is set up.
