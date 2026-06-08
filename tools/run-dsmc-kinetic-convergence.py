#!/usr/bin/env python3
"""Run the DSMC-hosted CO-forming sphere case at several sampling lengths."""

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


@dataclass(frozen=True)
class Result:
  steps: int
  time: float
  mass_fraction: float
  exact_mass_fraction: float
  mass_error_percent: float
  radius: float
  exact_radius: float
  radius_error_percent: float
  requested_mass: float
  reactions: float
  runtime_seconds: float
  history: Path
  input_path: Path


@dataclass(frozen=True)
class HistoryPoint:
  time: float
  mass_fraction: float
  exact_mass_fraction: float
  recession_um: float
  exact_recession_um: float


def parse_int_list(text: str, name: str) -> list[int]:
  values = [int(item.strip()) for item in text.split(",") if item.strip()]
  if not values or any(value <= 0 for value in values):
    raise SystemExit(f"--{name} must contain positive integers")
  return values


def read_history(path: Path) -> list[dict[str, str]]:
  with path.open(newline="") as handle:
    rows = list(csv.DictReader(handle))
  if not rows:
    raise SystemExit(f"history file is empty: {path}")
  return rows


def reaction_quantities(args: argparse.Namespace) -> tuple[float, float, float]:
  o2_number_density = args.number_density * args.o2_fraction
  gamma = o2_number_density * math.sqrt(KB * args.temperature /
                                        (2.0 * math.pi * args.o2_mass))
  flux = gamma * args.reaction_probability * args.solid_mass_per_reaction
  recession_speed = flux / args.solid_density
  return gamma, flux, recession_speed


def exact_values(time: float, initial_radius: float,
                 speed: float) -> tuple[float, float]:
  radius = max(initial_radius - speed * time, 0.0)
  mass_fraction = (radius / initial_radius) ** 3 if initial_radius > 0.0 else 0.0
  return mass_fraction, radius


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


def write_input(template: str, path: Path, steps: int,
                args: argparse.Namespace) -> None:
  root = Path.cwd()
  history = path.parent / "history.csv"
  species = root / "examples/dsmc-sphere-kinetic/air.species"
  vss = root / "examples/dsmc-sphere-kinetic/air.vss"
  reaction = path.parent / "carbon-co.surf"
  path.parent.mkdir(parents=True, exist_ok=True)
  reaction.write_text(
      "# Generated CO-forming surface reaction for the convergence case.\n"
      "# O2 + 2 C(s) -> 2 CO is represented as one O2 reaction event.\n\n"
      "O2 --> CO + CO\n"
      f"D S {args.reaction_probability:.12g}\n",
      encoding="utf-8",
  )

  time_scale = args.ablation_update_time / (steps * args.dsmc_dt)
  text = template
  text = replace_line(text, "shell", f"shell               mkdir {path.parent}")
  text = replace_line(
      text,
      "global",
      f"global              nrho {args.number_density:.8g} fnum {args.fnum:.8g} "
      "gridcut 0.0 surfmax 2000 splitmax 200 comm/sort yes",
  )
  text = replace_line(text, "timestep", f"timestep            {args.dsmc_dt:.8g}")
  text = replace_line(text, "boundary", "boundary            o o o")
  text = replace_line(
      text,
      "species",
      f"species             {species} O2 N2 CO",
  )
  text = replace_line(text, "mixture             air O2",
                      f"mixture             air O2 frac {args.o2_fraction:.8g}")
  text = replace_line(text, "mixture             air N2",
                      f"mixture             air N2 frac {1.0 - args.o2_fraction:.8g}")
  text = replace_line(text, "mixture             air temp",
                      f"mixture             air temp {args.temperature:.8g} vstream 0.0 0.0 0.0")
  text = replace_line(text, "collide", f"collide             vss air {vss}")
  text = replace_line(
      text,
      "create_particles",
      "create_particles    air n 0 twopass\n"
      "fix                 reservoir emit/face air all twopass",
  )
  text = replace_line(
      text,
      "voxel create",
      f"voxel create solid sphere diameter 1.0e-3 resolution {args.resolution} material carbon",
  )
  text = replace_line(text, "surf_react",
                      f"surf_react          ox prob {reaction}")
  text = replace_line(text, "surf_collide",
                      f"surf_collide        1 diffuse {args.wall_temperature:.8g} 1.0")
  text = replace_line(
      text,
      "fix                 rco",
      f"fix                 rco ave/surf all 1 {steps} {steps} c_rco[*] ave one",
  )
  text = replace_line(text, "variable            i loop",
                      f"variable            i loop {args.loops}")
  text = replace_line(text, "run", f"run                 {steps} post no")
  text = replace_line(
      text,
      "surface flux",
      "surface flux skin dsmc/reaction fix rco column 1 "
      f"sample-steps {steps} "
      f"solid-mass-per-reaction {args.solid_mass_per_reaction:.10e} "
      f"time-scale {time_scale:.10g}",
  )
  text = replace_line(
      text,
      "voxel write-history",
      f"voxel write-history solid {history}",
  )
  path.write_text(text, encoding="utf-8")


