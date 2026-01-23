#!/usr/bin/env python3
"""
gen_sample_images.py

Generate a couple of sample images suitable for the WeAct 2.9" e-paper (128x296).
Creates:
 - images/sample_bw.png  (1-bit black/white test image)
 - images/sample_3c.png  (RGB image with black + red regions)

Usage:
  python3 tools/gen_sample_images.py            # default sizes 128x296, outputs to examples/.../images
  python3 tools/gen_sample_images.py --out outdir --width 128 --height 296

Dependencies:
  pip install pillow

Notes:
 - The BW sample is saved as a 1-bit PNG (mode '1').
 - The 3C sample is an RGB PNG containing red regions to test the red channel.
"""

from __future__ import annotations

import argparse
import os

from PIL import Image, ImageDraw, ImageFont, ImageOps

DEFAULT_WIDTH = 128
DEFAULT_HEIGHT = 296


def load_font_prefer(size: int):
    # Try common TTFs, fallback to default bitmap font
    names = ["DejaVuSans-Bold.ttf", "Arial.ttf", "FreeSansBold.ttf"]
    for n in names:
        try:
            return ImageFont.truetype(n, size)
        except Exception:
            continue
    return ImageFont.load_default()


def center_text(
    draw: ImageDraw.Draw,
    text: str,
    font: ImageFont.ImageFont,
    width: int,
    y_offset: int,
    fill,
):
    # Compute text size and draw centered at given y_offset
    try:
        bbox = draw.textbbox((0, 0), text, font=font)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
    except AttributeError:
        # fallback
        w, h = draw.textsize(text, font=font)
    x = (width - w) // 2
    draw.text((x, y_offset), text, font=font, fill=fill)


def generate_bw_image(path: str, width: int, height: int):
    # Grayscale image, then convert to 1-bit
    img = Image.new("L", (width, height), 255)  # white background
    draw = ImageDraw.Draw(img)
    # Outer border
    draw.rectangle((0, 0, width - 1, height - 1), outline=0)
    # Diagonal hatch lines
    for y in range(0, height, 14):
        draw.line((0, y, width - 1, y), fill=200)
    # Central bold text
    font = load_font_prefer(max(12, width // 6))
    center_text(draw, "HELLO BW", font, width, y_offset=(height // 2) - 20, fill=0)
    # Some pattern near bottom
    for i in range(8):
        x0 = 6 + i * (width - 12) // 8
        draw.rectangle((x0, height - 30, x0 + 6, height - 10), outline=0, fill=0)
    # Convert to 1-bit (threshold)
    img_1 = img.convert("1")  # Pillow will apply a threshold
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img_1.save(path, optimize=True)
    print(f"Saved BW sample at {path} ({width}x{height}, mode=1)")


def generate_3c_image(path: str, width: int, height: int):
    img = Image.new("RGB", (width, height), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    # Top band in black
    band_h = max(18, height // 16)
    draw.rectangle((0, 0, width, band_h), fill=(0, 0, 0))
    font_big = load_font_prefer(max(18, width // 6))
    # 'Hello' in black
    center_text(draw, "Hello", font_big, width, y_offset=band_h + 8, fill=(0, 0, 0))
    # 'RED' in red below
    font_red = load_font_prefer(max(22, width // 5))
    center_text(draw, "RED", font_red, width, y_offset=band_h + 40, fill=(220, 10, 10))
    # Add some red accents (circles)
    r = min(14, width // 12)
    for i in range(3):
        cx = int(width * (0.2 + 0.3 * i))
        cy = int(height * 0.75)
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(220, 10, 10))
    # Add black QR-like box pattern for test
    block_w = max(14, width // 10)
    draw.rectangle(
        (width - block_w - 6, height - block_w - 6, width - 6, height - 6),
        outline=(0, 0, 0),
        width=2,
    )
    # Save RGB PNG
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
    print(f"Saved 3-color sample at {path} ({width}x{height}, mode=RGB)")


def main():
    parser = argparse.ArgumentParser(
        description='Generate sample images for the 2.9" e-paper.'
    )
    parser.add_argument(
        "--out", "-o", default="images", help="Output directory (default: images/)"
    )
    parser.add_argument(
        "--width", "-W", type=int, default=DEFAULT_WIDTH, help="Image width (px)"
    )
    parser.add_argument(
        "--height", "-H", type=int, default=DEFAULT_HEIGHT, help="Image height (px)"
    )
    parser.add_argument("--bw-name", default="sample_bw.png", help="BW sample filename")
    parser.add_argument(
        "--3c-name",
        dest="c3_name",
        default="sample_3c.png",
        help="3-color sample filename",
    )
    args = parser.parse_args()

    out_dir = args.out
    w = args.width
    h = args.height

    bw_path = os.path.join(out_dir, args.bw_name)
    c3_path = os.path.join(out_dir, args.c3_name)

    print(f"Generating sample images into {out_dir} ...")
    generate_bw_image(bw_path, w, h)
    generate_3c_image(c3_path, w, h)
    print("Done. You can upload them with:")
    print(
        f"  python3 tools/upload_image.py --file {bw_path} --url http://<esp-ip>/image --format bw --width {w} --height {h} --force-full"
    )
    print(
        f"  python3 tools/upload_image.py --file {c3_path} --url http://<esp-ip>/image --format 3c --width {w} --height {h} --force-full"
    )


if __name__ == "__main__":
    main()
