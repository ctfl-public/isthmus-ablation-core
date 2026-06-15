#!/usr/bin/env python3
"""Write a deterministic rough carbon-sample TIFF stack for examples/tests."""

import argparse
import random
import struct
from pathlib import Path
from typing import List


def ifd_entry(tag: int, field_type: int, count: int, value: int) -> bytes:
    return struct.pack("<HHII", tag, field_type, count, value)


def make_sample(width: int, height: int, depth: int, seed: int) -> List[bytes]:
    rng = random.Random(seed)
    surface = [[depth for _ in range(width)] for _ in range(height)]
    pit_depth_max = max(1, min(5, depth // 2))
    pinhole_min = max(1, depth // 3)
    pinhole_max = max(pinhole_min, min(depth - 1, (2 * depth) // 3))

    for _ in range(34):
        cx = rng.randrange(width)
        cy = rng.randrange(height)
        radius = rng.uniform(1.0, 3.6)
        pit_depth = rng.randint(1, pit_depth_max)
        radius2 = radius * radius
        for y in range(height):
            for x in range(width):
                dx = x - cx
                dy = y - cy
                r2 = dx * dx + dy * dy
                if r2 <= radius2:
                    taper = 0.35 + 0.65 * (1.0 - r2 / radius2)
                    surface[y][x] = min(surface[y][x], depth - max(1, round(pit_depth * taper)))

    for _ in range(18):
        x = rng.randrange(width)
        y = rng.randrange(height)
        surface[y][x] = min(surface[y][x], depth - rng.randint(pinhole_min, pinhole_max))

    pages = []  # type: List[bytes]
    for x_page in range(depth):
        pixels = bytearray()
        for y in range(height):
            for z_col in range(width):
                pixels.append(1 if x_page < surface[y][z_col] else 0)
        pages.append(bytes(pixels))
    return pages


def write_tiff(path: Path, pages: List[bytes], width: int, height: int) -> int:
    depth = len(pages)
    page_bytes = width * height
    ifd_entries = 8
    ifd_size = 2 + 12 * ifd_entries + 4
    first_ifd_offset = 8
    pixel_base = first_ifd_offset + depth * ifd_size
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
            out.write(ifd_entry(256, 4, 1, width))
            out.write(ifd_entry(257, 4, 1, height))
            out.write(ifd_entry(258, 3, 1, 8))
            out.write(ifd_entry(259, 3, 1, 1))
            out.write(ifd_entry(273, 4, 1, strip_offset))
            out.write(ifd_entry(277, 3, 1, 1))
            out.write(ifd_entry(278, 4, 1, height))
            out.write(ifd_entry(279, 4, 1, page_bytes))
            out.write(struct.pack("<I", next_ifd))

        for page in pages:
            out.write(page)

    return active


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=24)
    parser.add_argument("--height", type=int, default=24)
    parser.add_argument("--depth", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20260611)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0 or args.depth <= 0:
        raise SystemExit("width, height, and depth must be positive")

    pages = make_sample(args.width, args.height, args.depth, args.seed)
    active = write_tiff(args.out, pages, args.width, args.height)
    total = args.width * args.height * args.depth
    print(
        f"wrote {args.out} with {active}/{total} active voxels "
        f"({active / total:.6g})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
