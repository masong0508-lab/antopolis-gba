#!/usr/bin/env python3
"""Native-resolution DUAL VIEW screenshot regression; stdlib PNG only.

Reject blank/flat sky, missing terrain/shadows, broken HUD and duplicate
scout captures. This runs against actual two-console mGBA gameplay and
writes measured results into the linked test artifact for visual audit.
"""
import json
import struct
import sys
import zlib
from pathlib import Path

def png_rgb(path):
    data = Path(path).read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n"), path
    offset = 8
    stream = b""
    width = height = channels = 0
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        offset += 12 + size
        if kind == b"IHDR":
            width, height, bits, color, comp, filt, interlace = (
                struct.unpack(">IIBBBBB", payload))
            assert bits == 8 and color in (2, 6)
            assert (comp, filt, interlace) == (0, 0, 0)
            channels = 3 if color == 2 else 4
        if kind == b"IDAT":
            stream += payload
        if kind == b"IEND":
            break
    assert (width, height) == (480, 160), (path, width, height)
    raw = zlib.decompress(stream)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        method = raw[offset]
        row = bytearray(raw[offset + 1:offset + stride + 1])
        assert len(row) == stride and method <= 4
        offset += stride + 1
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            if method == 1: pred = left
            elif method == 2: pred = up
            elif method == 3: pred = (left + up) // 2
            elif method == 4:
                p = left + up - corner
                da, db, dc = abs(p - left), abs(p - up), abs(p - corner)
                pred = left if da <= db and da <= dc else up if db <= dc else corner
            else: pred = 0
            row[x] = (row[x] + pred) & 255
        rows.append([tuple(row[x:x + 3]) for x in range(0, stride, channels)])
        previous = row
    return rows

def audit(path, scout_half):
    rows = png_rgb(path)
    x0 = 240 * scout_half
    sky = [rows[y][x] for y in range(12, 65)
           for x in range(x0 + 20, x0 + 220)]
    ground = [rows[y][x] for y in range(85, 125)
              for x in range(x0 + 20, x0 + 220)]
    hud = [rows[y][x] for y in range(128, 158)
           for x in range(x0 + 8, x0 + 232)]
    blue = sum(b > r + 20 and b > g + 3 for r, g, b in sky)
    earth = sum((r > g * 1.07 and g > b * 1.12 and r > 40) or
                (g > r * 1.20 and g > b * 1.14) for r, g, b in ground)
    dark = sum(r < 50 and g < 50 and b < 55 for r, g, b in ground)
    assert blue > 2700, (path, "sky vanished", blue)
    assert earth > 2900, (path, "ground vanished", earth)
    assert dark > 50, (path, "ground shadows/landmarks vanished", dark)
    assert len(set(sky)) >= 16, (path, "flat skyline")
    assert len(set(ground)) >= 5, (path, "flat ground")
    assert len(set(hud)) >= 7, (path, "HUD missing")
    return dict(image=Path(path).name, scout_half=scout_half,
                sky_blue_pixels=blue, ground_material_pixels=earth,
                dark_ground_pixels=dark, sky_colors=len(set(sky)),
                ground_colors=len(set(ground)), hud_colors=len(set(hud)))

def main():
    a, b = audit(sys.argv[1], 1), audit(sys.argv[2], 0)
    assert Path(sys.argv[1]).read_bytes() != Path(sys.argv[2]).read_bytes()
    results = dict(size="2 x 240x160", mode="DUAL VIEW",
                   criteria="sky, terrain, shadows, HUD, distinct viewpoints",
                   screenshots=[a, b])
    if len(sys.argv) > 3:
        path = Path(sys.argv[3])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(results, indent=2) + "\n")
    print("GBA linked ANT EYE screenshots PASS: two native-resolution perspectives")
    print(json.dumps(results, indent=2))

if __name__ == "__main__":
    main()
