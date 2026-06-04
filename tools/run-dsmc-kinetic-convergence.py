#!/usr/bin/env python3
"""Run the DSMC-hosted sphere case at several sampling lengths."""

from __future__ import annotations

import argparse
import csv
import math
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

KB = 1.380649e-23


@dataclass(frozen=True)
class Result:
  steps: int
  time: float
  actual: float
  exact: float
  error_percent: float
  radius: float
  exact_radius: float
  radius_error_percent: float
  requested_mass: float
  history: Path
  input_path: Path


@dataclass(frozen=True)
class HistoryPoint:
  time: float
  mass_fraction: float
  exact_mass_fraction: float
  radius: float
  exact_radius: float


def parse_int_list(text: str, name: str) -> list[int]:
  steps = [int(item.strip()) for item in text.split(",") if item.strip()]
  if not steps or any(step <= 0 for step in steps):
    raise SystemExit(f"--{name} must contain positive integers")
  return steps


def read_history(path: Path) -> list[dict[str, str]]:
  with path.open(newline="") as handle:
    rows = list(csv.DictReader(handle))
  if not rows:
    raise SystemExit(f"history file is empty: {path}")
  return rows


def kinetic_quantities(args: argparse.Namespace) -> tuple[float, float, float]:
  gamma = args.number_density * math.sqrt(KB * args.temperature /
                                          (2.0 * math.pi * args.molecular_mass))
  flux = gamma * args.reaction_prob * args.solid_mass_per_hit
  speed = flux / args.solid_density
  return gamma, flux, speed


def exact_values(time: float, initial_radius: float,
                 speed: float) -> tuple[float, float]:
  radius = max(initial_radius - speed * time, 0.0)
  mass_fraction = (radius / initial_radius) ** 3 if initial_radius > 0.0 else 0.0
  return mass_fraction, radius


def write_input(template: str, path: Path, steps: int, loops: int, history: Path,
                ablation_dt: float | None, mass_courant: float | None, fnum: float,
                resolution: int, ledger_steps: int, bad_edges: int) -> None:
  text = template
  text = text.replace("fnum 1.0e14", f"fnum {fnum:.8g}")
  if bad_edges > 0:
    text = text.replace("gridcut 0.0 surfmax", f"gridcut 0.0 nedgebadnum {bad_edges} surfmax")
  text = text.replace(
      "voxel create solid sphere diameter 1.0e-3 resolution 20 material carbon",
      f"voxel create solid sphere diameter 1.0e-3 resolution {resolution} material carbon",
  )
  text = text.replace(
      "fix                 sflux ave/surf all 1 5 5 c_sflux[*] ave one",
      f"fix                 sflux ave/surf all 1 {steps} {steps} c_sflux[*] ave one",
  )
  text = text.replace("variable            i loop 2", f"variable            i loop {loops}")
  text = text.replace(
      "voxel write-history solid output/dsmc-sphere-kinetic/history.csv",
      f"voxel write-history solid {history}",
  )
  flux_suffix = ""
  if mass_courant is not None:
    flux_suffix = f" mass-courant {mass_courant:.17g}"
  elif ablation_dt is not None:
    flux_suffix = f" ablation-dt {ablation_dt:.17g}"
  text = text.replace(
      "surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 solid-mass-per-hit 1.99447348e-26",
      "surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 "
      f"solid-mass-per-hit 1.99447348e-26{flux_suffix}",
  )
  if ledger_steps > 1:
    text = text.replace(
        "surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 "
        f"solid-mass-per-hit 1.99447348e-26{flux_suffix}\n"
        "voxel ablate solid surface skin policy carryover/normal delete yes",
        "variable            j loop "
        f"{ledger_steps}\n"
        "label               ledger-loop\n"
        "surface flux skin dsmc/surf fix sflux column 1 reaction-prob 1.0 "
        f"solid-mass-per-hit 1.99447348e-26{flux_suffix}\n"
        "voxel ablate solid surface skin policy carryover/normal delete yes\n"
        "next                j\n"
        "jump                SELF ledger-loop",
    )
  text = text.replace("run                 5", f"run                 {steps}")
  text = text.replace(
      "surface install skin particle check type 1",
      "surface install skin particle none type 1",
  )
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(text, encoding="utf-8")


