# Qt Scrobbler

Initially this was just a clone (fork) from the sourceforge-project of Robert Keevil and others. I adapted the code to make it buildable with Qt5 (before Qt4).  
QTScrobbler ships both multiplatform GUI and CLI versions and requires Qt >= 5.5.
Optional MTP support requires _libmtp-dev_ and _pkg-config_ (or the Windows 7 SDK if using MS Visual C++).

`sudo apt-get install libmtp-dev pkg-config `

## How to build?
```
cd src && qmake && make  
cd qt && qmake && make
```

## Note
With the most recent changes the MTP support (at least for Win) is dropped. It builds, but you have to pick the `scrobbler.log` manually on the device.  
This is acceptable for me. I am also thinking about dropping the support for macOS and Win at all.  
Additionally future porting to Qt6 (OpenSource) is planned.

# last.fm
Update: last.fm has stopped the support of groups and closed all existing ones. So there is no other way than github to exchange ideas.
