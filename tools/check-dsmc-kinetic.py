#!/usr/bin/env python3
"""Compare DSMC-hosted kinetic sphere recession history to the continuum limit."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

KB = 1.380649e-23


def read_last_row(path: Path) -> dict[str, str]:
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit(f"history file is empty: {path}")
    return rows[-1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("history", type=Path)
    parser.add_argument("--number-density", type=float, required=True)
    parser.add_argument("--temperature", type=float, required=True)
    parser.add_argument("--molecular-mass", type=float, required=True)
    parser.add_argument("--solid-density", type=float, required=True)
    parser.add_argument("--solid-mass-per-hit", type=float, required=True)
    parser.add_argument("--reaction-prob", type=float, default=1.0)
    parser.add_argument("--initial-radius", type=float, required=True)
    parser.add_argument("--tolerance-percent", type=float, default=None)
    args = parser.parse_args()

    row = read_last_row(args.history)
    time = float(row["time"])
    actual = float(row["mass-fraction"])

    gamma = args.number_density * math.sqrt(KB * args.temperature /
                                            (2.0 * math.pi * args.molecular_mass))
    flux = gamma * args.reaction_prob * args.solid_mass_per_hit
    speed = flux / args.solid_density
    radius = max(args.initial_radius - speed * time, 0.0)
    exact = (radius / args.initial_radius) ** 3
    error = math.inf if exact == 0.0 else 100.0 * abs(actual - exact) / abs(exact)

    print(f"time = {time:.17g}")
    print(f"mass-fraction actual = {actual:.17g}")
    print(f"mass-fraction exact = {exact:.17g}")
    print(f"error = {error:.6g} percent")
    print(f"continuum number flux = {gamma:.17g} 1/m2/s")
    print(f"continuum mass flux = {flux:.17g} kg/m2/s")

    if args.tolerance_percent is not None and error > args.tolerance_percent:
        print(f"FAILED: error exceeds {args.tolerance_percent:g} percent")
        return 1
    if args.tolerance_percent is not None:
        print(f"PASSED: error is within {args.tolerance_percent:g} percent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