def run_case(dsmc: Path, template: str, out_dir: Path, steps: int,
             args: argparse.Namespace) -> Result:
  case_dir = out_dir / f"steps-{steps:04d}"
  input_path = case_dir / f"in.dsmc-sphere-kinetic-steps-{steps:04d}"
  write_input(template, input_path, steps, args)

  command = [str(dsmc), "-screen", "none", "-log", str(case_dir / "log.sparta"),
             "-in", str(input_path)]
  start = time.perf_counter()
  completed = subprocess.run(command, text=True, capture_output=True)
  runtime_seconds = time.perf_counter() - start
  if completed.returncode != 0:
    print(completed.stdout, end="")
    print(completed.stderr, end="")
    raise SystemExit(completed.returncode)

  rows = read_history(case_dir / "history.csv")
  last = rows[-1]
  _, _, speed = reaction_quantities(args)
  initial_radius = float(rows[0]["radius"])
  ablation_time = float(last["time"])
  exact_mass_fraction, exact_radius = exact_values(ablation_time, initial_radius, speed)
  mass_fraction = float(last["mass-fraction"])
  radius = float(last["radius"])
  mass_error = math.inf if exact_mass_fraction == 0.0 else (
      100.0 * abs(mass_fraction - exact_mass_fraction) / abs(exact_mass_fraction)
  )
  radius_error = math.inf if exact_radius == 0.0 else (
      100.0 * abs(radius - exact_radius) / abs(exact_radius)
  )
  requested = float(last["requested-mass-step"])
  reaction_total = 0.0
  for line in (case_dir / "log.sparta").read_text(encoding="utf-8", errors="ignore").splitlines():
    if "reaction O2 --> CO + CO:" in line:
      reaction_total += float(line.rsplit(":", 1)[1].strip())
  return Result(steps, ablation_time, mass_fraction, exact_mass_fraction, mass_error,
                radius, exact_radius, radius_error, requested, reaction_total,
                runtime_seconds, case_dir / "history.csv", input_path)


def write_summary(path: Path, results: list[Result]) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  with path.open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow([
        "dsmc-steps", "time", "mass-fraction", "exact-mass-fraction",
        "mass-error-percent", "radius", "exact-radius",
        "radius-error-percent", "requested-mass-step", "sampled-reactions",
        "runtime-seconds", "history", "input"
    ])
    for result in results:
      writer.writerow([
          result.steps, f"{result.time:.17g}",
          f"{result.mass_fraction:.17g}",
          f"{result.exact_mass_fraction:.17g}",
          f"{result.mass_error_percent:.17g}", f"{result.radius:.17g}",
          f"{result.exact_radius:.17g}",
          f"{result.radius_error_percent:.17g}",
          f"{result.requested_mass:.17g}", f"{result.reactions:.17g}",
          f"{result.runtime_seconds:.17g}", result.history, result.input_path,
      ])


