#!/usr/bin/env python3
"""Run selected verification tests and build a combined report."""

from __future__ import annotations

import argparse
import csv
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ReportCase:
  index: int
  name: str
  input_path: Path
  requires: tuple[str, ...]
  expected_fail: bool


def read_cases(path: Path) -> list[ReportCase]:
  with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle))
  return [
      ReportCase(
          index=int(row["index"]),
          name=row["name"],
          input_path=Path(row["input"]),
          requires=tuple(
              item.strip() for item in row.get("requires", "").split(";") if item.strip()
          ),
          expected_fail=row.get("expected-fail", "no").strip().lower() in ("yes", "true", "1"),
      )
      for row in rows
  ]


def select_case(token: str, cases: list[ReportCase]) -> list[ReportCase]:
  if token == "all":
    return cases
  if "-" in token and all(part.isdigit() for part in token.split("-", 1)):
    first, last = (int(part) for part in token.split("-", 1))
    return [case for case in cases if first <= case.index <= last]
  if token.isdigit():
    index = int(token)
    return [case for case in cases if case.index == index]

  exact = [case for case in cases if case.name == token]
  if exact:
    return exact
  partial = [case for case in cases if token in case.name]
  if len(partial) == 1:
    return partial
  if not partial:
    raise ValueError(f"unknown report test '{token}'")
  names = ", ".join(case.name for case in partial)
  raise ValueError(f"ambiguous report test '{token}' matches: {names}")


def select_cases(selection: str, cases: list[ReportCase]) -> list[ReportCase]:
  selected: list[ReportCase] = []
  seen: set[str] = set()
  for token in (part.strip() for part in selection.split(",")):
    if not token:
      continue
    for case in select_case(token, cases):
      if case.name not in seen:
        selected.append(case)
        seen.add(case.name)
  return selected


def detect_available_features(build_dir: Path) -> set[str]:
  features: set[str] = set()
  cache = build_dir / "CMakeCache.txt"
  if cache.exists():
    text = cache.read_text(encoding="utf-8", errors="replace")
    if "isthmus_cpp_DIR:PATH=" in text and "isthmus_cpp_DIR:PATH=isthmus_cpp_DIR-NOTFOUND" not in text:
      features.add("isthmus")
  return features


def filter_available(cases: list[ReportCase], features: set[str]) -> list[ReportCase]:
  available: list[ReportCase] = []
  for case in cases:
    missing = [feature for feature in case.requires if feature not in features]
    if missing:
      print(
          f"Skipping [{case.index}] {case.name}: requires {', '.join(missing)}",
          flush=True,
      )
      continue
    available.append(case)
  return available


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "tests",
      nargs="?",
      default="all",
      help="Tests to report: all, ranges like 1-4, indices, or comma-separated names.",
  )
  parser.add_argument("--build", type=Path, default=Path("build"))
  parser.add_argument("--manifest", type=Path, default=Path("tests/report-cases.csv"))
  parser.add_argument("--out", type=Path, default=Path("build/output/test-report.tex"))
  parser.add_argument(
      "--available",
      default="auto",
      help="Comma-separated optional features available for report cases, or auto.",
  )
  parser.add_argument("--no-pdf", action="store_true")
  args = parser.parse_args()

  root = Path.cwd()
  exe = args.build / "ia-core"
  if not exe.exists():
    parser.error(f"missing executable: {exe}")

  cases = select_cases(args.tests, read_cases(args.manifest))
  if args.available == "auto":
    features = detect_available_features(args.build)
  else:
    features = {item.strip() for item in args.available.split(",") if item.strip()}
  cases = filter_available(cases, features)
  if not cases:
    parser.error("no report tests selected")

  report_args = [
      "python3",
      "tools/build-test-report.py",
  ]

  for case in cases:
    csv_path = args.build / "output" / "test-report-data" / case.name / "report.csv"
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(exe),
        "-in",
        str(case.input_path),
        "-report-csv",
        str(csv_path),
    ]
    print(f"[{case.index}] {case.name}", flush=True)
    completed = subprocess.run(command, cwd=root)
    if completed.returncode != 0 and not case.expected_fail:
      return completed.returncode
    if completed.returncode == 0 and case.expected_fail:
      print(f"Expected [{case.index}] {case.name} to fail, but it passed", flush=True)
      return 1
    report_args.extend(["--case", case.name, str(csv_path), str(case.input_path)])

  report_args.extend(["--out", str(args.out)])
  if not args.no_pdf:
    report_args.append("--pdf")
  return subprocess.run(report_args, cwd=root).returncode


if __name__ == "__main__":
  raise SystemExit(main())
