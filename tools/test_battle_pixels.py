#!/usr/bin/env python3
"""Compare actual optimized GBA battle drawing against the original pixels.

This builds the shipping production vplot/vplot_pair/bdraw functions with
portable VRAM stand-ins. A separately written reference implementation
replays the exact original draw order on odd/even and clipped coordinates.
No ARM SDK or emulator needed.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


def production_function(source: str, name: str) -> str:
    match = re.search(
        r"IWRAM\s+static\s+void\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{",
        source,
    )
    assert match, f"missing production {name}"
    pos, depth = match.end(), 1
    while depth:
        assert pos < len(source), f"unterminated production {name}"
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
typedef int16_t s16;
#define IWRAM
#define BMAX 1000
#define BT_W 0
#define BT_S 1
#define BT_M 2
#define BC_ME 5
#define BC_EN 7
#define BC_ACID 10
typedef struct { s16 x,y; u16 tg; u8 hp,side,type,cd; } BAnt;
typedef struct { s16 x,y,tx,ty; u8 t; } BShot;
static BAnt ba[BMAX];
static u16 bcount;
static BShot bsh[24];
static u8 bshn;
static u16 actual[240*160/2], reference[240*160/2];
static uint32_t random_state=0x19282491u;
static uint32_t next_random(void) {
  random_state=random_state*1664525u+1013904223u;
  return random_state;
}
static void old_bdraw(volatile u16 *pg) {
  u16 i; BAnt *a; int x,y; u8 c0,c1;
  for (i=0; i<bcount; i++) {
    a=&ba[i]; if(!a->hp) continue;
    x=a->x>>4; y=a->y>>4;
    c0=a->side ? BC_EN : BC_ME; c1=(u8)(c0+1);
    if (a->type==BT_M) {
      vplot(pg,x,y,c0); vplot(pg,x+1,y,c0);
      vplot(pg,x,y+1,c0); vplot(pg,x+1,y+1,c0);
      vplot(pg,x+(a->side?-1:2),y,c1);
    } else {
      vplot(pg,x,y,c0);
      vplot(pg,x+(a->side?-1:1),y,a->type==BT_S?BC_ACID:c1);
    }
  }
  for (i=0; i<bshn; i++) {
    BShot *q=&bsh[i];
    x=(q->tx+(q->x-q->tx)*q->t/8)>>4;
    y=(q->ty+(q->y-q->ty)*q->t/8)>>4;
    vplot(pg,x,y,BC_ACID);
  }
}
static void fill_scenario(unsigned scenario) {
  unsigned i;
  bcount=(u16)(scenario%3u==0u ? BMAX : scenario%3u==1u ? 5u : 80u);
  bshn=(u8)(scenario%4u==0u ? 24u : 7u);
  for(i=0;i<bcount;i++) {
    BAnt *a=&ba[i];
    int x=(int)(next_random()%247u)-3;
    int y=(int)(next_random()%167u)-3;
    a->x=(s16)(x*16); a->y=(s16)(y*16);
    a->hp=(u8)((i%17u)==0u ? 0u : 1u);
    a->side=(u8)(i&1u); a->type=(u8)((i+scenario)%3u);
  }
  for(i=0;i<bshn;i++) {
    BShot *s=&bsh[i];
    s->x=(s16)((int)(next_random()%240u)*16);
    s->y=(s16)((int)(next_random()%160u)*16);
    s->tx=(s16)((int)(next_random()%240u)*16);
    s->ty=(s16)((int)(next_random()%160u)*16);
    s->t=(u8)(next_random()%9u);
  }
}
int main(void) {
  unsigned scene;
  /* Aligned pair must replace only the intended two adjacent bytes. */
  memset(actual,0x4A,sizeof actual);
  vplot_pair(actual,0,0,21,22);
  assert(actual[0]==(u16)(21u|(22u<<8)) && actual[1]==0x4A4Au);
  /* Odd pair must preserve adjacent bytes of both halfwords. */
  memset(actual,0x4A,sizeof actual);
  vplot_pair(actual,1,0,21,22);
  assert(actual[0]==(u16)(0x4Au|(21u<<8)));
  assert(actual[1]==(u16)(22u|(0x4Au<<8)));
  memset(actual,0x4A,sizeof actual);
  vplot_pair(actual,239,159,21,22);
  assert(actual[240*160/2-1]==(u16)(0x4Au|(21u<<8)));

  for(scene=0;scene<48;scene++) {
    fill_scenario(scene);
    memset(actual,0x4A,sizeof actual);
    memset(reference,0x4A,sizeof reference);
    bdraw(actual);
    old_bdraw(reference);
    if(memcmp(actual,reference,sizeof actual)!=0) {
      fprintf(stderr,"battle pixels differ in scene %u\n",scene);
      return 1;
    }
  }
  puts("GBA battle pixel pairs: 48 scenarios, up to 1000 ants, clipping, acid shots, all sprites EXACT");
  return 0;
}
"""


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/battle.c").read_text()
    bodies = "\n".join(
        production_function(source, name)
        for name in ("vplot", "vplot_pair", "bdraw")
    )
    with tempfile.TemporaryDirectory(prefix="battle-pixels-") as directory:
        path = Path(directory) / "pixel_test.c"
        exe = Path(directory) / "pixel_test"
        path.write_text(
            FIXTURE.replace("static void old_bdraw", bodies + "\nstatic void old_bdraw")
        )
        cc = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(
            cc + ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                  "-pedantic", str(path), "-o", str(exe)], check=True
        )
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
