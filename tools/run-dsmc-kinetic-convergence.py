#!/usr/bin/env python3
"""Run DSMC/IAC collision-flux sphere cases at several voxel resolutions."""

from __future__ import annotations

import argparse
import csv
import math
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

KB = 1.380649e-23
AVOGADRO = 6.02214076e23


@dataclass(frozen=True)
class Result:
  resolution: int
  time: float
  steps: int
  mass_fraction: float
  exact_mass_fraction: float
  mass_error_percent: float
  volume_fraction: float
  volume_error_percent: float
  radius: float
  exact_radius: float
  radius_error_percent: float
  runtime_seconds: float
  history: Path
  input_path: Path


def parse_int_list(text: str, name: str) -> list[int]:
  values = [int(item.strip()) for item in text.split(",") if item.strip()]
  if not values or any(value <= 0 for value in values):
    raise SystemExit(f"--{name} must contain positive integers")
  return values


def replace_line(text: str, prefix: str, replacement: str) -> str:
  lines = []
  found = False
  for line in text.splitlines():
    if line.strip().startswith(prefix):
      lines.append(replacement)
      found = True
    else:
      lines.append(line)
  if not found:
    raise SystemExit(f"template is missing line starting with: {prefix}")
  return "\n".join(lines) + "\n"


def read_history(path: Path) -> list[dict[str, str]]:
  with path.open(newline="") as handle:
    rows = list(csv.DictReader(handle))
  if not rows:
    raise SystemExit(f"history file is empty: {path}")
  return rows


def kinetic_flux(args: argparse.Namespace) -> tuple[float, float, float]:
  gamma = args.number_density * math.sqrt(
      KB * args.temperature / (2.0 * math.pi * args.molecular_mass)
  )
  solid_mass_per_hit = args.solid_atoms_per_hit * args.solid_molar_mass / AVOGADRO
  flux = gamma * args.reaction_prob * solid_mass_per_hit
  speed = flux / args.solid_density
  return gamma, flux, speed


def target_time(args: argparse.Namespace, speed: float) -> float:
  if args.target_mass_fraction <= 0.0 or args.target_mass_fraction >= 1.0:
    raise SystemExit("--target-mass-fraction must be between 0 and 1")
  return (
      args.initial_radius * (1.0 - args.target_mass_fraction ** (1.0 / 3.0))
      / speed
  )


def exact_values(time: float, initial_radius: float,
                 speed: float) -> tuple[float, float]:
  radius = max(initial_radius - speed * time, 0.0)
  mass = (radius / initial_radius) ** 3 if initial_radius > 0.0 else 0.0
  return mass, radius


def write_input(template: str, path: Path, resolution: int,
                args: argparse.Namespace, target: float) -> None:
  species = Path.cwd() / "examples/dsmc-sphere-kinetic/air.species"
  history = path.parent / "history.csv"
  text = template
  text = replace_line(text, "shell", f"shell               mkdir {path.parent}")
  text = replace_line(
      text,
      "global",
      f"global              nrho {args.number_density:.8g} fnum {args.fnum:.8g} "
      f"gridcut 0.0 surfmax {args.surfmax} splitmax {args.splitmax} comm/sort yes",
  )
  text = replace_line(text, "timestep", f"timestep            {args.dsmc_dt:.8g}")
  box = args.domain_half_width
  text = replace_line(
      text,
      "create_box",
      f"create_box          {-box:.10g} {box:.10g} {-box:.10g} {box:.10g} {-box:.10g} {box:.10g}",
  )
  text = replace_line(
      text,
      "create_grid",
      f"create_grid         {args.grid_cells} {args.grid_cells} {args.grid_cells}",
  )
  text = replace_line(text, "balance_grid", "balance_grid        rcb cell")
  text = replace_line(text, "species", f"species             {species} O2")
  text = replace_line(
      text,
      "voxel_material",
      f"voxel_material      carbon density {args.solid_density:.10g} "
      f"molar-mass {args.solid_molar_mass:.10g} formula C",
  )
  text = replace_line(
      text,
      "voxel_create",
      f"voxel_create        solid sphere diameter {args.sphere_diameter:.10g} resolution {resolution} material carbon",
  )
  text = replace_line(
      text,
      "variable            ablation_time",
      f"variable            ablation_time equal {target:.17g}",
  )
  text = replace_line(
      text,
      "fix                 sflux",
      f"fix                 sflux ave/surf all 1 {args.sample_steps} "
      f"{args.sample_steps} c_sflux[*] ave one",
  )
  text = replace_line(text, "run",
                      f"run                 {args.sample_steps} post no")
  text = replace_line(
      text,
      "surf_flux",
      "surf_flux           skin dsmc/surf fix sflux "
      f"mass-courant {args.mass_courant:.10g}",
  )
  text = replace_line(
      text,
      "voxel_write_history",
      f"voxel_write_history solid {history}",
  )
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(text, encoding="utf-8")