def coords(points: list[tuple[float, float]]) -> str:
  return "\n".join(f"({x:.17g},{y:.17g})" for x, y in points)


def history_points(result: Result, args: argparse.Namespace) -> list[HistoryPoint]:
  rows = read_history(result.history)
  _, _, speed = reaction_quantities(args)
  initial_radius = float(rows[0]["radius"])
  points = []
  for row in rows:
    time = float(row["time"])
    exact_mass, exact_radius = exact_values(time, initial_radius, speed)
    radius = float(row["radius"])
    points.append(
        HistoryPoint(
            time=time,
            mass_fraction=float(row["mass-fraction"]),
            exact_mass_fraction=exact_mass,
            recession_um=1.0e6 * (initial_radius - radius),
            exact_recession_um=1.0e6 * (initial_radius - exact_radius),
        )
    )
  return points


def plot_series(histories: list[tuple[Result, list[HistoryPoint]]],
                field: str) -> str:
  plots = []
  for result, points in histories:
    if field == "mass":
      values = [(point.time, point.mass_fraction) for point in points]
    else:
      values = [(point.time, point.recession_um) for point in points]
    plots.append(
        r"\addplot coordinates {" + "\n" + coords(values) +
        "\n};\n" + rf"\addlegendentry{{{result.steps} DSMC steps}}"
    )
  exact_points = histories[-1][1]
  if field == "mass":
    values = [(point.time, point.exact_mass_fraction) for point in exact_points]
  else:
    values = [(point.time, point.exact_recession_um) for point in exact_points]
  plots.append(
      r"\addplot[black,dashed,thick] coordinates {" + "\n" + coords(values) +
      "\n};\n" + r"\addlegendentry{analytical}"
  )
  return "\n".join(plots)


