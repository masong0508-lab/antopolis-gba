#!/usr/bin/env python3
"""Prevent full-world BFS and berry recount on every editor menu highlight.

This checks production wiring: on large 96x96 maps a menu cursor move
must redraw text without recomputing all 9,216 cells or searching paths.
"""
from pathlib import Path
code = Path("src/main.c").read_text()
resize = code.split("static void ed_resize(",1)[1].split("#define EM_N",1)[0]
menu_draw = code.split("static void em_draw(",1)[1].split("static u8 ed_menu(",1)[0]
menu_input = code.split("static u8 ed_menu(",1)[1].split("static u8 editor(",1)[0]
assert "static u8 em_dirty = 1, em_ok;" in code
assert "static u16 em_berries;" in code
assert "em_dirty = 1;" in resize, "world size changes must invalidate the summary"
assert "if (em_dirty) {" in menu_draw
before, rest = menu_draw.split("if (em_dirty) {",1)
cached, after = rest.split("em_dirty = 0;",1)
assert "bfs(0);" in cached and "for (y = 0; y < H; y++)" in cached
assert "bfs(0);" not in after and "bfs(0);" not in before
assert "em_berries > 99u" in menu_draw and "em_berries > FOOD_PATCH_LIMIT" in menu_draw
assert "em_ok ? \"ANTS CAN WALK\"" in menu_draw
assert "em_dirty = 1; // opening" in menu_input
assert "em_dirty = 1; sfx_flood();" in menu_input, "CLEAR to grass must invalidate"
assert "ed_resize(sel == 3 ? v : mw, sel == 4 ? v : mh)" in menu_input
assert "em_draw(sel, conf);" in menu_input, "selection still repaints menu"
print("PASS: Map Maker BFS and 9,216-cell berry scan run only on open/resize/clear")
