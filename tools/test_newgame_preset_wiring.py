"""Check that the actual GBA New Game menu exposes both presets.

This guards the UI/control wiring as well as the native pure presets.
Existing manual settings, SELECT seed rerolls and custom maps survive.
"""
from pathlib import Path
src = Path("src/main.c").read_text()
settings = src.split("static u8 newgame_menu(void)",1)[1].split("// ---------- the HOME screen",1)[0]
text = src.split("static void ng_text(void)",1)[1].split("static void ng_draw(void)",1)[0]
draw = src.split("static void ng_draw(void)",1)[1].split("static u8 newgame_menu(void)",1)[0]
assert 'if (p & (J_L | J_R))' in settings
assert 'ant_newgame_preset((u8)((p & J_R) != 0))' in settings
assert 'diff = q.difficulty; mapt = q.map; tw = q.width; th = q.height;' in settings
assert 'sandbox = q.sandbox; ng_learn = q.lessons' in settings
assert 'ng_text(); ng_preview(); continue;' in settings
assert '(p & J_SEL)' in settings, "Existing randomized seed should remain available"
assert 'if (tsel == 0) diff =' in settings, "Manual difficulty adjustment must remain"
assert 'if (p & J_DOWN)' in settings and 'if (p & J_UP)' in settings
assert '"GUIDED LESSONS"' in text and '"HARD SURVIVAL"' in text
assert 'L GUIDE R WILD' in draw and 'SEL: RESEED' in text
print("PASS: New Game has L guided/R wild, visible instructions and manual controls")
