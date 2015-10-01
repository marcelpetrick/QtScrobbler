# common qmake statements for both CLI and GUI apps

win32:win32-msvc* {
    SOURCES += ../common/xgetopt.c
    CONFIG(debug, debug|release) {
        LIBS += -L../lib/debug -llibscrobble
    } else {
        LIBS += -L../lib/release -llibscrobble
    }
    contains(MTP, WPD) {
        # for VariantTimeToSystemTime
        LIBS += OleAut32.lib
        contains(QMAKE_HOST.arch, "x86_64") {
            LIBS += -L"$$SDK\\lib\\x64" PortableDeviceGUIDs.lib
        } else {
            LIBS += -L"$$SDK\\lib" PortableDeviceGUIDs.lib
        }
    }
} else {
    # compile with libscrobble
    LIBS += -L../lib -lscrobble
}

QT += network sql xml
