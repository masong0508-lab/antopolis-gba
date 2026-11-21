#!/usr/bin/env python3
"""Make sure the shipped 4bpp GBA world fruit tiles match their pixel sketches.

Source art2.py intentionally owns both berries. A readable leafy silhouette
matters in the overhead map, including the co-op guide's DUAL VIEW, but
generating all 88 KB of art2.h need not be part of each native host run.
"""
import ast
import re
from pathlib import Path

source = ast.parse(Path("tools/art2.py").read_text())
def fruit_rows(name):
    nodes = [
        node for node in source.body
        if isinstance(node, ast.Assign)
        and any(isinstance(target, ast.Name) and target.id == name
                for target in node.targets)
    ]
    assert len(nodes) == 1, name
    expr = nodes[0].value
    assert isinstance(expr, ast.Call) and isinstance(expr.func, ast.Name)
    assert expr.func.id == "parse" and len(expr.args) == 1
    rows = ast.literal_eval(expr.args[0])
    assert len(rows) == 8 and all(len(row) == 8 for row in rows)
    return rows

def encode(rows):
    words = []
    for row in rows:
        word = 0
        for x, ch in enumerate(row):
            color = 0 if ch == "." else int(ch, 16)
            assert color < 16
            word |= color << (4 * x)
        words.append(word)
    return words

gold = fruit_rows("FOOD_A")
red = fruit_rows("FOOD_B")
assert gold != red
assert all(any(c == "4" for c in "".join(rows)) for rows in (gold, red))
assert "2" in "".join(gold) and "3" in "".join(gold)
assert "5" in "".join(red) and "3" in "".join(red)
content = Path("src/art2.h").read_text()
match = re.search(r"static const u32 DECO\[104\]\s*=\s*\{(.*?)\};", content, re.S)
assert match, "Missing GBA decor tile atlas"
actual = [int(word, 16) for word in re.findall(
    r"0x([0-9A-Fa-f]{8})", match.group(1)
)]
assert len(actual) == 104, "Unexpected decor atlas size; check VRAM budget"
assert actual[:16] == encode(gold) + encode(red), (
    "Generated 4bpp world fruit art is out of sync with tools/art2.py"
)
# Reconstruct only the three pure world-generation functions from the
# checked-in source AST. This deliberately avoids importing Pillow or
# regenerating unrelated sprites, and runs on the native host runner.
# Every one of the 324 (4 height x 81 edge) shipped tiles must exactly
# match the generator: 10,368 bytes total, no extra VRAM allocations.
functions = [n for n in source.body if isinstance(n, ast.FunctionDef)
             and n.name in ("hsh", "base", "terr_tile")]
assert {f.name for f in functions} == {"hsh", "base", "terr_tile"}
palette_code = compile(ast.Module(body=functions, type_ignores=[]),
                       "tools/art2.py:terrain", "exec")
terrain_ns = {}
exec(palette_code, terrain_ns)
tile_at = terrain_ns["terr_tile"]
atlas = re.search(r"static const u32 TERR\[2592\]\s*=\s*\{(.*?)\};",
                  content, re.S)
assert atlas, "GBA terrain atlas resized or missing"
terrain_words = [int(word, 16) for word in
                 re.findall(r"0x([0-9A-Fa-f]{8})", atlas.group(1))]
assert len(terrain_words) == 2592
for height in range(4):
    for edge_code in range(81):
        pixels = tile_at(height, edge_code)
        assert len(pixels) == 8 and all(len(row) == 8 for row in pixels)
        expect = [sum(pixels[y][x] << (4 * x) for x in range(8))
                  for y in range(8)]
        first = (height * 81 + edge_code) * 8
        assert terrain_words[first:first + 8] == expect, (
            f"Terrain palette tile stale: level={height}, edges={edge_code}"
        )
# Visible foamy shallows occupy row 1 next to land (not the whole tile).
assert 14 in tile_at(0, 1)[1]
assert 14 in tile_at(0, 9)[6]
# Taller drops contain physical shadow strata, short steps stay less harsh.
assert 13 in tile_at(3, 2)[2]
assert 9 in tile_at(3, 18)[5]
world = Path("src/main.c").read_text()
assert "h * 81 + c" in world and "draw_around(u8 x, u8 y)" in world
assert "draw_cell(x, y)" in world and "BGMAP0" in world
print("GBA world fruit art PASS: two legible 8x8 tiles, unchanged 104-word DECO atlas")
print("GBA edge art PASS: 324 terrain tiles exactly match 4bpp atlas, foam + cliff strata")
