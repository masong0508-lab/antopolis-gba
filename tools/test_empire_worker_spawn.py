#!/usr/bin/env python3
"""Compile the exact production Empire worker routines and exercise live paths.

The fixture uses tiny map/FX stubs; the spawn and movement functions themselves
are extracted unchanged from src/empire.c, so these are behavioral regressions,
not just text-grep checks. Requires only a host C99 compiler.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

def production_function(source: str, name: str) -> str:
    match = re.search(r"static void " + re.escape(name) + r"\([^)]*\)\s*\{", source)
    assert match, f"missing production function {name}"
    pos, depth = match.end(), 1
    while depth:
        assert pos < len(source), f"unterminated {name}"
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[match.start():pos]

FIXTURE = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
#define EWN 12
#define MAXW 8
#define NOPATH 65535u
#define B_NEST 1
#define FX_PLUS 1
typedef struct { u8 x,y,ox,oy,age,alive,b,ph,pl,pi,fl; } EW;
typedef struct { u8 x,y,t,staff; } EB;
static EW ew[EWN];
static EB ebl[80];
static u16 dnp[1][MAXW][MAXW], wpth[EWN][100];
static u8 nbl;
static unsigned random_value, random_calls, arrivals;
static unsigned rnd(void) { ++random_calls; return random_value; }
static void fx_add(u8 x, u8 y, u8 type) {
  (void)x; (void)y; assert(type == FX_PLUS); ++arrivals;
}
static u8 step_down(u8 *x, u8 *y, u16 (*field)[MAXW]) {
  if (*x && field[*y][*x - 1] < field[*y][*x]) {
    --*x; return 1;
  }
  return 0;
}
"""

TESTS = r"""
static void clear_state(void) {
  memset(ew, 0, sizeof ew);
  memset(ebl, 0, sizeof ebl);
  memset(wpth, 0, sizeof wpth);
  memset(dnp, 255, sizeof dnp);
  nbl = 0; random_value = 0; random_calls = arrivals = 0;
}
int main(void) {
  unsigned i;
  /* A large settlement with only one staffed, reachable workplace
     MUST spawn a visible ant. Eight random picks could miss it. */
  clear_state(); nbl = 80;
  for (i = 0; i < nbl; i++) { ebl[i].x = 3; ebl[i].y = 2; }
  ebl[79].t = 2; ebl[79].staff = 2; dnp[0][2][3] = 1;
  worker_spawn();
  assert(ew[0].alive && ew[0].b == 79);
  assert(ew[0].x == 3 && ew[0].y == 2);
  assert(random_calls == 1 && arrivals == 0);

  /* Do not spawn an ant whose required journey exceeds its 100-slot
     return-path buffer. Keep scanning and choose a valid alternative. */
  clear_state(); nbl = 80; random_value = 10;
  ebl[10].x = 3; ebl[10].y = 2; ebl[10].staff = 1; ebl[10].t = 2;
  dnp[0][2][3] = 101;
  ebl[70].x = 4; ebl[70].y = 3; ebl[70].staff = 1; ebl[70].t = 2;
  dnp[0][3][4] = 100;
  worker_spawn();
  assert(ew[0].alive && ew[0].b == 70 && random_calls == 1);

  clear_state(); nbl = 2;
  ebl[0].x = 3; ebl[0].y = 2; ebl[0].staff = 2; ebl[0].t = B_NEST;
  ebl[1].x = 4; ebl[1].y = 2; ebl[1].staff = 2; ebl[1].t = 2;
  dnp[0][2][3] = 0;
  worker_spawn(); // 1 is unreachable; 0 is the nest
  assert(!ew[0].alive && random_calls == 1);
  clear_state(); worker_spawn(); // no buildings, no RNG
  assert(random_calls == 0 && !ew[0].alive);

  /* The first inactive worker slot can be reused without disturbing a
     living worker, with a single random selection for the whole scan. */
  clear_state(); nbl = 1; ew[0].alive = 1;
  ebl[0].x = 3; ebl[0].y = 2; ebl[0].t = 2; ebl[0].staff = 1;
  dnp[0][2][3] = 1;
  worker_spawn();
  assert(ew[0].alive && ew[1].alive && ew[1].b == 0);
  assert(random_calls == 1);

  /* When map terraforming makes a running trip longer than 100 steps,
     give up safely rather than awarding a fake nest-arrival effect. */
  clear_state();
  ew[0].alive = 1; ew[0].x = 3; ew[0].y = 2; ew[0].pl = 100;
  dnp[0][2][3] = 5;
  worker_step(&ew[0]);
  assert(!ew[0].alive && arrivals == 0);

  /* Normal arrivals still play exactly one feedback effect. */
  clear_state();
  ew[0].alive = 1; ew[0].x = 3; ew[0].y = 2; ew[0].pl = 1;
  dnp[0][2][3] = 0;
  worker_step(&ew[0]);
  assert(ew[0].alive && ew[0].ph == 1 && ew[0].pi == 1);
  assert(arrivals == 1);

  /* Walk two steps home, acknowledge REAL arrival, then retrace the
     stored route exactly to the source workplace and despawn. */
  clear_state();
  ew[0].alive = 1; ew[0].x = 5; ew[0].y = 2;
  dnp[0][2][5] = 2; dnp[0][2][4] = 1; dnp[0][2][3] = 0;
  worker_step(&ew[0]);
  assert(ew[0].x == 4 && ew[0].pl == 1 && arrivals == 0);
  worker_step(&ew[0]);
  assert(ew[0].x == 3 && ew[0].pl == 2 && arrivals == 0);
  worker_step(&ew[0]);
  assert(ew[0].ph == 1 && ew[0].pi == 2 && arrivals == 1);
  worker_step(&ew[0]);
  assert(ew[0].x == 4 && ew[0].pi == 1);
  worker_step(&ew[0]);
  assert(ew[0].x == 5 && ew[0].pi == 0);
  worker_step(&ew[0]);
  assert(!ew[0].alive && arrivals == 1);
  puts("GBA Empire worker spawn: 80-building scan, one RNG draw, 100-step bound and honest round trips PASS");
  return 0;
}
"""

def main() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/empire.c").read_text()
    bodies = "\n".join(production_function(source, name)
                       for name in ("worker_spawn", "worker_step"))
    with tempfile.TemporaryDirectory(prefix="empire-worker-") as tmp:
        path = Path(tmp) / "worker.c"
        executable = Path(tmp) / "worker"
        path.write_text(FIXTURE + "\n" + bodies + "\n" + TESTS)
        compiler = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(compiler + [
            "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
            str(path), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

if __name__ == "__main__":
    main()
