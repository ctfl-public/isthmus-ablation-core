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
import sys

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

if not verbose and not has_screen:
    # Quiet native SPARTA terminal chatter by default. IAC bridge summaries still
    # print to stdout; native SPARTA output remains in the log file.
    argv = ["-screen", "none"] + argv

if color is not None:
    os.environ["IAC_COLOR"] = color
elif "IAC_COLOR" not in os.environ:
    os.environ["IAC_COLOR"] = "auto"

os.execv(exe, [exe] + argv)
"""
    args.wrapper.parent.mkdir(parents=True, exist_ok=True)
    if args.wrapper.exists() or args.wrapper.is_symlink():
        args.wrapper.unlink()
    args.wrapper.write_text(script)
    args.wrapper.chmod(0o755)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