def run_case(root: Path, dsmc: Path, template: str, out_dir: Path, steps: int,
             loops: int, args: argparse.Namespace) -> Result:
  case_dir = out_dir / f"steps-{steps:04d}"
  history = case_dir / "history.csv"
  input_path = case_dir / f"in.dsmc-sphere-kinetic-steps-{steps:04d}"
  write_input(template, input_path, steps, loops, history, args.ablation_dt,
              args.mass_courant, args.fnum, args.resolution, args.ledger_steps,
              args.bad_edges)

  command = [str(dsmc), "-in", str(input_path)]
  if args.show_dsmc_output:
    completed = subprocess.run(command, cwd=root)
  else:
    completed = subprocess.run(command, cwd=root, text=True, capture_output=True)
  if completed.returncode != 0:
    if not args.show_dsmc_output:
      print(completed.stdout, end="")
      print(completed.stderr, end="")
    raise SystemExit(completed.returncode)

  rows = read_history(history)
  row = rows[-1]
  _, _, speed = kinetic_quantities(args)
  initial_radius = float(rows[0]["radius"])
  time = float(row["time"])
  actual = float(row["mass-fraction"])
  radius = float(row["radius"])
  exact, exact_radius = exact_values(time, initial_radius, speed)
  error = math.inf if exact == 0.0 else 100.0 * abs(actual - exact) / abs(exact)
  radius_error = (math.inf if exact_radius == 0.0
                  else 100.0 * abs(radius - exact_radius) / abs(exact_radius))
  requested = float(row["requested-mass-step"])
  return Result(steps, time, actual, exact, error, radius, exact_radius,
                radius_error, requested, history, input_path)


def write_summary(path: Path, results: list[Result]) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  with path.open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow([
        "dsmc-steps", "time", "mass-fraction", "exact-mass-fraction",
        "error-percent", "radius", "exact-radius", "radius-error-percent",
        "requested-mass-step", "history", "input"
    ])
    for result in results:
      writer.writerow([
          result.steps, f"{result.time:.17g}", f"{result.actual:.17g}",
          f"{result.exact:.17g}", f"{result.error_percent:.17g}",
          f"{result.radius:.17g}", f"{result.exact_radius:.17g}",
          f"{result.radius_error_percent:.17g}",
          f"{result.requested_mass:.17g}", result.history, result.input_path
      ])


def history_points(result: Result, args: argparse.Namespace) -> list[HistoryPoint]:
  rows = read_history(result.history)
  _, _, speed = kinetic_quantities(args)
  initial_radius = float(rows[0]["radius"])
  points = []
  for row in rows:
    time = float(row["time"])
    exact_mass, exact_radius = exact_values(time, initial_radius, speed)
    points.append(
        HistoryPoint(
            time=time,
            mass_fraction=float(row["mass-fraction"]),
            exact_mass_fraction=exact_mass,
            radius=float(row["radius"]),
            exact_radius=exact_radius,
        )
    )
  return points


def coords(points: list[tuple[float, float]]) -> str:
  return "\n".join(f"({x:.17g},{y:.17g})" for x, y in points)


