#!/usr/bin/env python3
"""
upload_image.py - convert an image to 1-bit or 3-color bitplanes and upload to ESP32 E-Paper API

Requirements:
  - Python 3
  - Pillow (PIL)
  - requests

Install deps:
  pip install pillow requests

Usage:
  # Convert and upload to device in AP mode (default IP)
  python3 tools/upload_image.py -f myimage.png -u http://192.168.4.1/image --format 3c

  # Specify size (defaults to 128x296)
  python3 tools/upload_image.py -f img.png -u http://esp-ip/image --width 128 --height 296

  # Force a full refresh on the device (useful if partial updates are unstable)
  python3 tools/upload_image.py -f img.png -u http://esp-ip/image --force-full

The 3c format expects the uploader to pack two bitplanes:
  [black_plane bytes] + [red_plane bytes]
Each plane is width*height bits, MSB-first, row-major, padded on the last byte as needed.

The script can save a small preview file (png) showing how the converter sees the image:
  --preview preview.png

Note: input images are center-cropped to match the requested target size (no stretching). The crop is centered to preserve composition; use `--preview` to verify how the image maps to the panel.
"""

import argparse
import base64
import json
import sys
from io import BytesIO

import requests

try:
    from PIL import Image, ImageOps
except Exception as e:
    print("Missing Pillow: pip install pillow")
    raise

# Helpers for bitplane packing -------------------------------------------------


def pack_bitplane_bool(width, height, pixel_fn):
    """
    Pack a plane as bytes (row-major, MSB-first per byte).
    pixel_fn(x, y) -> True if pixel is set (1), False otherwise
    """
    out = bytearray()
    for y in range(height):
        byte = 0
        bits = 0
        for x in range(width):
            bit = 1 if pixel_fn(x, y) else 0
            byte = (byte << 1) | bit
            bits += 1
            if bits == 8:
                out.append(byte & 0xFF)
                byte = 0
                bits = 0
        if bits > 0:
            # pad LSBs with zeros
            byte = byte << (8 - bits)
            out.append(byte & 0xFF)
    return bytes(out)


def image_to_1bit_planes(
    img,
    width,
    height,
    format_="3c",
    bw_threshold=128,
    red_threshold=160,
    red_delta=60,
    preview_path=None,
):
    """
    Convert a PIL image to bitplanes.

    - For 'bw': returns (bw_bytes,)
    - For '3c': returns (black_bytes, red_bytes)
    """
    # Use a centered crop to match the target aspect ratio, then resize.
    # This prevents stretching: the image is cropped (center) and fitted to the target.
    im_rgb = ImageOps.fit(
        img.convert("RGB"), (width, height), Image.LANCZOS, centering=(0.5, 0.5)
    )

    pix = im_rgb.load()

    # Optionally create a preview image showing mapped colors
    if preview_path:
        preview = Image.new("RGB", (width, height), "white")
        pr = preview.load()

    def is_red(x, y):
        r, g, b = pix[x, y]
        return (r >= red_threshold) and (r - max(g, b) >= red_delta)

    def is_black_bw(x, y):
        r, g, b = pix[x, y]
        lum = int(0.299 * r + 0.587 * g + 0.114 * b)
        return lum < bw_threshold

    if format_ == "bw":
        if preview_path:
            for y in range(height):
                for x in range(width):
                    pr[x, y] = (0, 0, 0) if is_black_bw(x, y) else (255, 255, 255)
            preview.save(preview_path)
        bw = pack_bitplane_bool(width, height, lambda x, y: is_black_bw(x, y))
        return (bw,)

    # 3-color format: black plane + red plane
    if preview_path:
        for y in range(height):
            for x in range(width):
                if is_red(x, y):
                    pr[x, y] = (255, 0, 0)
                elif is_black_bw(x, y):
                    pr[x, y] = (0, 0, 0)
                else:
                    pr[x, y] = (255, 255, 255)
        preview.save(preview_path)

    black_plane = pack_bitplane_bool(
        width, height, lambda x, y: (is_black_bw(x, y) and not is_red(x, y))
    )
    red_plane = pack_bitplane_bool(width, height, is_red)

    return (black_plane, red_plane)


