# Qt Scrobbler

This is a clone of the famous sourceforge-project (from Robert Keevil and others ..); refreshed and slightly improved.
QTScrobbler ships both multiplatform GUI and CLI versions and requires Qt >= 5.5
Optional MTP support requires _libmtp-dev_ and _pkg-config_
(or the Windows 7 SDK if using MS Visual C++).

## How to build?
```
cd src && qmake && make
cd qt && qmake && make
```

## Note
With the most recent changes the MTP support (at least for Win) is dropped. It builds, but you have to pick the scrobbler.log manually on the device.

# last.fm
~~If you like QTScrobbler, please join http://www.last.fm/group/QTScrobbler~~  
Sadly, since last.fm closed the groups, this is not available anymore :)
