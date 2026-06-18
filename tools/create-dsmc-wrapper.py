#!/usr/bin/env python3
"""Create the user-facing DSMC/IAC launcher wrapper."""

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--wrapper", required=True, type=Path)
    args = parser.parse_args()

    script = f"""#!/usr/bin/env python3
\"\"\"Launch the private DSMC/IAC executable with IAC-friendly defaults.\"\"\"

import os
import pty
import subprocess
import sys
from collections import deque

exe = {str(args.executable)!r}
argv = []
verbose = False
has_screen = False
color = None

i = 1
while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == "--iac-dsmc-verbose":
        verbose = True
        i += 1
        continue
    if arg == "--iac-color":
        if i + 1 >= len(sys.argv):
            sys.stderr.write("dsmc-iac: --iac-color requires auto, on, or off\\n")
            raise SystemExit(2)
        color = sys.argv[i + 1]
        if color not in ("auto", "on", "off"):
            sys.stderr.write("dsmc-iac: --iac-color requires auto, on, or off\\n")
            raise SystemExit(2)
        i += 2
        continue
    argv.append(arg)
    if arg in ("-screen", "-sc"):
        has_screen = True
        if i + 1 >= len(sys.argv):
            sys.stderr.write("dsmc-iac: -screen requires an argument\\n")
            raise SystemExit(2)
        i += 1
        argv.append(sys.argv[i])
    i += 1

if color is not None:
    os.environ["IAC_COLOR"] = color
elif "IAC_COLOR" not in os.environ:
    os.environ["IAC_COLOR"] = "auto"

if verbose or has_screen:
    raise SystemExit(subprocess.call([exe] + argv))

output_tail = deque(maxlen=700)

def compact_line(line):
    plain = line
    while plain.startswith("\\033["):
        marker = plain.find("m")
        if marker < 0:
            break
        plain = plain[marker + 1:]
    return plain.startswith("[IAC]") or plain.startswith("[SPA]")

def handle_line(line):
    output_tail.append(line)
    if compact_line(line):
        sys.stdout.write(line)
        sys.stdout.flush()

master_fd, slave_fd = pty.openpty()
try:
    proc = subprocess.Popen(
        [exe] + argv,
        stdout=slave_fd,
        stderr=slave_fd,
        close_fds=True,
    )
finally:
    os.close(slave_fd)

pending = ""
while True:
    try:
        chunk = os.read(master_fd, 4096)
    except OSError:
        break
    if not chunk:
        break
    pending += chunk.decode("utf-8", errors="replace")
    lines = pending.splitlines(keepends=True)
    if lines and not lines[-1].endswith(("\\n", "\\r")):
        pending = lines.pop()
    else:
        pending = ""
    for line in lines:
        handle_line(line)

if pending:
    handle_line(pending)
os.close(master_fd)

result = proc.wait()

if result != 0:
    sys.stderr.write(f"dsmc-iac: DSMC/SPARTA exited with status {{result}}. Captured output follows.\\n")
    if output_tail:
        sys.stderr.write("--- captured output ---\\n")
        sys.stderr.write("".join(output_tail))

raise SystemExit(result)
"""
    args.wrapper.parent.mkdir(parents=True, exist_ok=True)
    if args.wrapper.exists() or args.wrapper.is_symlink():
        args.wrapper.unlink()
    args.wrapper.write_text(script)
    args.wrapper.chmod(0o755)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