def write_report(path: Path, results: list[Result], args: argparse.Namespace) -> None:
  rows = "\n".join(
      f"{r.steps} & {r.time:.3e} & {r.actual:.10f} & {r.exact:.10f} & "
      f"{r.error_percent:.6g} & {r.radius_error_percent:.6g}\\\\"
      for r in results
  )
  mass_coords = "\n".join(f"({r.steps},{r.error_percent:.17g})" for r in results)
  radius_coords = "\n".join(f"({r.steps},{r.radius_error_percent:.17g})" for r in results)
  histories = [(result, history_points(result, args)) for result in results]
  exact_points = histories[-1][1] if histories else []
  initial_radius = exact_points[0].radius if exact_points else args.initial_radius
  mass_fraction_plots = []
  recession_plots = []
  for result, points in histories:
    mass_fraction_plots.append(
        r"\addplot coordinates {" + "\n" +
        coords([(point.time, point.mass_fraction) for point in points]) +
        "\n};\n" + rf"\addlegendentry{{{result.steps} DSMC steps}}"
    )
    recession_plots.append(
        r"\addplot coordinates {" + "\n" +
        coords([(point.time, 1.0e6 * (initial_radius - point.radius))
                for point in points]) +
        "\n};\n" + rf"\addlegendentry{{{result.steps} DSMC steps}}"
    )
  exact_mass_fraction_plot = (
      r"\addplot[black,dashed,thick] coordinates {" + "\n" +
      coords([(point.time, point.exact_mass_fraction) for point in exact_points]) +
      "\n};\n\\addlegendentry{analytical}"
  )
  exact_recession_plot = (
      r"\addplot[black,dashed,thick] coordinates {" + "\n" +
      coords([(point.time, 1.0e6 * (initial_radius - point.exact_radius))
              for point in exact_points]) +
      "\n};\n\\addlegendentry{analytical}"
  )
  gamma, flux, speed = kinetic_quantities(args)
  pressure = args.number_density * KB * args.temperature
  final_time = results[-1].time if results else 0.0
  final_recession = speed * final_time
  final_mass_loss = 100.0 * (1.0 - exact_values(final_time, initial_radius, speed)[0])
  timestep_text = (
      "The ablation time step is derived from a mass Courant number of "
      f"{args.mass_courant:.3g}. For each coupled update, the bridge maps the "
      "sampled triangle mass fluxes through the ISTHMUS ownership fractions, "
      "finds the largest mass request on any active voxel for a trial one-second "
      "step, and chooses a time step that requests this voxel lose that fraction "
      "of one voxel mass. "
      if args.mass_courant is not None else
      f"The ablation time advanced per coupled update is fixed at {args.ablation_dt:.3e} s."
  )
  if args.ledger_steps > 1:
    timestep_text += (
        f"The generated input applies {args.ledger_steps} voxel mass-ledger "
        "updates for each DSMC/ISTHMUS surface refresh, reusing the last sampled "
        "surface flux between refreshes. "
    )
  if args.bad_edges > 0:
    timestep_text += (
        f"The generated input sets DSMC global nedgebadnum to {args.bad_edges}; "
        "this is an exploratory tolerance for late-stage surface checks. "
    )
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(
      r"""\documentclass{article}
\usepackage[margin=0.8in]{geometry}
\usepackage{booktabs}
\usepackage{pgfplots}
\pgfplotsset{compat=1.18}
\begin{document}
\section*{DSMC Sphere Kinetic Convergence}
This report runs the DSMC-hosted voxel/ISTHMUS sphere case with increasing
numbers of DSMC sampling steps before each coupled ablation update. The comparison
assumes a spatially uniform stationary gas and the continuum shrinking-sphere
solution. More ablation updates than DSMC sampling points are used so the plots
show the coupled trajectory, not just a final three-point comparison. """
      + timestep_text
      + r"""

\subsection*{Physical setup}
The DSMC domain is a periodic cube containing a spatially uniform, stationary
air mixture: 21\% O$_2$ and 79\% N$_2$. The gas number density is """
      + f"{args.number_density:.6e}"
      + r""" m$^{-3}$ and the gas temperature is """
      + f"{args.temperature:.1f}"
      + r""" K, which gives an ideal-gas pressure of """
      + f"{pressure:.6e}"
      + r""" Pa. The surface flux compute samples incident number flux from
the mixture and the bridge converts that flux to carbon mass removal. The
surface chemistry is modeled as a single effective oxidation event. Each
incident gas molecule counted by DSMC is treated as a reactive collision with
probability """
      + f"{args.reaction_prob:.3g}"
      + r""". A successful event removes one carbon atom from the solid,
represented by the solid mass per hit """
      + f"{args.solid_mass_per_hit:.6e}"
      + r""" kg. This is currently a one-way ablation model: the removed carbon
mass is deducted from the voxel ledger, while gas-phase product formation and
feedback on the DSMC chemistry are not yet modeled.

The solid is a 1 mm diameter voxel sphere with """
      + f"{args.resolution}"
      + r""" voxels across the diameter
and density """
      + f"{args.solid_density:.1f}"
      + r""" kg/m$^3$. The equivalent initial voxel radius used for the
analytical comparison is """
      + f"{initial_radius:.6e}"
      + r""" m. In the continuum comparison,
\[
  \Gamma = n \sqrt{\frac{k_B T}{2\pi m}}, \qquad
  j = \Gamma\,\alpha\,m_\mathrm{solid/hit}, \qquad
  R(t) = R_0 - \frac{j}{\rho_s}t, \qquad
  \frac{m(t)}{m_0} = \left(\frac{R(t)}{R_0}\right)^3.
\]
For this case, $j = """
      + f"{flux:.6e}"
      + r"""$ kg/m$^2$/s and $j/\rho_s = """
      + f"{speed:.6e}"
      + r"""$ m/s. The final analytical recession in this short convergence
case is only """
      + f"{1.0e6 * final_recession:.6e}"
      + r""" microns, corresponding to """
      + f"{final_mass_loss:.6e}"
      + r"""\% mass loss. The plots show remaining mass fraction and radius
recession.

\begin{center}
\begin{tabular}{rrrrrr}
\toprule
DSMC steps & time (s) & actual $m/m_0$ & exact $m/m_0$ &
mass error (\%) & radius error (\%)\\
\midrule
"""
      + rows
      + r"""
\bottomrule
\end{tabular}
\end{center}

\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.78\linewidth,
  height=0.42\linewidth,
  xlabel={ablation time (s)},
  ylabel={remaining mass fraction, $m/m_0$},
  grid=both,
  legend pos=north west]
"""
      + "\n".join(mass_fraction_plots)
      + "\n"
      + exact_mass_fraction_plot
      + r"""
\end{axis}
\end{tikzpicture}
\end{center}

\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.78\linewidth,
  height=0.42\linewidth,
  xlabel={ablation time (s)},
  ylabel={radius recession ($\mu$m)},
  grid=both,
  legend pos=north west]
"""
      + "\n".join(recession_plots)
      + "\n"
      + exact_recession_plot
      + r"""
\end{axis}
\end{tikzpicture}
\end{center}

\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.78\linewidth,
  height=0.42\linewidth,
  xlabel={DSMC steps before ablation update},
  ylabel={error (\%)},
  xmode=log,
  ymode=log,
  grid=both,
  legend pos=north east]
\addplot coordinates {
"""
      + mass_coords
      + r"""
};
\addlegendentry{mass fraction}
\addplot coordinates {
"""
      + radius_coords
      + r"""
};
\addlegendentry{radius}
\end{axis}
\end{tikzpicture}
\end{center}

\noindent Tolerance used for the test: """
      + f"{args.tolerance_percent:g}"
      + r"""\%.
\end{document}
""",
      encoding="utf-8",
  )


