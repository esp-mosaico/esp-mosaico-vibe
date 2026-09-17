#!/usr/bin/env python3
"""Make the horizontal boundary of an equirectangular panorama periodic."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image


def edge_error(image: Image.Image) -> float:
    pixels = image.load()
    width, height = image.size
    total = 0
    for y in range(height):
        left = pixels[0, y]
        right = pixels[width - 1, y]
        total += sum(abs(int(left[channel]) - int(right[channel])) for channel in range(3))
    return total / (height * 3)


def make_periodic(image: Image.Image, blend_width: int) -> Image.Image:
    result = image.convert("RGB")
    width, height = result.size
    if blend_width < 2 or blend_width * 2 >= width:
        raise ValueError("blend width must be at least 2 and less than half the image width")
    source = result.copy()
    src = source.load()
    dst = result.load()
    for offset in range(blend_width):
        # At the seam both sides become the same average. The influence falls
        # to zero with a cosine ramp before reaching unchanged source pixels.
        mix = 0.25 * (1.0 + math.cos(math.pi * offset / (blend_width - 1)))
        left_x = offset
        right_x = width - 1 - offset
        for y in range(height):
            left = src[left_x, y]
            right = src[right_x, y]
            dst[left_x, y] = tuple(round((1.0 - mix) * left[c] + mix * right[c]) for c in range(3))
            dst[right_x, y] = tuple(round((1.0 - mix) * right[c] + mix * left[c]) for c in range(3))
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", required=True, help="WIDTHxHEIGHT")
    parser.add_argument("--blend-width", type=int, default=128)
    parser.add_argument("--jpeg", type=Path)
    args = parser.parse_args()
    width, height = (int(value) for value in args.size.lower().split("x", 1))
    image = Image.open(args.input).convert("RGB").resize(
        (width, height), Image.Resampling.LANCZOS
    )
    before = edge_error(image)
    image = make_periodic(image, args.blend_width)
    after = edge_error(image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output, optimize=True)
    if args.jpeg:
        image.save(args.jpeg, quality=96, subsampling=2, optimize=True)
    print(f"{args.output}: edge_error {before:.2f} -> {after:.2f}")


if __name__ == "__main__":
    main()
