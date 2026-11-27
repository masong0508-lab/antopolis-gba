#!/usr/bin/env python3
"""Differential test of the actual City/Antverse tunnel-access function."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/city.c").read_text()
current = re.search(r"static u8 has_access\(u8 x, u8 y\) \{.*?\n\}", source, re.S)
assert current
assert "if (ETUN(b, a)) return 1;" in current.group()
fixture = r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef int8_t s8;
static u8 W,H, nestx[2],nesty[2], ezt[96][96];
#define ETUN(y,x) (ezt[y][x] >> 7)
static u8 dist(u8 a,u8 b){return a>b?a-b:b-a;}
static uint32_t rng = 1;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
"""
reference = r"""
static u8 reference(u8 x, u8 y) {                 // a tunnel (or the nest) within 2 squares
  s8 a, b;
  if (dist(x, nestx[0]) <= 3 && dist(y, nesty[0]) <= 3) return 1;
  for (b = (s8)(y - 2); b <= y + 2; b++) for (a = (s8)(x - 2); a <= x + 2; a++)
    if (a >= 0 && b >= 0 && a < W && b < H && ETUN(b, a)) return 1;
  return 0;
}
"""
tests = r"""
int main(void) {
  unsigned checks=0;
  // All 81 supported rectangular map sizes; both sparse and dense
  // layouts, every cell and all edges, plus near-nest shortcuts.
  for(unsigned w=32;w<=96;w+=8) for(unsigned h=32;h<=96;h+=8) {
    W=(u8)w; H=(u8)h;
    for(unsigned mode=0;mode<6;mode++){
      nestx[0]=(u8)(next()%W); nesty[0]=(u8)(next()%H);
      memset(ezt,0,sizeof ezt);
      for(unsigned y=0;y<96;y++) for(unsigned x=0;x<96;x++) {
        // Fill out-of-world cells too, so clipping is observable.
        unsigned sample=next()>>24;
        ezt[y][x]=(u8)(sample&127u);
        if(x>=w||y>=h||(mode&&sample<(1u<<(mode+1))))ezt[y][x]|=128;
      }
      for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++){
        assert(has_access((u8)x,(u8)y)==reference((u8)x,(u8)y));
        ++checks;
      }
    }
  }
  printf("City tunnel access: %u production-vs-reference cases PASS\n",checks);
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/"test.c"
    exe=Path(tmp)/"test"
    src.write_text(fixture+current.group()+reference+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
                   ["-std=c99","-Wall","-Wextra","-Werror","-pedantic","-O2",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