def write_report(path: Path, results: list[Result], args: argparse.Namespace) -> None:
  gamma, flux, speed = reaction_quantities(args)
  pressure = args.number_density * KB * args.temperature
  histories = [(result, history_points(result, args)) for result in results]
  final_time = results[-1].time
  initial_radius = results[-1].exact_radius + speed * final_time
  final_exact_mass, _ = exact_values(final_time, initial_radius, speed)
  rows = "\n".join(
      f"{r.steps} & {r.time:.3e} & {r.reactions:.0f} & "
      f"{r.mass_fraction:.6f} & {r.exact_mass_fraction:.6f} & "
      f"{r.mass_error_percent:.4g} & {r.radius_error_percent:.4g} & "
      f"{r.runtime_seconds:.2f}\\\\"
      for r in results
  )
  mass_errors = coords([(result.steps, result.mass_error_percent)
                        for result in results])
  radius_errors = coords([(result.steps, result.radius_error_percent)
                          for result in results])
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(
      r"""\documentclass{article}
\usepackage[margin=0.75in]{geometry}
\usepackage{booktabs}
\usepackage{pgfplots}
\pgfplotsset{compat=1.18}
\begin{document}
\section*{DSMC Sphere CO-Reaction Convergence}
This report runs the DSMC-hosted voxel/ISTHMUS sphere case with a real SPARTA
surface reaction, O$_2 \rightarrow$ CO + CO. The number of DSMC steps between
coupled ablation updates is varied; longer runs provide more surface-reaction
sampling before the sampled reaction counts are mapped back to voxel mass loss.

\subsection*{Physical setup}
The domain is a 3 mm cube with outflow boundaries on all faces and
`fix emit/face air all`, so particles can leave the box while fresh reservoir
gas is emitted from the domain faces. The emitted reservoir gas has O$_2$ mole
fraction """
      + f"{args.o2_fraction:.3g}"
      + r""" and N$_2$ mole fraction """
      + f"{1.0 - args.o2_fraction:.3g}"
      + r""". The number density is """
      + f"{args.number_density:.6e}"
      + r""" m$^{-3}$, the gas temperature is """
      + f"{args.temperature:.1f}"
      + r""" K, and the ideal-gas pressure is """
      + f"{pressure:.6e}"
      + r""" Pa. The solid is a 1 mm diameter carbon voxel sphere with density
"""
      + f"{args.solid_density:.1f}"
      + r""" kg/m$^3$. The surface reaction file uses probability """
      + f"{args.reaction_probability:.3g}"
      + r""", so every incident O$_2$ surface reaction event creates two CO
simulation particles and removes two carbon atoms from the voxel mass ledger.
The solid mass removed per reaction is """
      + f"{args.solid_mass_per_reaction:.6e}"
      + r""" kg.

The analytical comparison assumes the gas remains spatially uniform and
stationary. With O$_2$ number density $n_{O2}$,
\[
  \Gamma_{O2} = n_{O2}\sqrt{\frac{k_B T}{2\pi m_{O2}}}, \qquad
  j = \Gamma_{O2}\alpha m_\mathrm{solid/reaction}, \qquad
  R(t) = R_0 - \frac{j}{\rho_s}t, \qquad
  \frac{m(t)}{m_0} = \left(\frac{R(t)}{R_0}\right)^3.
\]
\begin{center}
\begin{tabular}{ll}
\toprule
Symbol & Meaning\\
\midrule
$\Gamma_{O2}$ & O$_2$ incident number flux to a surface, m$^{-2}$ s$^{-1}$\\
$n_{O2}$ & O$_2$ number density, m$^{-3}$\\
$k_B$ & Boltzmann constant\\
$T$ & gas translational temperature\\
$m_{O2}$ & mass of one O$_2$ molecule\\
$j$ & solid carbon mass-removal flux, kg m$^{-2}$ s$^{-1}$\\
$\alpha$ & O$_2$ surface reaction probability\\
$m_\mathrm{solid/reaction}$ & carbon mass removed by one O$_2$ reaction event\\
$R_0$, $R(t)$ & initial and current equivalent sphere radius\\
$\rho_s$ & solid carbon density\\
$m(t)/m_0$ & remaining solid mass fraction\\
\bottomrule
\end{tabular}
\end{center}
For this setup, $\Gamma_{O2} = """
      + f"{gamma:.6e}"
      + r"""$ m$^{-2}$s$^{-1}$, $j = """
      + f"{flux:.6e}"
      + r"""$ kg m$^{-2}$s$^{-1}$, and $j/\rho_s = """
      + f"{speed:.6e}"
      + r"""$ m/s.

To make a quick verification case, the bridge advances the solid by """
      + f"{args.ablation_update_time:.6e}"
      + r""" s per coupled update. For a case with $N$ DSMC sampling steps,
the bridge time-scale is chosen as $\Delta t_\mathrm{ablate}/(N\Delta
t_\mathrm{DSMC})$, so all convergence cases advance the same ablation time
while sampling for different DSMC durations. This tests reaction-count coupling
and voxel recession against the stationary-reservoir solution. The finest case
ends at analytical remaining mass fraction """
      + f"{final_exact_mass:.6f}"
      + r""".

The reservoir boundary makes this a better comparison to the analytical
constant-gas solution than the earlier closed periodic box. It is still a
finite DSMC domain, so surface sampling noise and local composition/temperature
perturbations can remain.

\subsection*{Final Values}
\begin{center}
\begin{tabular}{rrrrrrrr}
\toprule
DSMC steps & time (s) & reactions & actual $m/m_0$ & exact $m/m_0$ &
mass error (\%) & radius error (\%) & runtime (s)\\
\midrule
"""
      + rows
      + r"""
\bottomrule
\end{tabular}
\end{center}

\subsection*{Trajectories}
\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.74\linewidth,
  height=0.42\linewidth,
  xlabel={ablation time (s)},
  ylabel={remaining mass fraction, $m/m_0$},
  grid=both,
  legend style={at={(1.02,1)},anchor=north west}]
"""
      + plot_series(histories, "mass")
      + r"""
\end{axis}
\end{tikzpicture}
\end{center}

\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.74\linewidth,
  height=0.42\linewidth,
  xlabel={ablation time (s)},
  ylabel={radius recession ($\mu$m)},
  grid=both,
  legend style={at={(1.02,1)},anchor=north west}]
"""
      + plot_series(histories, "radius")
      + r"""
\end{axis}
\end{tikzpicture}
\end{center}

\subsection*{Sampling Error}
\begin{center}
\begin{tikzpicture}
\begin{axis}[
  width=0.74\linewidth,
  height=0.42\linewidth,
  xlabel={DSMC steps before ablation update},
  ylabel={final error (\%)},
  xmode=log,
  ymode=log,
  grid=both,
  legend style={at={(1.02,1)},anchor=north west}]
\addplot coordinates {
"""
      + mass_errors
      + r"""
};
\addlegendentry{mass fraction}
\addplot coordinates {
"""
      + radius_errors
      + r"""
};
\addlegendentry{radius}
\end{axis}
\end{tikzpicture}
\end{center}

\noindent Test tolerance: """
      + f"{args.tolerance_percent:g}"
      + r"""\%.
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
                      default=Path("build/output/dsmc-sphere-kinetic-convergence"))
  parser.add_argument("--steps", default="5,20,80")
  parser.add_argument("--loops", type=int, default=8)
  parser.add_argument("--resolution", type=int, default=10)
  parser.add_argument("--fnum", type=float, default=5.0e12)
  parser.add_argument("--dsmc-dt", type=float, default=1.0e-7)
  parser.add_argument("--ablation-update-time", type=float, default=0.0125)
  parser.add_argument("--tolerance-percent", type=float, default=20.0)
  parser.add_argument("--require-improvement", action="store_true")
  parser.add_argument("--pdf", action="store_true")
  parser.add_argument("--number-density", type=float, default=7.244e23)
  parser.add_argument("--temperature", type=float, default=5000.0)
  parser.add_argument("--wall-temperature", type=float, default=5000.0)
  parser.add_argument("--o2-fraction", type=float, default=0.21)
  parser.add_argument("--o2-mass", type=float, default=5.31352e-26)
  parser.add_argument("--solid-density", type=float, default=1800.0)
  parser.add_argument("--solid-mass-per-reaction", type=float,
                      default=3.98894696e-26)
  parser.add_argument("--reaction-probability", type=float, default=1.0)
  args = parser.parse_args()

  if args.loops <= 0:
    raise SystemExit("--loops must be positive")
  if args.resolution <= 0:
    raise SystemExit("--resolution must be positive")
  if args.ablation_update_time <= 0.0:
    raise SystemExit("--ablation-update-time must be positive")
  if not args.dsmc.exists():
    raise SystemExit(f"missing DSMC executable: {args.dsmc}")

  template = args.template.read_text(encoding="utf-8")
  if args.out.exists():
    shutil.rmtree(args.out)
  results = [
      run_case(args.dsmc, template, args.out, steps, args)
      for steps in parse_int_list(args.steps, "steps")
  ]
  write_summary(args.out / "summary.csv", results)
  write_report(args.out / "report.tex", results, args)
  if args.pdf:
    maybe_build_pdf(args.out / "report.tex")

  print(f"Wrote {args.out / 'summary.csv'}")
  print(f"Wrote {args.out / 'report.tex'}")
  if args.pdf:
    print(f"Wrote {args.out / 'report.pdf'}")

  finest = results[-1]
  finest_error = max(finest.mass_error_percent, finest.radius_error_percent)
  if finest_error > args.tolerance_percent:
    print(
        f"FAILED: finest error {finest_error:.6g}% exceeds "
        f"{args.tolerance_percent:g}%"
    )
    return 1
  if args.require_improvement:
    coarsest = max(results[0].mass_error_percent, results[0].radius_error_percent)
    if finest_error >= coarsest:
      print(
          "FAILED: finest case did not improve over coarsest case: "
          f"{coarsest:.6g}% -> {finest_error:.6g}%"
      )
      return 1
  print(
      f"PASSED: finest error {finest_error:.6g}% is within "
      f"{args.tolerance_percent:g}%"
  )
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
