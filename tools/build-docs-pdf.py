#!/usr/bin/env python3
"""Build a single PDF manual from the Markdown docs.

This is a lightweight local fallback for machines that do not have MkDocs or
Pandoc installed. It intentionally supports the Markdown features used by this
manual rather than trying to be a complete Markdown implementation.
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
BUILD = ROOT / "build" / "docs"
OUT_PDF = DOCS / "isthmus-ablation-core-manual.pdf"
FLOWCHART_TEX = DOCS / "architecture_flowchart.tex"
FLOWCHART_PDF = DOCS / "architecture_flowchart.pdf"
FLOWCHART_PNG = DOCS / "architecture_flowchart_preview-1.png"

PAGES = [
    ("Home", DOCS / "index.md"),
    ("Getting Started", DOCS / "getting-started.md"),
    ("Concepts", DOCS / "concepts" / "architecture.md"),
    ("Verification", DOCS / "concepts" / "verification.md"),
    ("Expected Features", DOCS / "concepts" / "expected-features.md"),
    ("Command Reference", DOCS / "commands" / "index.md"),
    ("include", DOCS / "commands" / "include.md"),
    ("voxel", DOCS / "commands" / "voxel.md"),
    ("source", DOCS / "commands" / "source.md"),
    ("timestep", DOCS / "commands" / "timestep.md"),
    ("loop commands", DOCS / "commands" / "loops.md"),
    ("voxel ablate", DOCS / "commands" / "voxel-ablate.md"),
    ("fix voxel/ablate", DOCS / "commands" / "fix-voxel-ablate.md"),
    ("isthmus surface", DOCS / "commands" / "isthmus-surface.md"),
    ("surface", DOCS / "commands" / "surface.md"),
    ("stats and stats_style", DOCS / "commands" / "stats.md"),
    ("voxel dump", DOCS / "commands" / "voxel-dump.md"),
    ("verify", DOCS / "commands" / "verify.md"),
    ("run", DOCS / "commands" / "run.md"),
    ("Direct Slab Ablation", DOCS / "examples" / "slab-direct.md"),
    ("ISTHMUS Slab Ablation", DOCS / "examples" / "slab-isthmus.md"),
    ("Sphere ISTHMUS Ablation", DOCS / "examples" / "sphere-isthmus.md"),
    ("DSMC Sphere Kinetic Example", DOCS / "examples" / "dsmc-sphere-kinetic.md"),
    ("Directory Layout", DOCS / "development" / "directory-layout.md"),
    ("Build And Link", DOCS / "development" / "build-and-link.md"),
    ("Testing", DOCS / "development" / "testing.md"),
    ("Test Reports", DOCS / "development" / "test-reports.md"),
    ("Documentation", DOCS / "development" / "documentation.md"),
]


SPECIALS = {
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


def latex_escape(text: str) -> str:
    return "".join(SPECIALS.get(ch, ch) for ch in text)


def format_inline(text: str) -> str:
    pieces = re.split(r"(`[^`]*`)", text)
    rendered = []
    for piece in pieces:
        if piece.startswith("`") and piece.endswith("`"):
            rendered.append(r"\texttt{" + latex_escape(piece[1:-1]) + "}")
        else:
            rendered.append(latex_escape(piece))
    return "".join(rendered)


def heading_command(level: int) -> str:
    if level <= 1:
        return "section"
    if level == 2:
        return "subsection"
    if level == 3:
        return "subsubsection"
    return "paragraph"


def markdown_to_latex(path: Path) -> str:
    lines = path.read_text(encoding="utf-8").splitlines()
    out: list[str] = []
    in_code = False
    in_itemize = False

    def close_itemize() -> None:
        nonlocal in_itemize
        if in_itemize:
            out.append(r"\end{itemize}")
            in_itemize = False

    for raw in lines:
        line = raw.rstrip()
        if line.startswith("```"):
            if in_code:
                out.append(r"\end{Verbatim}")
                in_code = False
            else:
                close_itemize()
                out.append(r"\begin{Verbatim}[fontsize=\small]")
                in_code = True
            continue

        if in_code:
            out.append(line)
            continue

        if not line.strip():
            close_itemize()
            out.append("")
            continue

        if line.startswith("!!!"):
            close_itemize()
            out.append(r"\medskip")
            out.append(r"\noindent\textbf{" + format_inline(line.replace("!!!", "").strip()) + r"}")
            continue

        match = re.match(r"^!\[(.*?)\]\((.*?)\)$", line)
        if match:
            close_itemize()
            image_path = (path.parent / match.group(2)).resolve()
            caption = format_inline(match.group(1))
            out.append(r"\begin{center}")
            out.append(r"\includegraphics[width=0.95\textwidth]{\detokenize{" + str(image_path) + r"}}")
            if caption:
                out.append(r"\smallskip")
                out.append(r"\textit{" + caption + r"}")
            out.append(r"\end{center}")
            continue

        match = re.match(r"^(#{1,6})\s+(.*)$", line)
        if match:
            close_itemize()
            level = len(match.group(1))
            title = re.sub(r"\[(.*?)\]\(.*?\)", r"\1", match.group(2))
            out.append("\\" + heading_command(level) + "{" + format_inline(title) + "}")
            continue

        match = re.match(r"^\s*[-*]\s+(.*)$", line)
        if match:
            if not in_itemize:
                out.append(r"\begin{itemize}")
                in_itemize = True
            out.append(r"\item " + format_inline(match.group(1)))
            continue

        match = re.match(r"^\s*(\d+)\.\s+(.*)$", line)
        if match:
            if not in_itemize:
                out.append(r"\begin{itemize}")
                in_itemize = True
            out.append(r"\item " + format_inline(match.group(2)))
            continue

        close_itemize()
        line = re.sub(r"\[(.*?)\]\(.*?\)", r"\1", line)
        out.append(format_inline(line))

    close_itemize()
    if in_code:
        out.append(r"\end{Verbatim}")
    return "\n".join(out)


def build_tex() -> str:
    body: list[str] = []
    for title, path in PAGES:
        if not path.exists():
            continue
        body.append(r"\clearpage")
        body.append(markdown_to_latex(path))

    return r"""\documentclass[10pt]{article}
