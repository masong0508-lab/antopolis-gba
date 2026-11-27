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

# Compile the live arrival condition rather than a duplicated scoring rule.
# Check both colonies, priority of food, defense and siege, and free motion.
import os
import re
import shlex
import subprocess
import tempfile

arrival = re.search(r"  if \(a->carry \?.*?goto target_reached;", step, re.S)
assert arrival, "missing live arrival guard"
assert step.index("ph[a->y][a->x] =") < arrival.start()
assert arrival.end() < step.index("for (d = 0; d < 4; d++)")
assert step.index("target_reached:") < step.index("if (a->carry && a->x == nestx[t]")
program = r"""
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
#define MAXA 50
typedef struct { u8 x,y,carry; } Ant;
static Ant ant[MAXA];
static u8 nestx[2]={2,29}, nesty[2]={3,28};
static int hold(Ant *a, u8 t, u8 guard, u8 soldier) {
  u8 e=t^1;

""" + arrival.group() + r"""
  return 0;
target_reached:
  return 1;
}

int main(void) {
  for (uint8_t t=0;t<2;++t) {
    uint8_t e=t^1;
    Ant a={nestx[t],nesty[t],1};
    ant[0]=(Ant){12,13,0};
    assert(hold(&a,t,0,1));
    a.x=12; a.y=13;
    assert(!hold(&a,t,0,1)); /* food outranks the intruder */
    a.carry=0;
    assert(hold(&a,t,0,0)); /* defense even without an offensive army */
    a.x++;
    assert(!hold(&a,t,0,1));
    a.x=nestx[e]; a.y=nesty[e];
    assert(hold(&a,t,MAXA,1));
    assert(!hold(&a,t,MAXA,0));
    assert(!hold(&a,t,0,1)); /* a live defense target outranks siege */
    a.x--; assert(!hold(&a,t,MAXA,1));
  }
  return 0;
}

"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "arrival.c"
    exe = Path(tmp) / "arrival"
    src.write_text(program)
    subprocess.run(shlex.split(os.environ.get("HOSTCC", "cc")) +
                   ["-std=c99", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("Live colony arrival guard: workers, defense and siege passed")
