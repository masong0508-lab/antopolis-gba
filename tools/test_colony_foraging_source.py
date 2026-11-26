from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = (root / "src/main.c").read_text()
assert '#include "colony_foraging.h"' in source
step = source.split("static void step_ant(Ant *a) {", 1)[1].split("static void grow_food(", 1)[0]
assert "if (!can_go(a->x, a->y, DX[d], DY[d])) continue;" in step
assert "sc = colony_forager_score(ph[ty][tx], food[ty][tx], (u8)(rnd() & 31), dh," in step
assert "(u8)(tx == px && ty == py));" in step
assert "food[ty][tx] ? 120" not in step
assert "if (sc > best_sc)" in step
print("GBA colony foraging: shipping movement uses tested score")
