include (../common.pri)

unix:!mac { 
    target.path = $${BINDIR}

    desktop.path = $${DATADIR}/applications
    desktop.files = qtscrob.desktop

    icon16.path = $${DATADIR}/icons/hicolor/16x16/apps
    icon16.files = resources/icons/16x16/qtscrob.png

    icon32.path = $${DATADIR}/icons/hicolor/32x32/apps
    icon32.files = resources/icons/32x32/qtscrob.png

    icon64.path = $${DATADIR}/icons/hicolor/64x64/apps
    icon64.files = resources/icons/64x64/qtscrob.png

    man.files = qtscrob.1
    man.path = $${DATADIR}/man/man1
    
    man-compress.path = $${DATADIR}/man/man1
    man-compress.extra = "gzip -9 -f \$(INSTALL_ROOT)/$$DATADIR/man/man1/qtscrob.1"

    INSTALLS = target \
        desktop \
        icon16 \
        icon32 \
        icon64 \
        man \
        man-compress
}

TEMPLATE = app
TARGET = qtscrob
QT += gui \
      widgets
LANGUAGE = C++
INCLUDEPATH += . \
    src \
    src/ui \
    ../lib \
    ../common
DEPENDPATH += ../lib
CONFIG += qt \
    x11

win32 {
    CONFIG(debug, debug|release) {
        CONFIG += console
    } else {
        CONFIG -= console
        CONFIG += windows
    }
    RC_FILE = qtscrob.rc
}

OPENEDFILES = src/main.cpp \
    src/qtscrob.h \
    src/qtscrob.cpp

RESOURCES = qtscrob.qrc

unix:mac {
    ICON = resources/icons/qtscrob.icns
}

include (../app.pri)
include (qt.pri)
