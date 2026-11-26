#!/usr/bin/env python3
"""Compile the real cartridge spawn routine against the original two-pass behavior."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.c").read_text()
match = re.search(r"static u8 spawn\(u8 team\) \{", source)
assert match
end = match.end()
depth = 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
spawn = source[match.start():end]
assert spawn.count("i < MAXA") == 1
fixture = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;
typedef struct { u8 x,y,team,alive,carry,sol,ox,oy,age,fl; } Ant;
#define MAXA 48
static Ant ant[MAXA];
static uint8_t nestx[2]={3,27}, nesty[2]={7,22}, hatched[2];
static uint32_t seed=0x10f96ab3u;
static uint32_t next(void) { seed=seed*1664525u+1013904223u; return seed; }
"""
reference = r"""
static uint8_t reference(uint8_t team) {
  uint8_t i,n=0;
  for(i=0;i<MAXA;++i) if(ant[i].alive && ant[i].team==team) ++n;
  if(n>=MAXA/2) return 0;
  for(i=0;i<MAXA;++i) if(!ant[i].alive) {
    ant[i].x=ant[i].ox=nestx[team]; ant[i].y=ant[i].oy=nesty[team]; ant[i].age=8; ant[i].fl=team;
    ant[i].team=team; ant[i].alive=1; ant[i].carry=0;
    ant[i].sol=((hatched[team]++&3)==0);
    return 1;
  }
  return 0;
}
int main(void) {
  Ant before[MAXA], expected[MAXA];
  uint8_t h[2], want[2];
  for(unsigned k=0;k<16000;++k) {
    memset(ant,0,sizeof ant);
    for(unsigned i=0;i<MAXA;++i) {
      ant[i].alive=(uint8_t)(k%4==0 ? 1 : k%4==1 ? 0 : (next()>>16)%2);
      ant[i].team=(uint8_t)(k%7==0 ? 0 : (next()>>16)%2);
      ant[i].x=(uint8_t)(next()>>16); ant[i].y=(uint8_t)(next()>>16);
      ant[i].carry=(uint8_t)(next()>>16); ant[i].sol=(uint8_t)(next()>>16);
      ant[i].ox=91; ant[i].oy=77; ant[i].age=3; ant[i].fl=1;
    }
    hatched[0]=(uint8_t)(next()>>16); hatched[1]=(uint8_t)(next()>>16);
    memcpy(before,ant,sizeof ant); memcpy(h,hatched,sizeof h);
    uint8_t team=(uint8_t)(k&1), result=reference(team);
    memcpy(expected,ant,sizeof ant); memcpy(want,hatched,sizeof want);
    memcpy(ant,before,sizeof ant); memcpy(hatched,h,sizeof h);
    assert(spawn(team)==result);
    assert(memcmp(expected,ant,sizeof ant)==0);
    assert(memcmp(want,hatched,sizeof want)==0);
  }
  puts("Colony hatching: 16000 original-vs-new slot, quota, caste and counter cases PASS");
  return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="colony-hatching-") as d:
    p=Path(d)/"test.c"
    exe=Path(d)/"test"
    p.write_text(fixture+"\n"+spawn+"\n"+reference)
    cc=shlex.split(os.environ.get("HOSTCC","cc"))
    subprocess.run(cc+["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