def run_case(dsmc: Path, template: str, out_dir: Path, resolution: int,
             args: argparse.Namespace) -> Result:
  case_dir = out_dir / f"resolution-{resolution:03d}"
  input_path = case_dir / f"in.dsmc-sphere-kinetic-resolution-{resolution:03d}"
  _, flux, speed = kinetic_flux(args)
  target = target_time(args, speed)
  runtime = 0.0
  last: dict[str, str] | None = None
  time_value = 0.0
  write_input(template, input_path, resolution, args, target)
  command = [
      str(dsmc), "-screen", "none", "-log", str(case_dir / "log.sparta"),
      "-in", str(input_path)
  ]
  start = time.perf_counter()
  completed = subprocess.run(command, text=True, capture_output=True, timeout=args.timeout)
  runtime += time.perf_counter() - start
  if completed.returncode != 0:
    print(completed.stdout, end="")
    print(completed.stderr, end="")
    raise SystemExit(completed.returncode)

  rows = read_history(case_dir / "history.csv")
  last = rows[-1]
  time_value = float(last["time"])
  steps = int(last["step"])

  if last is None:
    raise SystemExit("DSMC case did not produce history output")
  exact_mass, exact_radius = exact_values(time_value, args.initial_radius, speed)
  actual_mass = float(last["mass-fraction"])
  actual_volume = float(last["volume-fraction"])
  actual_radius = float(last["radius"])
  mass_error = (
      math.inf if exact_mass == 0.0
      else 100.0 * abs(actual_mass - exact_mass) / abs(exact_mass)
  )
  volume_error = (
      math.inf if exact_mass == 0.0
      else 100.0 * abs(actual_volume - exact_mass) / abs(exact_mass)
  )
  radius_error = (
      math.inf if exact_radius == 0.0
      else 100.0 * abs(actual_radius - exact_radius) / abs(exact_radius)
  )
  return Result(
      resolution, time_value, steps, actual_mass, exact_mass, mass_error,
      actual_volume, volume_error,
      actual_radius, exact_radius, radius_error, runtime,
      case_dir / "history.csv", input_path,
  )


def write_summary(path: Path, results: list[Result]) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  with path.open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow([
        "resolution", "time", "mass-fraction", "exact-mass-fraction",
        "mass-error-percent", "volume-fraction", "volume-error-percent", "radius", "exact-radius",
        "radius-error-percent", "steps", "runtime-seconds",
        "history", "input"
    ])
    for result in results:
      writer.writerow([
          result.resolution, f"{result.time:.17g}",
          f"{result.mass_fraction:.17g}",
          f"{result.exact_mass_fraction:.17g}",
          f"{result.mass_error_percent:.17g}",
          f"{result.volume_fraction:.17g}",
          f"{result.volume_error_percent:.17g}",
          f"{result.radius:.17g}", f"{result.exact_radius:.17g}",
          f"{result.radius_error_percent:.17g}",
          result.steps,
          f"{result.runtime_seconds:.17g}", result.history, result.input_path,
      ])


