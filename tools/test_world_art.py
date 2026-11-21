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
match = re.search(r"static const u32 DECO\\[104\\]\\s*=\\s*\\{(.*?)\\};", content, re.S)
assert match, "Missing GBA decor tile atlas"
actual = [int(word, 16) for word in re.findall(
    r"0x([0-9A-Fa-f]{8})", match.group(1)
)]
assert len(actual) == 104, "Unexpected decor atlas size; check VRAM budget"
assert actual[:16] == encode(gold) + encode(red), (
    "Generated 4bpp world fruit art is out of sync with tools/art2.py"
)
print("GBA world fruit art PASS: two legible 8x8 tiles, unchanged 104-word DECO atlas")
