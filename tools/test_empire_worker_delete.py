#!/usr/bin/env python3
"""Test real Empire building deletion and animated worker index integrity."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


def function(source: str, name: str) -> str:
    m = re.search(r"static void " + re.escape(name) + r"\([^)]*\)\s*\{", source)
    assert m, f"missing function {name}"
    p, depth = m.end(), 1
    while depth:
        assert p < len(source)
        depth += (source[p] == "{") - (source[p] == "}")
        p += 1
    return source[m.start():p]


FIXTURE = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
#define EWN 12
#define B_BRIDGE 28
#define B_WONDER 12
#define FX_PUFF 8
typedef struct { u8 x,y,t,hp,staff; } EB;
typedef struct { u8 x,y,ox,oy,age,alive,b,ph,pl,pi,fl; } EW;
static EB ebl[160];
static EW ew[EWN];
static u8 ebi[96][96], hgt[96][96], nbl;
static u16 wprog;
static unsigned redraws, counted, puffs;
static void draw_around(u8 x,u8 y) {
  assert(x<96 && y<96); redraws++;
}
static void count_b(void) { counted++; }
static void fx_add(u8 x,u8 y,u8 effect) {
  assert(x<96 && y<96 && effect==FX_PUFF); puffs++;
}
"""

TESTS = r"""
static void set_world(unsigned buildings) {
  unsigned i;
  memset(ebl,0,sizeof ebl);
  memset(ew,0,sizeof ew);
  memset(ebi,0,sizeof ebi);
  memset(hgt,0,sizeof hgt);
  nbl=(u8)buildings; counted=redraws=puffs=0; wprog=40;
  for(i=0;i<buildings;i++) {
    u8 x=(u8)(5+i%80u),y=(u8)(10+i/80u);
    ebl[i].x=x; ebl[i].y=y; ebl[i].t=2;
    ebl[i].hp=(u8)(i+1u); ebl[i].staff=(u8)(1u+i%4u);
    ebi[y][x]=(u8)(i+1u); hgt[y][x]=2u;
  }
}
static void verify_world(unsigned before, unsigned deleted) {
  unsigned i;
  assert(nbl==before-1u && counted==1 && puffs==1);
  for(i=0;i<nbl;i++) {
    unsigned original = i < deleted ? i : i + 1u;
    u8 x=(u8)(5+original%80u), y=(u8)(10+original/80u);
    assert(ebl[i].x==x && ebl[i].y==y);
    assert(ebi[y][x]==i+1u);
  }
  assert(ebi[10u+deleted/80u][5u+deleted%80u]==0);
  for(i=0;i<EWN;i++) if(ew[i].alive)
    assert(ew[i].b<nbl);
}
int main(void) {
  unsigned i;
  // Demolishing a middle building must retire its own workers and
  // remap every later worker index to the packed building list.
  set_world(6);
  for(i=0;i<EWN;i++) {
    ew[i].alive=1; ew[i].b=(u8)(i%6u);
  }
  del_b(2);
  verify_world(6,2);
  for(i=0;i<EWN;i++) {
    if(i%6u==2u) assert(!ew[i].alive);
    else assert(ew[i].alive &&
      ew[i].b==(u8)(i%6u > 2u ? i%6u - 1u : i%6u));
  }

  // First/last deletions, including the full 160-building capacity.
  set_world(160);
  for(i=0;i<EWN;i++) { ew[i].alive=1; ew[i].b=(u8)(148u+i); }
  del_b(0);
  verify_world(160,0);
  for(i=0;i<EWN;i++) assert(ew[i].alive && ew[i].b==147u+i);
  del_b(158);
  verify_world(159,158);
  for(i=0;i<EWN;i++) if(i==11u) assert(!ew[i].alive);
  // When nothing survives after deleting the sole workplace, the next
  // spawn cannot attribute traffic to a ghost building.
  set_world(1); ew[0].alive=1; ew[0].b=0;
  del_b(0); verify_world(1,0);
  assert(!ew[0].alive);

  // Real bridge/wonder cleanup must still execute without touching an
  // unrelated worker trip.
  set_world(3);
  ebl[1].t=B_BRIDGE; ew[0].alive=1; ew[0].b=2;
  del_b(1); verify_world(3,1);
  assert(redraws==1 && hgt[10][6]==0 && ew[0].b==1);
  set_world(2); ebl[1].t=B_WONDER;
  del_b(1); verify_world(2,1);
  assert(wprog==0 && redraws==0);
  puts("GBA Empire deletion: 160-building compaction, valid worker refs, bridge and wonder cleanup PASS");
  return 0;
}
"""

def main() -> None:
    source = (Path(__file__).resolve().parents[1] / "src/empire.c").read_text()
    code = FIXTURE + "\n" + function(source, "del_b") + "\n" + TESTS
    with tempfile.TemporaryDirectory(prefix="empire-delete-") as work:
        path = Path(work) / "delete.c"
        executable = Path(work) / "delete"
        path.write_text(code)
        cc = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(cc + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                             "-pedantic", str(path), "-o", str(executable)],
                       check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
