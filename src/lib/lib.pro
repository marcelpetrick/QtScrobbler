include (../common.pri)
include (lib.pri)

win32:win32-msvc* {
    TARGET = libscrobble
} else {
    TARGET = scrobble
}

TEMPLATE = lib
CONFIG += staticlib # shared

QT += core \
    network \
    xml \
    sql

QT -= gui
LANGUAGE = C++
