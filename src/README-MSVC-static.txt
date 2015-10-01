Static building of QTScrobbler using MSVC.


Prerequisites:

If you don't already have a version of Visual Studio installed, the free Express version can be downloaded from: http://www.microsoft.com/express/vc/
Also required (for QT and MTP support) is the Windows SDK for Windows 7.


Building QT

The first stage is to download and compile Trolltechs QT framework.  Download the latest qt-everywhere-opensource-src zip file from:
ftp://ftp.trolltech.com/qt/source/
and extract this out, i.e. "C:\QT\4.6.2-static".  This location will become the final installation place for QT.

Next, edit the QT mkspec for your version of Visual Studio, i.e. mkspecs\win32-msvc2008\qmake.conf
Change the following lines from:

QMAKE_CFLAGS_RELEASE	= -O2 -MD
QMAKE_CFLAGS_DEBUG	= -Zi -MDd

to:

QMAKE_CFLAGS_RELEASE	= -O2 -MT
QMAKE_CFLAGS_DEBUG	= -Zi -MTd

This statically compiles the MSVC runtime library into QT, removing the need for it to be installed when QT is distributed to end users.

Launch the "Visual Studio Command Prompt" and cd to the extracted QT source folder.  Run configure for your version of MSVC, i.e.:

configure -static -debug-and-release -platform win32-msvc2008

Once the configure stage has finished, complete the QT build process by running "nmake sub-src" (the sub-src option skips the building of the examples and demos, which can take several hours).

Create a new batch file within the QT bin folder (normally called qtvars.bat - adjust paths as needed)

###########
@echo off

@set QTDIR=C:\QT\4.6.2-static
@set PATH=C:\QT\4.6.2-static\bin;%PATH%
@set QMAKESPEC=win32-msvc2008

"C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"
###########

and create a command prompt shortcut link, i.e. "%COMSPEC% /k "C:\QT\4.6.2-static\bin\qtvars.bat"


Building QTScrobbler

Use the command prompt shortcut created at the end of the QT stage, and navigate to the src folder (where this file is located).  Run "qmake -tp vc -r", and launch the MSVC qtscrob.sln file and compile (or run plain qmake followed by nmake). You will need to re-run the qmake step if the project files (.pro and .pri) are modified.

