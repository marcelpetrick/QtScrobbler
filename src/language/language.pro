TEMPLATE = app
DEPENDPATH += \
    ../lib \
    ../cli \
    ../qt

include (../lib/lib.pri)
include (../cli/cli.pri)
include (../qt/qt.pri)

TRANSLATIONS = de.ts pl.ts