def maybe_build_pdf(tex: Path, show_output: bool) -> None:
  if show_output:
    completed = subprocess.run(
        ["pdflatex", "-interaction=nonstopmode", "-halt-on-error", tex.name],
        cwd=tex.parent,
    )
  else:
    completed = subprocess.run(
        ["pdflatex", "-interaction=nonstopmode", "-halt-on-error", tex.name],
        cwd=tex.parent,
        text=True,
        capture_output=True,
    )
  if completed.returncode != 0:
    if not show_output:
      print(completed.stdout, end="")
      print(completed.stderr, end="")
    raise SystemExit(completed.returncode)


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--dsmc", type=Path, required=True)
  parser.add_argument("--template", type=Path,
                      default=Path("examples/dsmc-sphere-kinetic/in.dsmc-sphere-kinetic"))
  parser.add_argument("--out", type=Path,
                      default=Path("build/output/dsmc-sphere-kinetic-convergence"))
  parser.add_argument("--steps", default="1,2,5,10,20,50")
  parser.add_argument("--loops", type=int, default=10)
  parser.add_argument(
      "--resolution",
      type=int,
      default=20,
      help="Number of voxels across the 1 mm sphere diameter in generated inputs.",
  )
  parser.add_argument(
      "--ledger-steps",
      type=int,
      default=1,
      help="Voxel mass-ledger updates to apply per DSMC/ISTHMUS surface refresh.",
  )
  parser.add_argument(
      "--bad-edges",
      type=int,
      default=0,
      help="Set DSMC global nedgebadnum in generated inputs for exploratory late-stage surfaces.",
  )
  parser.add_argument(
      "--fnum",
      type=float,
      default=1.0e13,
      help="DSMC particle weight used in generated convergence inputs.",
  )
  parser.add_argument(
      "--ablation-dt",
      type=float,
      default=None,
      help="Physical ablation time advanced after each DSMC sampling run.",
  )
  parser.add_argument(
      "--mass-courant",
      type=float,
      default=0.25,
      help="Choose each ablation timestep from mapped DSMC flux so the largest voxel request is this fraction of one voxel mass.",
  )
  parser.add_argument("--tolerance-percent", type=float, default=1.0)
  parser.add_argument("--pdf", action="store_true")
  parser.add_argument("--show-dsmc-output", action="store_true")
  parser.add_argument("--number-density", type=float, default=7.244e23)
  parser.add_argument("--temperature", type=float, default=5000.0)
  parser.add_argument("--molecular-mass", type=float, default=4.753323e-26)
  parser.add_argument("--solid-density", type=float, default=1800.0)
  parser.add_argument("--solid-mass-per-hit", type=float, default=1.99447348e-26)
  parser.add_argument("--reaction-prob", type=float, default=1.0)
  parser.add_argument("--initial-radius", type=float, default=5.0e-4)
  args = parser.parse_args()
  if args.ablation_dt is not None and args.mass_courant is not None:
    raise SystemExit("--ablation-dt and --mass-courant are mutually exclusive")
  if args.resolution <= 0:
    raise SystemExit("--resolution must be positive")
  if args.ledger_steps <= 0:
    raise SystemExit("--ledger-steps must be positive")
  if args.bad_edges < 0:
    raise SystemExit("--bad-edges must be nonnegative")

  root = Path.cwd()
  dsmc = args.dsmc
  if not dsmc.exists():
    raise SystemExit(f"missing DSMC executable: {dsmc}")
  template = args.template.read_text(encoding="utf-8")
  if args.out.exists():
    shutil.rmtree(args.out)

  results = [
      run_case(root, dsmc, template, args.out, steps, args.loops, args)
      for steps in parse_int_list(args.steps, "steps")
  ]
  summary = args.out / "summary.csv"
  report = args.out / "report.tex"
  write_summary(summary, results)
  write_report(report, results, args)
  if args.pdf:
    maybe_build_pdf(report, args.show_dsmc_output)

  print(f"Wrote {summary}")
  print(f"Wrote {report}")
  if args.pdf:
    print(f"Wrote {report.with_suffix('.pdf')}")
  finest = results[-1]
  finest_error = max(finest.error_percent, finest.radius_error_percent)
  if finest_error > args.tolerance_percent:
    print(
        f"FAILED: finest error {finest_error:.6g}% exceeds "
        f"{args.tolerance_percent:g}%"
    )
    return 1
  print(
      f"PASSED: finest error {finest_error:.6g}% is within "
      f"{args.tolerance_percent:g}%"
  )
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
