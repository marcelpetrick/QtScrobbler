#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run a Linux command with loopback networking and no external network access.

Usage: python3 tools/run_offline.py -- COMMAND [ARGUMENT ...]

Dependency provisioning must finish first. Linux user, network and PID namespace
support and util-linux unshare with --kill-child are required. Each invocation
creates fresh namespaces; all command descendants are contained and terminated
when their namespace init exits. A private session bus has
no service directories, so GUI checks cannot activate desktop services. Missing
namespace support or tools fail rather than using host networking or its bus.
"""
from __future__ import annotations

import ctypes
import os
import shutil
import signal
import subprocess
import sys


# This helper is passed directly to the namespace's Python interpreter. There is
# deliberately no public "already isolated" mode or environment-variable bypass.
ENTER = """import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile

daemon = None
status = 125
try:
    result = subprocess.run([sys.argv[1], 'link', 'set', 'dev', 'lo', 'up'], check=False)
    if result.returncode:
        print('Offline runner: could not enable isolated loopback', file=sys.stderr)
        raise RuntimeError('isolated loopback setup failed')
    with tempfile.TemporaryDirectory(prefix='qtscrobbler-offline-bus-') as temporary:
        config = Path(temporary) / 'session.conf'
        config.write_text('<busconfig><type>session</type><listen>unix:tmpdir=' + temporary +
                          '</listen><auth>EXTERNAL</auth><policy context="default">'
                          '<allow user="*"/><allow send_destination="*"/>'
                          '<allow receive_sender="*"/><allow own="*"/></policy></busconfig>')
        try:
            daemon = subprocess.Popen([sys.argv[2], '--nofork', '--print-address=1',
                                       '--config-file=' + str(config)], stdout=subprocess.PIPE)
            if not select.select([daemon.stdout], [], [], 5)[0]:
                raise RuntimeError('private session bus did not provide an address')
            address = daemon.stdout.readline(4096).decode().strip()
            if not address.startswith('unix:') or daemon.poll() is not None:
                raise RuntimeError('private session bus did not start')
            environment = os.environ.copy()
            for name in ('DBUS_SESSION_BUS_PID', 'DBUS_STARTER_ADDRESS', 'DBUS_STARTER_BUS_TYPE'):
                environment.pop(name, None)
            environment['DBUS_SESSION_BUS_ADDRESS'] = address
            status = subprocess.run(sys.argv[4:], env=environment, check=False).returncode
        finally:
            if daemon is not None:
                if daemon.poll() is None:
                    daemon.terminate()
                try:
                    daemon.communicate(timeout=3)
                except subprocess.TimeoutExpired:
                    daemon.kill()
                    daemon.communicate()
except (OSError, RuntimeError, subprocess.SubprocessError) as error:
    print('Offline runner: ' + str(error), file=sys.stderr)
    status = 126
# Namespace PID1 cannot forward a default fatal signal to itself. Send the
# signed command status to the outer supervisor, which forwards it exactly.
os.write(int(sys.argv[3]), (str(status) + '\\n').encode())
os.close(int(sys.argv[3]))
"""


def invocation(arguments: list[str], status_fd: int) -> list[str]:
    if sys.platform != "linux":
        raise ValueError("offline execution requires Linux user, network and PID namespaces")
    if not arguments or arguments[0] != "--" or len(arguments) < 2:
        raise ValueError("usage: run_offline.py -- COMMAND [ARGUMENT ...]")
    executables = {}
    for name in ("unshare", "ip", "dbus-daemon"):
        path = shutil.which(name)
        if path is None:
            raise ValueError(f"required offline execution tool is missing: {name}")
        executables[name] = os.path.abspath(path)
    return [executables["unshare"], "--user", "--map-root-user", "--net", "--pid", "--fork",
            "--kill-child=SIGKILL", "--", sys.executable, "-c", ENTER,
            executables["ip"], executables["dbus-daemon"], str(status_fd), *arguments[1:]]


def stop_with_parent(parent_pid: int) -> None:
    """Unshare dies with this supervisor; --kill-child then kills namespace PID1."""
    if ctypes.CDLL(None, use_errno=True).prctl(1, signal.SIGKILL, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), "could not set namespace parent-death signal")
    if os.getppid() != parent_pid:
        os.kill(os.getpid(), signal.SIGKILL)


def main(arguments: list[str] | None = None) -> int:
    read_fd, write_fd = os.pipe()
    process = None
    status = 125
    try:
        command = invocation(sys.argv[1:] if arguments is None else arguments, write_fd)
        parent_pid = os.getpid()
        process = subprocess.Popen(command, pass_fds=(write_fd,),
                                   preexec_fn=lambda: stop_with_parent(parent_pid))
        os.close(write_fd)
        write_fd = -1
        process.wait()
        raw_status = os.read(read_fd, 64)
        if process.returncode or not raw_status:
            raise ValueError("namespace setup or command supervisor failed")
        status = int(raw_status)
        if not -64 <= status <= 255:
            raise ValueError("invalid command exit status from namespace supervisor")
    except KeyboardInterrupt:
        status = -signal.SIGINT
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"Offline runner: {error}", file=sys.stderr)
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait()
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)
    if status < 0:
        if -status not in (signal.SIGKILL, signal.SIGSTOP):
            signal.signal(-status, signal.SIG_DFL)
        os.kill(os.getpid(), -status)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
