#!/usr/bin/env python3
"""Write a small ISTHMUS-compatible multipage TIFF sphere fixture."""

import argparse
import math
import struct
from pathlib import Path


def ifd_entry(tag: int, field_type: int, count: int, value: int) -> bytes:
    return struct.pack("<HHII", tag, field_type, count, value)


def make_page(size: int, radius: float, z: int) -> bytes:
    center = 0.5 * (size - 1)
    radius2 = radius * radius
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            dx = x - center
            dy = y - center
            dz = z - center
            pixels.append(1 if dx * dx + dy * dy + dz * dz <= radius2 else 0)
    return bytes(pixels)


def write_tiff(path: Path, size: int, radius: float) -> int:
    if size <= 0:
        raise ValueError("size must be positive")
    if radius <= 0.0:
        raise ValueError("radius must be positive")

    depth = size
    page_bytes = size * size
    ifd_entries = 8
    ifd_size = 2 + 12 * ifd_entries + 4
    first_ifd_offset = 8
    pixel_base = first_ifd_offset + depth * ifd_size

    pages = [make_page(size, radius, z) for z in range(depth)]
    active = sum(page.count(1) for page in pages)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as out:
        out.write(b"II")
        out.write(struct.pack("<H", 42))
        out.write(struct.pack("<I", first_ifd_offset))

        for z in range(depth):
            ifd_offset = first_ifd_offset + z * ifd_size
            next_ifd = ifd_offset + ifd_size if z + 1 < depth else 0
            strip_offset = pixel_base + z * page_bytes
            out.write(struct.pack("<H", ifd_entries))
            out.write(ifd_entry(256, 4, 1, size))
            out.write(ifd_entry(257, 4, 1, size))
            out.write(ifd_entry(258, 3, 1, 8))
            out.write(ifd_entry(259, 3, 1, 1))
            out.write(ifd_entry(273, 4, 1, strip_offset))
            out.write(ifd_entry(277, 3, 1, 1))
            out.write(ifd_entry(278, 4, 1, size))
            out.write(ifd_entry(279, 4, 1, page_bytes))
            out.write(struct.pack("<I", next_ifd))

        for page in pages:
            out.write(page)

    return active


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=24, help="voxels per TIFF dimension")
    parser.add_argument("--radius", type=float, default=11.5, help="sphere radius in voxels")
    parser.add_argument("--out", type=Path, required=True, help="output TIFF path")
    args = parser.parse_args()

    active = write_tiff(args.out, args.size, args.radius)
    volume_fraction = active / math.pow(args.size, 3)
    print(
        f"wrote {args.out} with {active} active voxels "
        f"({volume_fraction:.6g} of {args.size}^3)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
