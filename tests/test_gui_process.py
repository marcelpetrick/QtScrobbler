#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Launch the real GUI on a private X server, then request a graceful window close."""
import ctypes as C
from ctypes.util import find_library
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import time


class ClientMessage(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('message_type', C.c_ulong),
                ('format', C.c_int), ('data', C.c_long * 5)]


class Event(C.Union):
    _fields_ = [('client', ClientMessage), ('padding', C.c_long * 24)]


def main():
    binary = Path(sys.argv[1]).resolve()
    if not binary.is_file() or not shutil.which('Xvfb') or not find_library('X11'):
        raise RuntimeError('GUI binary, Xvfb and libX11 are mandatory for display-backed workflow')
    x = C.CDLL(find_library('X11'))
    x.XOpenDisplay.argtypes, x.XOpenDisplay.restype = [C.c_char_p], C.c_void_p
    x.XDefaultRootWindow.argtypes, x.XDefaultRootWindow.restype = [C.c_void_p], C.c_ulong
    x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
    x.XGetWMProtocols.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_int)]
    x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
    x.XInternAtom.argtypes, x.XInternAtom.restype = [C.c_void_p, C.c_char_p, C.c_int], C.c_ulong
    x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]
    x.XSync.argtypes = [C.c_void_p, C.c_int]
    x.XFree.argtypes = [C.c_void_p]
    x.XCloseDisplay.argtypes = [C.c_void_p]
    with tempfile.TemporaryDirectory(prefix='qtscrobbler-display-') as temporary:
        home = Path(temporary)
        read_fd, write_fd = os.pipe()
        server_log = (home / 'xvfb.log').open('w+')
        server = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'],
                                  pass_fds=(write_fd,), stdout=server_log, stderr=server_log)
        os.close(write_fd)
        child = None
        display = None
        try:
            # Shared CI runners can need several seconds before Xvfb reports its display.
            if not select.select([read_fd], [], [], 20)[0]:
                server_log.seek(0)
                raise RuntimeError('private X server did not start: ' + server_log.read())
            display_name = b':' + os.read(read_fd, 32).strip()
            os.close(read_fd)
            read_fd = -1
            display = x.XOpenDisplay(display_name)
            if not display:
                raise RuntimeError('cannot connect to private X server')
            config = home / 'custom.ini'
            config.touch()
            environment = os.environ.copy()
            environment.update(HOME=temporary, XDG_CONFIG_HOME=temporary, XDG_CACHE_HOME=temporary,
                               XDG_RUNTIME_DIR=temporary, DISPLAY=display_name.decode(),
                               QT_QPA_PLATFORM='xcb', QT_XCB_GL_INTEGRATION='none', LC_ALL='C.UTF-8')
            child = subprocess.Popen([str(binary), '--config', str(config), '--verbose=0'], env=environment,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 5
            window = 0
            while time.monotonic() < deadline and child.poll() is None and not window:
                root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                if not x.XQueryTree(display, x.XDefaultRootWindow(display), C.byref(root), C.byref(parent), C.byref(children), C.byref(count)):
                    raise RuntimeError('cannot inspect private X tree')
                try:
                    for candidate in children[:count.value]:
                        name = C.c_void_p()
                        if x.XFetchName(display, candidate, C.byref(name)) and name.value:
                            try:
                                if C.string_at(name) == b'QTScrobbler':
                                    protocols, number = C.POINTER(C.c_ulong)(), C.c_int()
                                    try:
                                        if x.XGetWMProtocols(display, candidate, C.byref(protocols), C.byref(number)):
                                            if x.XInternAtom(display, b'WM_DELETE_WINDOW', 0) in protocols[:number.value]:
                                                window = candidate
                                    finally:
                                        if protocols:
                                            x.XFree(protocols)
                            finally:
                                x.XFree(name)
                finally:
                    if children:
                        x.XFree(children)
                if not window:
                    time.sleep(0.01)
            if not window:
                raise RuntimeError('real Qt main window did not appear')
            event = Event()
            event.client = ClientMessage(33, 0, 1, display, window,
                                         x.XInternAtom(display, b'WM_PROTOCOLS', 0), 32,
                                         (C.c_long * 5)(x.XInternAtom(display, b'WM_DELETE_WINDOW', 0), 0, 0, 0, 0))
            deadline = time.monotonic() + 5
            while child.poll() is None and time.monotonic() < deadline:
                if not x.XSendEvent(display, window, 0, 0, C.byref(event)):
                    raise RuntimeError('window close request failed')
                x.XSync(display, 0)
                time.sleep(0.05)
            output, errors = child.communicate(timeout=5)
            if child.returncode or errors:
                raise RuntimeError(f'GUI close failed ({child.returncode}): {errors.decode(errors="replace")}')
            if not config.is_file():
                raise RuntimeError('custom settings file disappeared')
            print('PASS: private X11 main window appeared and closed normally; custom configuration retained')
        finally:
            if child and child.poll() is None:
                child.kill()
                child.communicate(timeout=5)
            if display:
                x.XCloseDisplay(display)
            if read_fd >= 0:
                os.close(read_fd)
            server.terminate()
            server.wait(timeout=5)
            server_log.close()


if __name__ == '__main__':
    main()
