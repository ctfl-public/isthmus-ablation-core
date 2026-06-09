#!/usr/bin/env python3
"""Build a compact LaTeX/PGFPlots verification report from verify CSV data."""

from __future__ import annotations

import argparse
import csv
import subprocess
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


def tex_escape(value: str) -> str:
  replacements = {
      "\\": r"\textbackslash{}",
      "&": r"\&",
      "%": r"\%",
      "$": r"\$",
      "#": r"\#",
      "_": r"\_",
      "{": r"\{",
      "}": r"\}",
      "~": r"\textasciitilde{}",
      "^": r"\textasciicircum{}",
  }
  return "".join(replacements.get(ch, ch) for ch in value)


@dataclass(frozen=True)
class Case:
  name: str
  csv_path: Path
  input_path: Path | None


@dataclass(frozen=True)
class ConvergenceData:
  rows: list[dict[str, str]]
  order_rows: list[dict[str, str]]
  series: dict[str, list[dict[str, str]]]


def read_rows(path: Path) -> dict[str, list[dict[str, str]]]:
  with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle))
  grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
  for row in rows:
    grouped[row["quantity"]].append(row)
  return grouped


def read_csv(path: Path) -> list[dict[str, str]]:
  if not path.exists():
    return []
  with path.open(newline="", encoding="utf-8") as handle:
    return list(csv.DictReader(handle))


def read_convergence(case: Case) -> ConvergenceData | None:
  root = case.csv_path.parent
  rows = read_csv(root / "convergence.csv")
  order_rows = read_csv(root / "convergence-order.csv")
  if not rows and not order_rows:
    return None
  series: dict[str, list[dict[str, str]]] = {}
  for row in rows:
    csv_name = row.get("csv", "")
    if csv_name:
      series[row["label"]] = read_csv(root / csv_name)
  return ConvergenceData(rows=rows, order_rows=order_rows, series=series)


def coordinates(rows: list[dict[str, str]], field: str) -> str:
  return " ".join(f"({row['time']},{row[field]})" for row in rows)


def input_listing(path: Path | None) -> str:
  if path is None:
    return "No input listing was provided."
  if not path.exists():
    return f"Input file was not found: {path}"
  return path.read_text(encoding="utf-8")


def included_input_paths(path: Path | None) -> list[Path]:
  if path is None or not path.exists():
    return []

  seen: set[Path] = set()
  ordered: list[Path] = []

  def visit(current: Path) -> None:
    try:
      lines = current.read_text(encoding="utf-8").splitlines()
    except OSError:
      return
    for line in lines:
      stripped = line.strip()
      if not stripped or stripped.startswith("#"):
        continue
      parts = stripped.split()
      if len(parts) < 2 or parts[0] != "include":
        continue
      included = (current.parent / parts[1]).resolve()
      if included in seen:
        continue
      seen.add(included)
      ordered.append(included)
      visit(included)

  visit(path.resolve())
  return ordered


def input_sections(case: Case) -> str:
  sections: list[str] = []
  if case.input_path is None:
    sections.append(
        r"""
\subsection{Test Input}
No input listing was provided.
"""
    )
    return "\n".join(sections)

  sections.append(
      rf"""
\subsection{{Test Input}}
\textbf{{Path:}} \texttt{{{tex_escape(str(case.input_path))}}}
\begin{{Verbatim}}[fontsize=\scriptsize]
{input_listing(case.input_path)}
\end{{Verbatim}}
"""
  )

  for path in included_input_paths(case.input_path):
    sections.append(
        rf"""
\clearpage
\subsection{{Included Input}}
\textbf{{Path:}} \texttt{{{tex_escape(str(path))}}}
\begin{{Verbatim}}[fontsize=\scriptsize]
{input_listing(path)}
\end{{Verbatim}}
"""
    )

  return "\n".join(sections)


def case_summary_rows(name: str, grouped: dict[str, list[dict[str, str]]]) -> list[str]:
  rows: list[str] = []
  for quantity, quantity_rows in grouped.items():
    tolerance = quantity_rows[0]["tolerance"]
    tolerance_mode = quantity_rows[0].get("tolerance-mode", "absolute")
    norm = quantity_rows[0]["norm"]
    if norm == "final":
      summary_rows = [quantity_rows[-1]]
    else:
      summary_rows = quantity_rows
    passed = "yes" if all(row["pass"] == "yes" for row in summary_rows) else "no"
    max_error = max(float(row["error"]) for row in summary_rows)
    rows.append(
        f"{tex_escape(name)} & {tex_escape(quantity)} & {tex_escape(norm)} & "
        f"{max_error:.6e} & {float(tolerance):.6e} {tex_escape(tolerance_mode)} & "
        f"{tex_escape(passed)} \\\\"
    )
  return rows


