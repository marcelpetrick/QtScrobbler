# Translation catalogs and runtime checks

QtScrobbler ships German (`de`) and Polish (`pl`) catalogs. The application selects
its localized `:/language.qm` resource through Qt's locale handling. The tests set
`QLocale` explicitly; they do not require `de_DE.UTF-8` or another locale to be
installed in the operating system.

## Catalog scope

The catalog source is the current `.cpp`, `.h` and `.ui` files under `src/lib`,
`src/cli` and `src/qt/src`, including the messages extracted from platform adapters.
Do not update the catalogs through the obsolete qmake include paths in
`src/language/language.pro`: that previously marked every translation inactive.

Measured after extraction with Qt Linguist 6.12.0 on 2026-11-17:

| Catalog | Active entries | Finished | Empty/unfinished | Historical entries | Same spelling as English |
| --- | ---: | ---: | ---: | ---: | ---: |
| German | 223 | 223 | 0 | 46 | 41 |
| Polish | 223 | 223 | 0 | 46 | 41 |

Historical entries remain in the TS files for reference and are not compiled into
runtime translations. Thirty-three existing entries per catalog were recovered
from the inactive history; the remaining active messages now have translations.
Every active message preserves the source's complete placeholder multiset,
including `%1` through `%5` and repeated placeholders.

An identical source and translation is not a fallback. The runtime test requires a
nonempty `QTranslator::translate()` result for all 223 active entries in each
language, and compares it with the corresponding TS text. The 41 identical entries
per catalog are reviewed vocabulary or notation:

- Both: the application name `QTScrobbler`, `Album`, `INFO `, `DEBUG`, `TRACE`,
  `Proxy`, `Port`, `OK`, all 27 hour offsets from `-12` to `+14`, the minute values
  `00`, `15`, `30`, `45`, and the rich-text colon between hour and minute controls.
- German additionally retains the menu word `Global`.
- Polish additionally retains the version-label format `QTScrobbler v%1`.

`Last.fm` and `Libre.fm` remain service names. They are displayed unchanged and are
not catalog entries. URLs, email addresses, author names, protocol identifiers,
file-format headers and command-line option spellings remain unchanged within
translated messages.

## Runtime labels and stored configuration

The configuration groups remain `Last.fm`, `Libre.fm` and `Custom` in every
language. `site_display_name()` translates the Custom label when Settings and
Progress widgets are constructed; it does not rename saved configuration groups.
`log_level_display_name()` resolves translated Console labels after translator
installation. Resolving those labels during static initialization was too early
and left them in English.

Backend log-level prefixes remain the technical identifiers `ERROR`, `INFO `,
`DEBUG` and `TRACE`. The Console translates their presentation. Backend diagnostics
wrapped in `tr()` have translated bodies; unmarked technical diagnostics remain
English. User-entered track metadata, credentials and service responses are data
and are displayed as supplied.

The 223-entry scope is the extracted translatable-message inventory, not a claim
that all product text is localized. The static HTML help body and the GUI
executable's command-line help are currently English. They are visible limitations,
not catalog successes or blanket exclusions from a localization percentage.

## Updating and checking

Use the selected Qt SDK and real maintained source files:

```sh
python3 - <<'PY'
import os
import subprocess
from pathlib import Path
sdk = Path(os.environ['QTSCROBBLER_QT_PREFIX'])
files = [str(path) for folder in ('src/lib', 'src/cli', 'src/qt/src')
         for path in Path(folder).rglob('*') if path.suffix in ('.cpp', '.h', '.ui')]
subprocess.run([str(sdk / 'bin/lupdate'), *files, '-ts',
                'src/language/de.ts', 'src/language/pl.ts'], check=True)
PY
```

Review newly extracted and fuzzy entries, preserve placeholders, translate active
messages, then build through the normal CMake/pipeline command. CMake compiles the
catalogs into the build tree and embeds them in the actual application resources.
Do not reuse an old QM file as evidence of a changed TS catalog.

The `translations` CTest checks all active compiled entries and placeholders for
both locales. It also constructs the actual translated main window, Settings,
Console and Progress widgets, verifies German/Polish labels, and confirms that
configuration keys and backend prefixes stay stable. An absent test key must return
an empty translator lookup, demonstrating the fallback detection used by the test.
A zero unfinished count alone is insufficient: an entirely inactive catalog also
has zero unfinished entries and produces an empty runtime catalog.