\usepackage[letterpaper,margin=0.85in]{geometry}
\usepackage[T1]{fontenc}
\usepackage{lmodern}
\usepackage{xcolor}
\usepackage{hyperref}
\usepackage{fancyvrb}
\usepackage{graphicx}
\usepackage{microtype}
\hypersetup{colorlinks=true,linkcolor=blue,urlcolor=blue}
\setlength{\parindent}{0pt}
\setlength{\parskip}{0.55em}
\title{isthmus-ablation-core Manual}
\author{Voxel ablation and coupling core}
\date{\today}
\begin{document}
\maketitle
\tableofcontents
""" + "\n".join(body) + "\n\\end{document}\n"


def build_flowchart_assets() -> int:
    if not FLOWCHART_TEX.exists():
        return 0

    flowchart_build = BUILD / "flowchart"
    flowchart_build.mkdir(parents=True, exist_ok=True)

    compile_cmd = [
        "pdflatex",
        "-interaction=nonstopmode",
        "-halt-on-error",
        f"-output-directory={flowchart_build}",
        str(FLOWCHART_TEX),
    ]
    completed = subprocess.run(compile_cmd, cwd=ROOT, text=True)
    if completed.returncode != 0:
        return completed.returncode

    built_pdf = flowchart_build / "architecture_flowchart.pdf"
    if not built_pdf.exists():
        print(f"Expected flowchart PDF was not produced: {built_pdf}", file=sys.stderr)
        return 1
    FLOWCHART_PDF.write_bytes(built_pdf.read_bytes())

    pdftoppm = shutil.which("pdftoppm")
    if pdftoppm:
        png_prefix = flowchart_build / "architecture_flowchart_preview"
        completed = subprocess.run(
            [pdftoppm, "-png", "-singlefile", "-r", "220", str(built_pdf), str(png_prefix)],
            cwd=ROOT,
            text=True,
        )
        if completed.returncode != 0:
            return completed.returncode
        built_png = flowchart_build / "architecture_flowchart_preview.png"
        if built_png.exists():
            FLOWCHART_PNG.write_bytes(built_png.read_bytes())
            return 0

    sips = shutil.which("sips")
    if sips:
        completed = subprocess.run(
            [sips, "-s", "format", "png", str(built_pdf), "--out", str(FLOWCHART_PNG)],
            cwd=ROOT,
            text=True,
        )
        return completed.returncode

    print("Need pdftoppm or sips to generate docs/architecture_flowchart_preview-1.png", file=sys.stderr)
    return 1


def main() -> int:
    BUILD.mkdir(parents=True, exist_ok=True)
    flowchart_status = build_flowchart_assets()
    if flowchart_status != 0:
        return flowchart_status

    tex_path = BUILD / "isthmus-ablation-core-manual.tex"
    tex_path.write_text(build_tex(), encoding="utf-8")

    cmd = [
        "pdflatex",
        "-interaction=nonstopmode",
        "-halt-on-error",
        f"-output-directory={BUILD}",
        str(tex_path),
    ]
    for _ in range(2):
        completed = subprocess.run(cmd, cwd=ROOT, text=True)
        if completed.returncode != 0:
            return completed.returncode

    pdf_path = BUILD / "isthmus-ablation-core-manual.pdf"
    if not pdf_path.exists():
        print(f"Expected PDF was not produced: {pdf_path}", file=sys.stderr)
        return 1
    OUT_PDF.write_bytes(pdf_path.read_bytes())
    print(f"Wrote {OUT_PDF}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
