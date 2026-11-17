# Acceptance and end-to-end checks

The acceptance suite exercises the real production targets with temporary settings,
fixture copies, Qt event loops and bounded loopback HTTP servers. Default tests use
fictional credentials and simulated services/devices.

## Workflow coverage

| Workflow | Test and exact acceptance evidence | Scope |
| --- | --- | --- |
| CLI import and upload | [upload](../tests/test_cli.cpp): launches scrobbler, asserts exact handshake/POST fields, source-version header, timestamp, normal exit and removal of the copied source log | Process end-to-end with a loopback service |
| CLI failure and metadata lookup | [CLI contracts](../tests/test_cli.cpp): argument/config/parser errors, rejected upload, missing-duration resolution before submission, retained source and exit codes | Process end-to-end; no live service |
| GUI import, edit and close | [importEditAndClose](../tests/test_gui.cpp): real production window, exact table fields, edits propagated into tracks, settings reopened, safe source clear and close | Qt widget acceptance inside Qt Test |
| GUI submit and cancel | [submissionProgress](../tests/test_gui.cpp): actual Submit/Cancel button clicks, progress lifetime, disabled/enabled actions, outcome signal and exact source/track retention for success/failure/cancel | Qt widget and network integration |
| GUI startup and shutdown | [private-display process test](../tests/test_gui_process.py): launches actual qtscrob under Xvfb, finds its real window, requests WM_DELETE_WINDOW and verifies graceful exit | Display-backed process end-to-end smoke |
| Retry without duplicate submission | [Scrobble contracts](../tests/test_scrobble.cpp): failed/auth/cancel retry clears only after success, successful sites receive no duplicate request, replacement imports preserve prior source | Production controller acceptance with loopback HTTP |
| Translation loading | [catalog contracts](../tests/test_translations.cpp): actual QTranslator resources, every active catalog entry and placeholders | Runtime resource integration |
| Extracted package | [package checker](../tools/run_quality_checks.py): archive extraction, ldd, exact CLI/GUI versions and observed real GUI startup | Packaged entry-point smoke; host Qt runtime required |

The GUI workflow suite runs the real widgets and controller in the test process.
The standalone GUI process check covers startup/close; it does not automate the
entire import/upload workflow in a separate process. Physical MTP/iPod and live
scrobbling-service checks have not been performed and are not implied by these tests.

## Run the checks

After building with the selected Qt and zlib prefixes:

```sh
ctest --test-dir build --label-regex acceptance --output-on-failure --no-tests=error
ctest --test-dir build --label-regex e2e --output-on-failure --no-tests=error
python3 tools/run_quality_checks.py --stage package --build-dir build --report-dir /path/to/new-package-reports
```

Labels organize existing deterministic checks; they do not replace the complete
pipeline, source line/branch thresholds, sanitizers, or native mutation qualification.
See [quality results](QUALITY.md#measured-results) and the [execution plan](plan.md)
for exact versions, observed results and unresolved release requirements.