def coords(values: list[tuple[float, float]]) -> str:
  return "\n".join(f"({x:.17g},{y:.17g})" for x, y in values)


def convergence_rows(results: list[Result]) -> str:
  rows = []
  for left, right in zip(results, results[1:]):
    if right.mass_error_percent <= 0.0 or left.mass_error_percent <= 0.0:
      order = "n/a"
    else:
      ratio = right.resolution / left.resolution
      order = f"{math.log(left.mass_error_percent / right.mass_error_percent) / math.log(ratio):.3g}"
    rows.append(
        f"{left.resolution}$\\rightarrow${right.resolution} & "
        f"{left.mass_error_percent:.4g}$\\rightarrow${right.mass_error_percent:.4g} & "
        f"{order}\\\\"
    )
  return "\n".join(rows)


def trajectory_plots(results: list[Result], initial_radius: float, speed: float,
                     field: str) -> str:
  plots = []
  max_time = max(result.time for result in results)
  exact_count = 160
  exact_points = [
      (
          max_time * i / (exact_count - 1),
          exact_values(max_time * i / (exact_count - 1), initial_radius, speed)[0],
      )
      for i in range(exact_count)
  ]
  plots.append(
      r"\addplot[black,dashed,thick,mark=none] coordinates {" + "\n" +
      coords(exact_points) + "\n};\n" +
      r"\addlegendentry{analytical}"
  )
  for result in results:
    rows = read_history(result.history)
    actual = []
    for row in rows:
      time_value = float(row["time"])
      actual.append((time_value, float(row[field])))
    plot_style = r"\addplot+[mark=none] coordinates {"
    if field == "volume-fraction":
      plot_style = r"\addplot+[const plot,mark=none] coordinates {"
    plots.append(
        plot_style + "\n" +
        coords(actual) + "\n};\n" +
        rf"\addlegendentry{{{result.resolution} voxels}}"
    )
  return "\n".join(plots)


