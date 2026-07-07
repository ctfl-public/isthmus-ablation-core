#!/usr/bin/env python3
"""Write a tiny label-preserving multiphase slab TIFF fixture."""

import argparse
import struct
from pathlib import Path
from typing import List


def ifd_entry(tag: int, field_type: int, count: int, value: int) -> bytes:
    return struct.pack("<HHII", tag, field_type, count, value)


def make_pages(width: int, height: int, depth: int) -> List[bytes]:
    pages = []
    split = height // 2
    for _z in range(depth):
        pixels = bytearray()
        for y in range(height):
            value = 1 if y < split else 2
            for _x in range(width):
                pixels.append(value)
        pages.append(bytes(pixels))
    return pages


def write_tiff(path: Path, pages: List[bytes], width: int, height: int) -> None:
    depth = len(pages)
    page_bytes = width * height
    ifd_entries = 8
    ifd_size = 2 + 12 * ifd_entries + 4
    first_ifd_offset = 8
    pixel_base = first_ifd_offset + depth * ifd_size

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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=4)
    parser.add_argument("--height", type=int, default=4)
    parser.add_argument("--depth", type=int, default=2)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.width <= 0 or args.height <= 0 or args.depth <= 0 or args.height % 2 != 0:
        raise SystemExit("width/depth must be positive and height must be positive/even")
    write_tiff(args.out, make_pages(args.width, args.height, args.depth), args.width, args.height)
    print(f"wrote {args.out} with labels 1 and 2")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
