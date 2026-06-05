#!/usr/bin/env python3
"""Convert DSMC visual example grid dumps to ParaView-ready VTU files."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


@dataclass
class SpartaBlock:
    timestep: int
    columns: list[str]
    rows: list[list[float]]


def read_sparta_dump(path: Path, item: str) -> SpartaBlock:
    lines = path.read_text().splitlines()
    timestep = None
    columns: list[str] | None = None
    rows: list[list[float]] = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line == "ITEM: TIMESTEP":
            timestep = int(lines[i + 1].strip())
            i += 2
            continue
        if line.startswith(f"ITEM: {item} "):
            columns = line.split()[2:]
            i += 1
            while i < len(lines) and not lines[i].startswith("ITEM:"):
                if lines[i].strip():
                    rows.append([float(value) for value in lines[i].split()])
                i += 1
            continue
        i += 1
    if timestep is None or columns is None:
        raise RuntimeError(f"{path} is not a recognized SPARTA {item.lower()} dump")
    return SpartaBlock(timestep=timestep, columns=columns, rows=rows)


def unique_sorted(values: list[float]) -> list[float]:
    rounded = sorted({round(value, 15) for value in values})
    return [float(value) for value in rounded]


def spacing(coords: list[float], volumes: list[float]) -> tuple[float, float, float]:
    axes = coords[0], coords[1], coords[2]
    deltas = []
    for axis in axes:
        diffs = [b - a for a, b in zip(axis, axis[1:]) if b > a]
        deltas.append(min(diffs) if diffs else volumes[0] ** (1.0 / 3.0))
    return deltas[0], deltas[1], deltas[2]


def data_array(name: str, values: list[float], dtype: str = "Float64") -> str:
    body = "\n".join(f"          {value:.17g}" for value in values)
    return f'        <DataArray type="{dtype}" Name="{name}" format="ascii">\n{body}\n        </DataArray>\n'


def write_grid_vtu(block: SpartaBlock, path: Path) -> None:
    index = {name: i for i, name in enumerate(block.columns)}
    for required in ("id", "xc", "yc", "zc", "vol"):
        if required not in index:
            raise RuntimeError(f"grid dump is missing required column {required}")

    rows = sorted(block.rows, key=lambda row: int(row[index["id"]]))
    xs = unique_sorted([row[index["xc"]] for row in rows])
    ys = unique_sorted([row[index["yc"]] for row in rows])
    zs = unique_sorted([row[index["zc"]] for row in rows])
    volumes = [row[index["vol"]] for row in rows]
    dx, dy, dz = spacing([xs, ys, zs], volumes)

    points: list[tuple[float, float, float]] = []
    connectivity: list[list[int]] = []
    for row in rows:
        xc, yc, zc = row[index["xc"]], row[index["yc"]], row[index["zc"]]
        xlo, xhi = xc - 0.5 * dx, xc + 0.5 * dx
        ylo, yhi = yc - 0.5 * dy, yc + 0.5 * dy
        zlo, zhi = zc - 0.5 * dz, zc + 0.5 * dz
        base = len(points)
        points.extend(
            [
                (xlo, ylo, zlo),
                (xhi, ylo, zlo),
                (xhi, yhi, zlo),
                (xlo, yhi, zlo),
                (xlo, ylo, zhi),
                (xhi, ylo, zhi),
                (xhi, yhi, zhi),
                (xlo, yhi, zhi),
            ]
        )
        connectivity.append([base + offset for offset in range(8)])

    field_names = {
        "f_gasave[1]": "temperature",
        "f_gasave[2]": "pressure",
        "f_gasave[3]": "mass-fraction-O2",
        "f_gasave[4]": "mass-fraction-N2",
        "f_gasave[5]": "mass-fraction-CO",
    }
    arrays = []
    arrays.append(data_array("id", [int(row[index["id"]]) for row in rows], "Int64"))
    for column, name in field_names.items():
        if column in index:
            arrays.append(data_array(name, [row[index[column]] for row in rows]))

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as out:
        out.write('<?xml version="1.0"?>\n')
        out.write('<VTKFile type="UnstructuredGrid" version="0.1" byte_order="LittleEndian">\n')
        out.write("  <UnstructuredGrid>\n")
        out.write(f'    <Piece NumberOfPoints="{len(points)}" NumberOfCells="{len(rows)}">\n')
        out.write("      <Points>\n")
        out.write('        <DataArray type="Float64" NumberOfComponents="3" format="ascii">\n')
        for x, y, z in points:
            out.write(f"          {x:.17g} {y:.17g} {z:.17g}\n")
        out.write("        </DataArray>\n")
        out.write("      </Points>\n")
        out.write("      <Cells>\n")
        out.write('        <DataArray type="Int64" Name="connectivity" format="ascii">\n')
        for cell in connectivity:
            out.write("          " + " ".join(str(value) for value in cell) + "\n")
        out.write("        </DataArray>\n")
        out.write('        <DataArray type="Int64" Name="offsets" format="ascii">\n')
        for i in range(len(rows)):
            out.write(f"          {(i + 1) * 8}\n")
        out.write("        </DataArray>\n")
        out.write('        <DataArray type="UInt8" Name="types" format="ascii">\n')
        for _ in rows:
            out.write("          12\n")
        out.write("        </DataArray>\n")
        out.write("      </Cells>\n")
        out.write('      <CellData Scalars="temperature">\n')
        for array in arrays:
            out.write(array)
        out.write("      </CellData>\n")
        out.write("    </Piece>\n")
        out.write("  </UnstructuredGrid>\n")
        out.write("</VTKFile>\n")


def convert(case_dir: Path) -> None:
    output = case_dir / "output"
    grid_dumps = sorted(output.glob("grid.*.dump"), key=lambda p: int(p.stem.split(".")[-1]))
    for dump in grid_dumps:
        block = read_sparta_dump(dump, "CELLS")
        write_grid_vtu(block, output / f"fluid-{block.timestep}.vtu")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "case_dir",
        nargs="?",
        default="examples/dsmc-sphere-visual",
        help="DSMC visual case directory",
    )
    args = parser.parse_args()
    convert(Path(args.case_dir))


if __name__ == "__main__":
    main()
