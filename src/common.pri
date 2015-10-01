# Comment the following section if autodetection fails
unix {
    CCACHE = $$system(which ccache)
    !isEmpty(CCACHE) {
        message("using ccache")
        QMAKE_CXX = ccache g++
        QMAKE_CC = ccache gcc
    }

    !mac {
        isEmpty( PREFIX ) {
            PREFIX = /usr/local
            message( "You can provide your own prefix." )
            message( "Example: PREFIX=\"/usr\" qmake-qt4" )
        }
        message( "Scrobbler will be installed in $${PREFIX}" )
        BINDIR =  $$PREFIX/bin
        DATADIR = $$PREFIX/share
    }
}

CUSTOM_BUILD_FOLDERS = true

win32{
    win32-msvc* {
        # exclude MSVC - otherwise requires full rebuilds when changing between release/debug
        CUSTOM_BUILD_FOLDERS = false
    }
}

contains(CUSTOM_BUILD_FOLDERS, true) {
    OBJECTS_DIR = build/.o
    UI_DIR = build/.ui
    MOC_DIR = build/.moc
    RCC_DIR = build/.rcc
}

#mtp joy
win32{
    win32-msvc* {
        # Can't always use WindowsSdkDir, but ProgramFiles doesn't work on AMD64
        # Change this as required.
		# SDK = $$(ProgramW6432)"\Microsoft SDKs\Windows\v7.0\"
        # SDK = $$(ProgramFiles)"\Microsoft SDKs\Windows\v7.0\"
        SDK = $$(WindowsSdkDir)
        message($$SDK)
        exists( $$SDK ) {
            message( "Found the Windows SDK - enabling MTP" )
            DEFINES += HAVE_WPD
            MTP = WPD
        } else {
            message ( "The Windows Platform SDK was NOT found - MTP support will not be compiled" )
        }
    }
} else {
    PKGC = $$system(which pkg-config)
    !isEmpty(PKGC) {
        message("using pkg-config")
        CONFIG += link_pkgconfig
        PKGCONFIG += libmtp
        DEFINES += HAVE_LIBMTP
        MTP = LIBMTP
    } else {
        message ("pkg-config was not found. MTP support will not be included.")
    }
}