def convergence_order_summary_rows(name: str, data: ConvergenceData | None) -> list[str]:
  if data is None:
    return []
  rows: list[str] = []
  for row in data.order_rows:
    rows.append(
        f"{tex_escape(name)} & {tex_escape(row['quantity'])} & convergence & "
        f"{float(row['order']):.6e} & "
        f"[{float(row['min-order']):.3g}, {float(row['max-order']):.3g}] & "
        f"{tex_escape(row['pass'])} \\\\"
    )
  return rows


def build_case_section(case: Case, grouped: dict[str, list[dict[str, str]]]) -> str:
  sections: list[str] = []

  for quantity, rows in grouped.items():
    sections.append(
        rf"""
\subsection*{{{tex_escape(quantity)}}}
\begin{{tikzpicture}}
\begin{{axis}}[
  width=\textwidth,
  height=0.34\textwidth,
  xlabel={{time}},
  ylabel={{{tex_escape(quantity)}}},
  legend pos=outer north east,
  grid=both,
]
\addplot+[mark=*] coordinates {{{coordinates(rows, "actual")}}};
\addlegendentry{{actual}}
\addplot+[mark=none,dashed] coordinates {{{coordinates(rows, "exact")}}};
\addlegendentry{{exact}}
\end{{axis}}
\end{{tikzpicture}}

\begin{{tikzpicture}}
\begin{{axis}}[
  width=\textwidth,
  height=0.30\textwidth,
  xlabel={{time}},
  ylabel={{absolute error}},
  legend pos=outer north east,
  grid=both,
]
\addplot+[mark=*] coordinates {{{coordinates(rows, "error")}}};
\addlegendentry{{error}}
\addplot+[mark=none,dashed] coordinates {{{coordinates(rows, "tolerance")}}};
\addlegendentry{{tolerance}}
\end{{axis}}
\end{{tikzpicture}}
"""
    )

  body = "\n".join(sections)
  convergence = build_convergence_section(read_convergence(case))
  return rf"""
\clearpage
\section{{{tex_escape(case.name)}}}

\textbf{{Verification data:}} \texttt{{{tex_escape(str(case.csv_path))}}}

{input_sections(case)}

\clearpage
\subsection{{Plots}}
{body}
{convergence}
"""


def build_convergence_section(data: ConvergenceData | None) -> str:
  if data is None:
    return ""

  order_rows = "\n".join(
      f"{tex_escape(row['quantity'])} & {float(row['first-refinement']):.6g} & "
      f"{float(row['last-refinement']):.6g} & {float(row['first-error']):.6e} & "
      f"{float(row['last-error']):.6e} & {float(row['order']):.6e} & "
      f"[{float(row['min-order']):.3g}, {float(row['max-order']):.3g}] & "
      f"{tex_escape(row['monotonic'])} & {tex_escape(row['pass'])} \\\\"
      for row in data.order_rows
  )

  plots: list[str] = []
  quantities = sorted({row["quantity"] for row in data.rows})
  for quantity in quantities:
    rows = [row for row in data.rows if row["quantity"] == quantity]
    error_plot_lines: list[str] = []
    for row in rows:
      error_plot_lines.append(
          rf"\addplot+[mark=*] coordinates {{({row['refinement']},{row['error']})}};"
      )
      error_plot_lines.append(rf"\addlegendentry{{{tex_escape(row['label'])}}}")

    curve_lines: list[str] = []
    for label, series_rows in data.series.items():
      matching = [row for row in series_rows if row["quantity"] == quantity]
      if not matching:
        continue
      curve_lines.append(
          rf"\addplot+[mark=*] coordinates {{{coordinates(matching, 'actual')}}};"
      )
      curve_lines.append(rf"\addlegendentry{{{tex_escape(label)} actual}}")
      curve_lines.append(
          rf"\addplot+[mark=none,dashed] coordinates {{{coordinates(matching, 'exact')}}};"
      )
      curve_lines.append(rf"\addlegendentry{{{tex_escape(label)} exact}}")

    plots.append(
        rf"""
\subsection*{{Convergence: {tex_escape(quantity)}}}
\begin{{tikzpicture}}
\begin{{axis}}[
  width=\textwidth,
  height=0.34\textwidth,
  xlabel={{refinement}},
  ylabel={{error}},
  legend pos=outer north east,
  grid=both,
]
{chr(10).join(error_plot_lines)}
\end{{axis}}
\end{{tikzpicture}}

\begin{{tikzpicture}}
\begin{{axis}}[
  width=\textwidth,
  height=0.34\textwidth,
  xlabel={{time}},
  ylabel={{{tex_escape(quantity)}}},
  legend pos=outer north east,
  grid=both,
]
{chr(10).join(curve_lines)}
\end{{axis}}
\end{{tikzpicture}}
"""
    )

  return rf"""
\subsection*{{Convergence Order}}
\resizebox{{\textwidth}}{{!}}{{%
\begin{{tabular}}{{lllllllll}}
\toprule
Quantity & First & Last & First Error & Last Error & Order & Allowed & Monotone & Pass \\
\midrule
{order_rows}
\bottomrule
\end{{tabular}}
}}
{''.join(plots)}
"""