# HTTP upload ------------------------------------------------------------------


def upload_planes_json(url, width, height, fmt, planes, force_full=False, timeout=30):
    """
    Compose JSON with base64 payload and POST it to the esp endpoint.
    'planes' is tuple of bytes: (bw,) or (black, red)
    """
    combined = b"".join(planes)
    payload = {
        "width": int(width),
        "height": int(height),
        "format": fmt,
        "data": base64.b64encode(combined).decode("ascii"),
        "forceFull": bool(force_full),
    }
    headers = {"Content-Type": "application/json"}
    r = requests.post(url, json=payload, headers=headers, timeout=timeout)
    return r


# CLI and main -----------------------------------------------------------------


def parse_args():
    p = argparse.ArgumentParser(
        description="Convert image to bitplanes and upload to ESP e-paper /image endpoint"
    )
    p.add_argument("-f", "--file", required=True, help="Input image file (PNG/JPG/etc)")
    p.add_argument(
        "-u",
        "--url",
        default="http://192.168.4.1/image",
        help="Endpoint URL (default AP address)",
    )
    p.add_argument("-W", "--width", type=int, default=128, help="Target width (px)")
    p.add_argument("-H", "--height", type=int, default=296, help="Target height (px)")
    p.add_argument(
        "-F",
        "--format",
        choices=["3c", "bw"],
        default="3c",
        help="Output format: 3c or bw",
    )
    p.add_argument(
        "--force-full",
        action="store_true",
        help="Force a full update on the device (slower but more reliable)",
    )
    p.add_argument(
        "--bw-threshold",
        type=int,
        default=128,
        help="Luminance threshold for black detection (0-255)",
    )
    p.add_argument(
        "--red-threshold",
        type=int,
        default=160,
        help="Red channel threshold to detect red regions (0-255)",
    )
    p.add_argument(
        "--red-delta",
        type=int,
        default=60,
        help="How much red must exceed other channels to be considered red",
    )
    p.add_argument(
        "--preview",
        help="Save local preview image of the processed result (helpful to tune thresholds)",
    )
    p.add_argument("--timeout", type=int, default=30, help="HTTP request timeout (s)")
    return p.parse_args()


def main():
    args = parse_args()
    print("Loading image:", args.file)
    try:
        img = Image.open(args.file)
    except Exception as e:
        print("Failed to open image:", e)
        sys.exit(1)

    print("Converting to {} ({}x{})...".format(args.format, args.width, args.height))
    if args.format == "bw":
        planes = image_to_1bit_planes(
            img,
            args.width,
            args.height,
            format_="bw",
            bw_threshold=args.bw_threshold,
            preview_path=args.preview,
        )
    else:
        planes = image_to_1bit_planes(
            img,
            args.width,
            args.height,
            format_="3c",
            bw_threshold=args.bw_threshold,
            red_threshold=args.red_threshold,
            red_delta=args.red_delta,
            preview_path=args.preview,
        )
    # Show sizes
    total_bytes = sum(len(p) for p in planes)
    print(
        "Prepared %d plane(s), total %d bytes (base64 -> ~%d bytes)"
        % (len(planes), total_bytes, (total_bytes * 4 + 2) // 3)
    )

    print("Uploading to:", args.url)
    try:
        r = upload_planes_json(
            args.url,
            args.width,
            args.height,
            args.format,
            planes,
            force_full=args.force_full,
            timeout=args.timeout,
        )
        print("Server response:", r.status_code)
        print(r.text)
        if r.status_code == 200:
            print("Upload OK")
        else:
            print("Upload failed (http %d)" % r.status_code)
    except Exception as e:
        print("Upload error:", e)
        sys.exit(1)


if __name__ == "__main__":
    main()
