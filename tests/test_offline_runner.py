#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fail-closed command contracts and real Linux network-isolation evidence."""
from __future__ import annotations

import errno
import importlib.util
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time
import unittest
from unittest import mock


RUNNER = Path(__file__).resolve().parents[1] / "tools/run_offline.py"
SPEC = importlib.util.spec_from_file_location("run_offline", RUNNER)
offline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(offline)


class OfflineRunnerTest(unittest.TestCase):
    def invoke(self, *arguments):
        return subprocess.run([sys.executable, str(RUNNER), "--", *arguments],
                              capture_output=True, text=True, timeout=10, check=False)

    def invoke_helper(self, ip, bus):
        read_fd, write_fd = os.pipe()
        try:
            result = subprocess.run([sys.executable, "-c", offline.ENTER, ip, bus, str(write_fd),
                                     sys.executable, "-c", "print('must not execute')"],
                                    pass_fds=(write_fd,), text=True, capture_output=True, timeout=5, check=False)
            os.close(write_fd)
            write_fd = -1
            return result, int(os.read(read_fd, 64))
        finally:
            os.close(read_fd)
            if write_fd >= 0:
                os.close(write_fd)

    def test_missing_command_and_separator_fail_before_execution(self):
        for arguments in ([], ["--"], ["echo", "unsafe"]):
            with self.subTest(arguments=arguments), mock.patch.object(offline.subprocess, "Popen") as execute:
                self.assertEqual(offline.main(arguments), 125)
                execute.assert_not_called()

    def test_unsupported_platform_fails_before_execution(self):
        with mock.patch.object(offline.sys, "platform", "darwin"), \
                mock.patch.object(offline.subprocess, "Popen") as execute:
            self.assertEqual(offline.main(["--", "command"]), 125)
            execute.assert_not_called()

    def test_missing_tool_fails_before_execution(self):
        for missing in ("unshare", "ip", "dbus-daemon"):
            with self.subTest(tool=missing), mock.patch.object(offline.shutil, "which") as which, \
                    mock.patch.object(offline.subprocess, "Popen") as execute:
                which.side_effect = lambda name: None if name == missing else "/usr/bin/" + name
                self.assertEqual(offline.main(["--", "command"]), 125)
                execute.assert_not_called()

    def test_namespace_arguments_preserve_command_without_shell_or_bypass(self):
        arguments = ["command with space", "", "$(touch forbidden)", "a; b", "--net"]
        with mock.patch.object(offline.shutil, "which", side_effect=lambda name: "/usr/bin/" + name), \
                mock.patch.dict(os.environ, {"QTSCROBBLER_OFFLINE": "1", "ALREADY_ISOLATED": "1"}):
            result = offline.invocation(["--", *arguments], 99)
        self.assertEqual(result[:9], ["/usr/bin/unshare", "--user", "--map-root-user", "--net",
                                    "--pid", "--fork", "--mount-proc", "--kill-child=SIGKILL", "--"])
        self.assertEqual(result[9:15], [sys.executable, "-c", offline.ENTER, "/usr/bin/ip", "/usr/bin/dbus-daemon", "99"])
        self.assertEqual(result[15:], arguments)

    def test_exec_failure_never_falls_back_to_host_command(self):
        with mock.patch.object(offline, "invocation", return_value=["unshare", "arguments"]), \
                mock.patch.object(offline.subprocess, "Popen", side_effect=PermissionError("namespace denied")) as execute:
            self.assertEqual(offline.main(["--", "command"]), 125)
            execute.assert_called_once()
            self.assertEqual(execute.call_args.args[0], ["unshare", "arguments"])

    def test_loopback_setup_failure_does_not_run_command(self):
        # The namespace helper itself must stop even if namespace creation succeeds.
        result, status = self.invoke_helper("/bin/false", "/bin/false")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(status, 126)
        self.assertNotIn("must not execute", result.stdout)
        self.assertIn("could not enable isolated loopback", result.stderr)

    def test_session_bus_startup_failure_does_not_run_command(self):
        result, status = self.invoke_helper("/bin/true", "/bin/false")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(status, 126)
        self.assertNotIn("must not execute", result.stdout)
        self.assertIn("private session bus did not start", result.stderr)

    def test_real_namespace_preserves_arguments_and_nonzero_exit(self):
        arguments = ["", "space and unicode é", "$(not executed); &"]
        for status in (0, 7, 143, 255):
            with self.subTest(status=status):
                result = self.invoke(sys.executable, "-c",
                                     f"import json,sys; print(json.dumps(sys.argv[1:])); sys.exit({status})",
                                     *arguments)
                self.assertEqual(result.returncode, status, result.stderr)
                self.assertEqual(json.loads(result.stdout), arguments)

    def test_real_namespace_forwards_signal_status(self):
        for termination in (signal.SIGTERM, signal.SIGKILL):
            with self.subTest(signal=termination):
                result = self.invoke(sys.executable, "-c", f"import os; os.kill(os.getpid(), {int(termination)})")
                self.assertEqual(result.returncode, -termination, result.stderr)

    @staticmethod
    def host_processes_with(token: str) -> dict[int, str]:
        """Map host PIDs of `python -c SCRIPT` processes whose SCRIPT carries token.

        The namespace has its own /proc, so PIDs printed inside it are not host PIDs.
        The supervisor, unshare and namespace init also carry the token as a later
        argument; only the command and its grandchild run a script containing it.
        """
        found = {}
        for entry in Path("/proc").iterdir():
            if not entry.name.isdigit() or int(entry.name) == os.getpid():
                continue
            try:
                argv = (entry / "cmdline").read_bytes().split(b"\0")
                if len(argv) < 3 or argv[1] != b"-c" or token.encode() not in argv[2]:
                    continue
                found[int(entry.name)] = (entry / "stat").read_text().rsplit(")", 1)[1].split()[0]
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                continue
        return found

    def test_killed_supervisor_stops_command_and_separate_session_grandchild(self):
        token = f"qtscrobbler-offline-escape-{os.getpid()}-{time.monotonic_ns()}"
        grandchild = f"import time; print('ready', flush=True); time.sleep(30)  # {token}"
        command = ("import subprocess,sys,time; "
                   f"p=subprocess.Popen([sys.executable,'-c',{grandchild!r}],stdout=subprocess.PIPE,text=True,start_new_session=True); "
                   "print(p.stdout.readline().strip(),flush=True); "
                   f"time.sleep(30)  # {token}")
        process = subprocess.Popen([sys.executable, str(RUNNER), "--", sys.executable, "-c", command],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        child_pids = []
        try:
            self.assertTrue(select.select([process.stdout], [], [], 5)[0], "offline child did not start")
            self.assertEqual(process.stdout.readline().strip(), "ready")
            child_pids = list(self.host_processes_with(token))
            self.assertEqual(len(child_pids), 2, child_pids)
            process.kill()
            process.communicate(timeout=5)
            self.assertEqual(process.returncode, -signal.SIGKILL)
            deadline = time.monotonic() + 3
            remaining = {}
            while time.monotonic() < deadline:
                # An adopted zombie has exited and is only waiting to be reaped.
                remaining = {pid: state for pid, state in self.host_processes_with(token).items() if state != "Z"}
                if not remaining:
                    break
                time.sleep(0.01)
            self.assertEqual(remaining, {}, "offline descendants stayed running after their supervisor was killed")
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate(timeout=5)
            for child_pid in self.host_processes_with(token):
                try:
                    os.kill(child_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_namespace_has_its_own_proc_for_leak_sanitizer(self):
        # LeakSanitizer reads /proc/<pid>/task; a host /proc in a PID namespace aborts it.
        probe = ("import os; print(os.getpid(), os.readlink('/proc/self'), "
                 "os.path.isdir(f'/proc/{os.getpid()}/task'), flush=True)")
        result = self.invoke(sys.executable, "-c", probe)
        self.assertEqual(result.returncode, 0, result.stderr)
        pid, proc_self, has_task = result.stdout.split()
        self.assertEqual(pid, proc_self)
        self.assertEqual(has_task, "True")

    def test_real_namespace_has_loopback_http_and_no_external_route(self):
        probe = r'''import errno,http.server,json,os,socket,subprocess,sys,threading,urllib.request
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'isolated loopback')
    def log_message(self, *args):
        pass
with http.server.HTTPServer(('127.0.0.1', 0), Handler) as server:
    worker = threading.Thread(target=server.handle_request)
    worker.start()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        with opener.open('http://127.0.0.1:%d/' % server.server_port, timeout=2) as response:
            payload = response.read().decode()
    finally:
        worker.join(timeout=3)
    assert not worker.is_alive()
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
    connection.settimeout(1)
    try:
        connection.connect(('192.0.2.1', 9))
    except OSError as error:
        blocked_errno = error.errno
    else:
        raise AssertionError('external address unexpectedly reachable')
bus = subprocess.run(['dbus-send', '--session', '--print-reply', '--dest=org.freedesktop.DBus',
                      '/org/freedesktop/DBus', 'org.freedesktop.DBus.ListActivatableNames'],
                     text=True, capture_output=True, check=True, timeout=2)
print(json.dumps({'net': os.readlink('/proc/self/ns/net'),
                  'user': os.readlink('/proc/self/ns/user'),
                  'bus': os.environ['DBUS_SESSION_BUS_ADDRESS'], 'activatable': bus.stdout,
                  'http': payload, 'blocked_errno': blocked_errno}))
'''
        result = self.invoke(sys.executable, "-c", probe)
        self.assertEqual(result.returncode, 0, result.stderr)
        proof = json.loads(result.stdout)
        self.assertNotEqual(proof["net"], os.readlink("/proc/self/ns/net"))
        self.assertNotEqual(proof["user"], os.readlink("/proc/self/ns/user"))
        self.assertEqual(proof["http"], "isolated loopback")
        self.assertEqual(proof["blocked_errno"], errno.ENETUNREACH)
        self.assertIn("qtscrobbler-offline-bus-", proof["bus"])
        self.assertEqual(proof["activatable"].count('string "'), 1)
        self.assertIn('string "org.freedesktop.DBus"', proof["activatable"])
        address = proof["bus"].split("unix:path=", 1)[1].split(",", 1)[0]
        self.assertFalse(Path(address).parent.exists(), "private bus directory was not removed")


if __name__ == "__main__":
    unittest.main()
