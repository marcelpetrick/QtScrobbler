VER = $$find(QT_VERSION, ^4\\.[6-9]+.*)
isEmpty(VER) {
    message("Qt >= 4.6 required!")
    !isEmpty(QT_VERSION) error("Qt found:" $$[QT_VERSION])
}
message("Qt version used:" $$VER)

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
isEmpty(QMAKE_LUPDATE) {
    win32:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]/lupdate.exe
    else:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]/lupdate
}

system($$QMAKE_LUPDATE -silent language/language.pro)
system($$QMAKE_LRELEASE -silent language/language.pro)

TEMPLATE=subdirs
SUBDIRS=lib qt cli
CONFIG += ordered

qt.depends = lib
cli.depends = lib