def build_tex(cases: list[Case]) -> str:
  grouped_cases = [(case, read_rows(case.csv_path)) for case in cases]
  summary_rows = [
      row
      for case, grouped in grouped_cases
      for row in case_summary_rows(case.name, grouped)
  ]
  summary_rows.extend(
      row
      for case in cases
      for row in convergence_order_summary_rows(case.name, read_convergence(case))
  )
  summary = "\n".join(summary_rows)
  body = "\n".join(build_case_section(case, grouped) for case, grouped in grouped_cases)
  return rf"""\documentclass[10pt]{{article}}
\usepackage[letterpaper,margin=0.85in]{{geometry}}
\usepackage{{booktabs}}
\usepackage{{fancyvrb}}
\usepackage{{pgfplots}}
\pgfplotsset{{compat=1.18}}
\setlength{{\parindent}}{{0pt}}
\setlength{{\parskip}}{{0.5em}}
\title{{Verification Report}}
\author{{isthmus-ablation-core}}
\date{{}}
\begin{{document}}
\maketitle
\tableofcontents
\clearpage

\section*{{Summary}}
\addcontentsline{{toc}}{{section}}{{Summary}}
\resizebox{{\textwidth}}{{!}}{{%
\begin{{tabular}}{{llllll}}
\toprule
Case & Quantity & Norm & Error & Tolerance & Pass \\
\midrule
{summary}
\bottomrule
\end{{tabular}}
}}

{body}

\end{{document}}
"""


def discover_cases(root: Path) -> list[Case]:
  cases: list[Case] = []
  for csv_path in sorted(root.glob("*/report.csv")):
    name = csv_path.parent.name
    cases.append(Case(name=name, csv_path=csv_path, input_path=None))
  return cases


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("csv", nargs="*", type=Path)
  parser.add_argument(
      "--case",
      action="append",
      nargs=3,
      metavar=("NAME", "CSV", "INPUT"),
      default=[],
      help="Add a named report case with its verification CSV and input file.",
  )
  parser.add_argument(
      "--discover",
      type=Path,
      help="Discover report.csv files one directory below this root.",
  )
  parser.add_argument(
      "--only",
      action="append",
      default=[],
      help="Include only the named case. May be repeated.",
  )
  parser.add_argument("--out", type=Path, required=True)
  parser.add_argument("--pdf", action="store_true")
  args = parser.parse_args()

  cases = [Case(name=name, csv_path=Path(csv_path), input_path=Path(input_path))
           for name, csv_path, input_path in args.case]
  cases.extend(Case(name=path.stem, csv_path=path, input_path=None) for path in args.csv)
  if args.discover is not None:
    cases.extend(discover_cases(args.discover))

  if args.only:
    wanted = set(args.only)
    cases = [case for case in cases if case.name in wanted]

  if not cases:
    parser.error("no report cases were provided")

  missing = [str(case.csv_path) for case in cases if not case.csv_path.exists()]
  if missing:
    parser.error("missing report CSV file(s): " + ", ".join(missing))

  out_path = args.out.resolve()
  out_path.parent.mkdir(parents=True, exist_ok=True)
  out_path.write_text(build_tex(cases), encoding="utf-8")
  print(f"Wrote {out_path}")

  if args.pdf:
    command = [
        "pdflatex",
        "-interaction=nonstopmode",
        "-halt-on-error",
        f"-output-directory={out_path.parent}",
        out_path.name,
    ]
    for _ in range(2):
      completed = subprocess.run(command, cwd=out_path.parent)
      if completed.returncode != 0:
        return completed.returncode
    print(f"Wrote {out_path.with_suffix('.pdf')}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
