#!/usr/bin/env python3
"""Compile production city graph and food counter; compare original output."""
from pathlib import Path
import os, re, shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
city=(root/"src/city.c").read_text()
main=(root/"src/main.c").read_text()
graph=re.search(r"static void graph\(u8 row0, u8 which\) \{.*?\n\}",city,re.S).group()
food=re.search(r"static u16 count_food_grid\(void\) \{.*?\n\}",main,re.S).group()
assert "nfood = count_food_grid();" in main
assert "j < eon && eslot < 128u" in main
assert main.count("eye_shadow_room(eslot, j, eon)")==2
fixture=r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int16_t s16;
typedef uint32_t u32;typedef int32_t s32;
#define MAPT 832
static u8 W,H,nh2;
static u16 hpop[48], BGMAP1[1024];
static s16 hcash[48];
static u32 TILE32[8192], expected[8192];
static u16 expected_map[1024];
static u8 food[96][96];
static u32 state=0x46128719;
static u32 next(void){state ^= state<<13;state ^=state>>17;state^=state<<5;return state;}
"""
reference=r"""
static void old_graph(u8 row0, u8 which) {
  u8 tx, ty, y, x, i, n = (u8)(nh2 < 2 ? 2 : nh2); s32 mx = 1, mn = 0; u32 w;
  for (i = 0; i < nh2; i++) { s32 v = which ? hcash[i] : hpop[i]; if (v > mx) mx = v; if (v < mn) mn = v; }
  for (ty = 0; ty < 5; ty++) for (tx = 0; tx < 12; tx++) {
    for (y = 0; y < 8; y++) {
      u8 py = (u8)(ty * 8 + y); w = 0;
      for (x = 0; x < 8; x++) {
        u8 px = (u8)(tx * 8 + x), c = (py == 39 || px == 0) ? 11 : ((py & 7) == 0 && (px & 1)) ? 12 : 1;
        u8 k = (u8)((u16)px * (n - 1) / 95), k0 = (u8)((u16)(px ? px - 1 : 0) * (n - 1) / 95);
        if (k < nh2) {
          s32 v1 = which ? hcash[k] : hpop[k], v0 = which ? hcash[k0] : hpop[k0];
          u8 a1 = (u8)(38 - (v1 - mn) * 37 / (mx - mn)), a0 = (u8)(38 - (v0 - mn) * 37 / (mx - mn)), lo = a1 < a0 ? a1 : a0, hi = a1 < a0 ? a0 : a1;
          if (py >= lo && py <= hi) c = which ? (u8)(v1 < 0 ? 7 : 10) : 6;
          if (which && mn < 0 && py == (u8)(38 - (0 - mn) * 37 / (mx - mn)) && c == 1) c = 12;
        }
        w |= (u32)c << (4 * x);
      }
      TILE32[(MAPT + which * 60 + ty * 12 + tx) * 8 + y] = w;
    }
    BGMAP1[(row0 + ty) * 32 + 9 + tx] = (u16)((MAPT + which * 60 + ty * 12 + tx) | (6 << 12));
  }
}
"""
tests=r"""
int main(void){
  unsigned cases=0;
  // Every history length, all-positive/negative/mixed and saturated values.
  for(unsigned n=0;n<=48;n++) for(unsigned pattern=0;pattern<8;pattern++){
    nh2=(u8)n;
    for(unsigned i=0;i<n;i++){
      hpop[i]=(u16)(pattern==0?0:next()%65001u);
      hcash[i]=(s16)(pattern==0?0:pattern==1?-32000:pattern==2?32000:(int)(next()%64001u)-32000);
    }
    memset(TILE32,0,sizeof TILE32);memset(BGMAP1,0,sizeof BGMAP1);
    old_graph(3,0);old_graph(10,1);
    memcpy(expected,TILE32,sizeof expected);memcpy(expected_map,BGMAP1,sizeof expected_map);
    memset(TILE32,0,sizeof TILE32);memset(BGMAP1,0,sizeof BGMAP1);
    graph(3,0);graph(10,1);
    assert(!memcmp(expected,TILE32,sizeof expected));
    assert(!memcmp(expected_map,BGMAP1,sizeof expected_map));
    ++cases;
  }
  // Check all dimensions, including tails and every row stride.
  for(unsigned y=0;y<96;y++)for(unsigned x=0;x<96;x++)food[y][x]=(u8)next();
  for(W=1;W<=96;W++)for(H=1;H<=96;H++){
    u16 sum=0;
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++)sum+=food[y][x];
    assert(count_food_grid()==sum);
  }
  printf("City graph: %u original-pixel comparisons; food: 9216 map dimensions PASS\n",cases);
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"opt.c";exe=Path(tmp)/"opt"
    p.write_text(fixture+reference+graph+"\n"+food+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
        ["-std=c99","-O2","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
