include (../common.pri)
include (cli.pri)

TEMPLATE = app
TARGET = scrobbler
QT -= gui
LANGUAGE = C++
INCLUDEPATH += . \
    src \
    src/ui \
    ../lib \
    ../common
DEPENDPATH += ../lib
CONFIG += qt \
    console

RESOURCES = scrobbler.qrc

unix:mac {
    CONFIG -= app_bundle
}

unix:!mac {
    INSTALLS += target \
                man \
                man-compress

    target.path = $$BINDIR

    man.files = scrobbler.1
    man.path = $${DATADIR}/man/man1

    man-compress.path = $${DATADIR}/man/man1
    man-compress.extra = "gzip -9 -f \$(INSTALL_ROOT)/$$DATADIR/man/man1/scrobbler.1"

}

include (../app.pri)
