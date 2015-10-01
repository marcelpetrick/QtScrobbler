# Common files
SOURCES += libscrobble.cpp \
    parse-ipod.cpp \
    parse-log.cpp \
    submit.cpp \
    common.cpp \
    conf.cpp \
    gettrackinfo.cpp \
    dbcache.cpp

HEADERS += libscrobble.h \
    submit.h \
    parse.h \
    parse-ipod.h \
    parse-log.h \
    conf.h \
    gettrackinfo.h \
    dbcache.h

#mtp joy
contains (MTP, WPD) {
    SOURCES += parse-mtp-win32.cpp
    HEADERS += parse-mtp.h
}
contains (MTP, LIBMTP) {
    SOURCES += parse-mtp-libmtp.cpp
    HEADERS += parse-mtp.h
}