def write_report(path: Path, results: list[Result],
                 args: argparse.Namespace) -> None:
  gamma, flux, speed = kinetic_flux(args)
  rows = "\n".join(
      f"{r.resolution} & {r.steps} & {r.time:.4e} & {r.mass_fraction:.6f} & "
      f"{r.volume_fraction:.6f} & {r.exact_mass_fraction:.6f} & {r.mass_error_percent:.4g} & "
      f"{r.volume_error_percent:.4g} & "
      f"{r.radius_error_percent:.4g} & {r.runtime_seconds:.2f}\\\\"
      for r in results
  )
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(
      r"""\documentclass{article}
\usepackage[margin=0.75in]{geometry}
\usepackage{booktabs}
\usepackage{pgfplots}
\pgfplotsset{compat=1.18}
\begin{document}
\section*{DSMC Collision-Flux Sphere Grid Refinement}
This report verifies the simplified DSMC/IAC sphere case. SPARTA samples pure
O$_2$ incident number flux on each ISTHMUS triangle using `compute surf ...
nflux\_incident`; IAC converts that per-triangle DSMC flux to carbon mass flux
and ablates voxels with normal carryover. Gas chemistry and gas-gas collisions
are disabled so the target is the ideal-gas one-way thermal impingement rate.

\[
  \Gamma = n\sqrt{\frac{k_B T}{2\pi m}}, \qquad
  j = \Gamma\alpha m_\mathrm{solid/hit}, \qquad
  R(t) = R_0 - \frac{j}{\rho_s}t, \qquad
  \frac{m(t)}{m_0} = \left(\frac{R(t)}{R_0}\right)^3.
\]

Here $\Gamma = """
      + f"{gamma:.6e}"
      + r"""$ m$^{-2}$s$^{-1}$, $j = """
      + f"{flux:.6e}"
      + r"""$ kg m$^{-2}$s$^{-1}$, and $j/\rho_s = """
      + f"{speed:.6e}"
      + r"""$ m/s. The DSMC sample uses """
      + f"{args.sample_steps}"
      + r""" steps per ablation update in a cubic periodic box with half-width """
      + f"{args.domain_half_width:.6g}"
      + r""" m and a DSMC grid of """
      + f"{args.grid_cells}"
      + r""" cells per direction. Each generated input uses the surface
mass Courant number """
      + f"{args.mass_courant:g}"
      + r""" so the runtime solid timestep is $\Delta t = C_m\rho L_v /
  \max(j_\triangle)$, where $j_\triangle$ is the largest sampled triangle mass
  flux that maps to an active voxel. This is the mass removed through one voxel
  face divided by one voxel mass, matching the standalone mass-Courant
  definition. The input specifies the physical ablation time corresponding to
  target final mass fraction """
      + f"{args.target_mass_fraction:g}"
      + r""".

\begin{center}
\begin{tabular}{rrrrrrrrrr}
\toprule
resolution & steps & time (s) & actual $m/m_0$ & voxel $V/V_0$ & exact &
mass error (\%) & volume error (\%) & radius error (\%) & runtime (s)\\
\midrule
"""
      + rows
      + r"""
\bottomrule
\end{tabular}
\end{center}

\begin{center}
\begin{tabular}{lll}
\toprule
resolution pair & mass error (\%) & observed order\\
\midrule
"""
      + convergence_rows(results)
      + r"""
\bottomrule
\end{tabular}
\end{center}

\begin{figure}[ht!]
\centering
\begin{minipage}{0.48\linewidth}
\centering
\begin{tikzpicture}
\begin{axis}[
  width=\linewidth,
  height=0.72\linewidth,
  xlabel={ablation time (s)},
  ylabel={$m/m_0$},
  ymin=0,
  ymax=1.03,
  grid=both,
  legend style={at={(0.03,0.03)},anchor=south west,font=\scriptsize}]
"""
      + trajectory_plots(results, args.initial_radius, speed, "mass-fraction")
      + r"""
\end{axis}
\end{tikzpicture}
\end{minipage}
\hfill
\begin{minipage}{0.48\linewidth}
\centering
\begin{tikzpicture}
\begin{axis}[
  width=\linewidth,
  height=0.72\linewidth,
  xlabel={ablation time (s)},
  ylabel={voxelized volume fraction},
  ymin=0,
  ymax=1.03,
  grid=both,
  legend style={draw=none,fill=none,at={(0.03,0.03)},anchor=south west,font=\scriptsize}]
"""
      + trajectory_plots(results, args.initial_radius, speed, "volume-fraction")
      + r"""
\end{axis}
\end{tikzpicture}
\end{minipage}
\caption{Sphere recession using DSMC collision flux and conservative normal-directed carryover.}
\end{figure}
\end{document}
""",
      encoding="utf-8",
  )


def maybe_build_pdf(tex: Path) -> None:
  completed = subprocess.run(
      ["pdflatex", "-interaction=nonstopmode", "-halt-on-error", tex.name],
      cwd=tex.parent,
      text=True,
      capture_output=True,
  )
  if completed.returncode != 0:
    print(completed.stdout, end="")
    print(completed.stderr, end="")
    raise SystemExit(completed.returncode)


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--dsmc", type=Path, required=True)
  parser.add_argument("--template", type=Path,
                      default=Path("examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic"))
  parser.add_argument("--out", type=Path,
                      default=Path("build/output/dsmc-sphere-kinetic-grid-convergence"))
  parser.add_argument("--resolutions", default="5,10,20")
  parser.add_argument("--sample-steps", type=int, default=20)
  parser.add_argument("--mass-courant", type=float, default=0.5)
  parser.add_argument("--fnum", type=float, default=2.0e12)
  parser.add_argument("--dsmc-dt", type=float, default=1.0e-7)
  parser.add_argument("--domain-half-width", type=float, default=6.5e-4)
  parser.add_argument("--grid-cells", type=int, default=6)
  parser.add_argument("--surfmax", type=int, default=4000)
  parser.add_argument("--splitmax", type=int, default=400)
  parser.add_argument("--timeout", type=float, default=20.0)
  parser.add_argument("--tolerance-percent", type=float, default=25.0)
  parser.add_argument("--pdf", action="store_true")
  parser.add_argument("--number-density", type=float, default=7.244e23)
  parser.add_argument("--temperature", type=float, default=5000.0)
  parser.add_argument("--molecular-mass", type=float, default=5.31352e-26)
  parser.add_argument("--solid-density", type=float, default=1800.0)
  parser.add_argument("--solid-molar-mass", type=float, default=0.0120107)
  parser.add_argument("--solid-atoms-per-hit", type=float, default=1.0)
  parser.add_argument("--reaction-file", default="carbon-co.surf")
  parser.add_argument("--reaction-prob", type=float, default=1.0)
  parser.add_argument("--initial-radius", type=float, default=5.0e-4)
  parser.add_argument("--sphere-diameter", type=float, default=1.0e-3)
  parser.add_argument("--target-mass-fraction", type=float, default=0.2)
  parser.add_argument("--require-improvement", action="store_true")
  args = parser.parse_args()

  if args.sample_steps <= 0:
    raise SystemExit("--sample-steps must be positive")
  if args.mass_courant <= 0.0:
    raise SystemExit("--mass-courant must be positive")
  if args.domain_half_width <= 0.5 * args.sphere_diameter:
    raise SystemExit("--domain-half-width must exceed the sphere radius")
  if args.grid_cells <= 0:
    raise SystemExit("--grid-cells must be positive")
  if args.timeout <= 0.0:
    raise SystemExit("--timeout must be positive")
  if not args.dsmc.exists():
    raise SystemExit(f"missing DSMC executable: {args.dsmc}")

  template = args.template.read_text(encoding="utf-8")
  if args.out.exists():
    shutil.rmtree(args.out)
  results = [
      run_case(args.dsmc, template, args.out, resolution, args)
      for resolution in parse_int_list(args.resolutions, "resolutions")
  ]
  write_summary(args.out / "summary.csv", results)
  write_report(args.out / "report.tex", results, args)
  if args.pdf:
    maybe_build_pdf(args.out / "report.tex")

  print(f"Wrote {args.out / 'summary.csv'}")
  print(f"Wrote {args.out / 'report.tex'}")
  if args.pdf:
    print(f"Wrote {args.out / 'report.pdf'}")
  for result in results:
    print(
        f"resolution {result.resolution}: mass error "
        f"{result.mass_error_percent:.6g}%, radius error "
        f"{result.radius_error_percent:.6g}%, volume error "
        f"{result.volume_error_percent:.6g}%, runtime "
        f"{result.runtime_seconds:.3g}s"
    )

  finest = results[-1]
  finest_error = max(
      finest.mass_error_percent,
      finest.radius_error_percent,
      finest.volume_error_percent,
  )
  if finest_error > args.tolerance_percent:
    print(
        f"FAILED: finest error {finest_error:.6g}% exceeds "
        f"{args.tolerance_percent:g}%"
    )
    return 1
  if (args.require_improvement and len(results) > 1 and
      finest.mass_error_percent >= results[0].mass_error_percent):
    print(
        "FAILED: finest mass error did not improve over coarsest: "
        f"{results[0].mass_error_percent:.6g}% -> "
        f"{finest.mass_error_percent:.6g}%"
    )
    return 1
  print(
      f"PASSED: finest error {finest_error:.6g}% is within "
      f"{args.tolerance_percent:g}%"
  )
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
