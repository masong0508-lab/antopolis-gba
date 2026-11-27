#!/usr/bin/env python3
"""Compile the actual GBA Classic target scan and worker movement."""
from pathlib import Path
import re
import os
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "src/main.c").read_text()
def function(name):
    m = re.search(r"static void "+name+r"\([^)]*\) \{", source)
    assert m
    i=m.end()
    depth=1
    while depth:
        depth+=(source[i]=="{")-(source[i]=="}")
        i+=1
    return source[m.start():i]
assert "count(ncnt); refresh_defense();" in source
assert "count(ncnt); defend_i[0] = defend_i[1] = MAXA;" in source
fixture=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "colony_foraging.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef struct { u8 x,y,team,alive,carry,sol,ox,oy,age,fl; } Ant;
#define MAXA 48
#define MAXW 32
#define NOPATH 65535
#define O(x) 1
#define O_REDATK 0
#define O_MYATK 1
#define MANA_PER_FOOD 1
#define MANA_MAX 20
#define FX_PLUS 9
#define FX_HEART 12
static Ant ant[MAXA];
static u8 hgt[32][32],ph[32][32],food[32][32];
static u16 maps[2][32][32];
static u16 (*dnp[2])[32]={maps[0],maps[1]};
static u8 nestx[2],nesty[2],stock[2],qhp[2],ncnt[2],defend_i[2];
static u8 cht[16],mana,mana2,mp_on,sandbox,tut,qfl[2],qlost;
static u16 st_food;
static const s8 DX[4]={1,-1,0,0},DY[4]={0,0,1,-1};
static unsigned rng_calls;
static unsigned rnd(void){++rng_calls;return 0;}
static u8 dist(u8 a,u8 b){return a>b?a-b:b-a;}
static u8 can_go(u8 x,u8 y,s8 dx,s8 dy){
  int xx=x+dx,yy=y+dy;
  return xx>=0&&xx<32&&yy>=0&&yy<32&&hgt[yy][xx]!=0;
}
static void draw_dec(u8 x,u8 y){(void)x;(void)y;}
static void fx_add(u8 x,u8 y,u8 f){(void)x;(void)y;(void)f;}
static void apr(s8 d){(void)d;}
static void alert(u8 x,u8 y,const char *m){(void)x;(void)y;(void)m;}
static void sfx_food(void){}
static void sfx_hit(void){}
static void sfx_qdead(void){}
static void sfx_fight(void){}
static u8 mp_vs(void){return 0;}
static u8 practice_queen_can_lose_hp(u8 a,u8 b,u8 c,u8 d){(void)b;(void)c;(void)d;return a!=0;}
"""
tests=r"""
static void setup(u8 t) {
  u8 e=t^1;
  memset(ant,0,sizeof ant); memset(food,0,sizeof food);
  memset(ph,0,sizeof ph); memset(hgt,1,sizeof hgt);
  memset(cht,0,sizeof cht); memset(stock,0,sizeof stock);
  nestx[t]=10; nesty[t]=10; nestx[e]=2; nesty[e]=10;
  for(unsigned side=0;side<2;++side)
    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x)
      maps[side][y][x]=dist(x,nestx[side])+dist(y,nesty[side]);
  qhp[0]=qhp[1]=5; ncnt[0]=ncnt[1]=3;
  ant[0]=(Ant){10,10,t,1,0,1,10,10,8,0};
  ant[1]=(Ant){11,10,e,1,0,0,11,10,8,0};
  food[10][9]=1;
  refresh_defense(); assert(defend_i[t]==1);
  rng_calls=0;
}
static void check(u8 t) {
  setup(t); step_ant(&ant[0]); assert(ant[0].x==11 && rng_calls==4);
  setup(t); ant[0].sol=0; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  setup(t); ant[1].alive=0; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  setup(t); ant[1].team=t; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  setup(t); ant[1].x=16; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  setup(t); hgt[10][11]=0; refresh_defense(); assert(defend_i[t]==MAXA);
  setup(t); maps[t][10][11]=NOPATH; refresh_defense(); assert(defend_i[t]==MAXA);
  setup(t); ant[0].carry=1; ant[0].x=9; step_ant(&ant[0]);
  assert(ant[0].x==10 && !ant[0].carry && stock[t]==1 && rng_calls==4);
  setup(t); ant[0].x=20; ncnt[t]=12; step_ant(&ant[0]); assert(ant[0].x==19 && rng_calls==4);
  setup(t); qhp[t]=0; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  setup(t); ant[2]=ant[1]; refresh_defense(); assert(defend_i[t]==1);
}
static void route_test(u8 t) {
  // The only reachable neighbor is east, even though west looks
  // geometrically closer. Test beyond the former 2000-step clamp too.
  for (unsigned k=0;k<2;++k) {
    setup(t);
    ant[0].x=20; ant[0].carry=1;
    maps[t][10][21]=(u16)(k ? 5000 : 250);
    maps[t][10][19]=maps[t][9][20]=maps[t][11][20]=NOPATH;
    step_ant(&ant[0]); assert(ant[0].x==21 && rng_calls==4);
    setup(t);
    ant[0].x=20; ncnt[t]=12;
    maps[t^1][10][21]=(u16)(k ? 5000 : 250);
    maps[t^1][10][19]=maps[t^1][9][20]=maps[t^1][11][20]=NOPATH;
    step_ant(&ant[0]); assert(ant[0].x==21 && rng_calls==4);
  }
  // A completely stale/disconnected field still permits legal fallback.
  setup(t); ant[0].x=20; ant[0].carry=1;
  maps[t][10][21]=maps[t][10][19]=maps[t][9][20]=maps[t][11][20]=NOPATH;
  step_ant(&ant[0]); assert(ant[0].x==19 && rng_calls==4);
  setup(t); ant[0].x=20; ncnt[t]=12;
  maps[t^1][10][21]=maps[t^1][10][19]=maps[t^1][9][20]=maps[t^1][11][20]=NOPATH;
  step_ant(&ant[0]); assert(ant[0].x==19 && rng_calls==4);
  // Two valid routes beyond the old clamp must retain their gradient.
  // East is enumerated first; the shorter western route must still win.
  setup(t); ant[0].x=20; ant[0].carry=1;
  maps[t][10][21]=5001; maps[t][10][19]=5000;
  maps[t][9][20]=maps[t][11][20]=NOPATH;
  step_ant(&ant[0]); assert(ant[0].x==19 && rng_calls==4);
  setup(t); ant[0].x=20; ncnt[t]=12;
  maps[t^1][10][21]=5001; maps[t^1][10][19]=5000;
  maps[t^1][9][20]=maps[t^1][11][20]=NOPATH;
  step_ant(&ant[0]); assert(ant[0].x==19 && rng_calls==4);
  assert(colony_nest_score(NOPATH,190,1,0)>-1100000L);
  assert(colony_nest_score(NOPATH,0,1,31)<colony_nest_score(9215,0,1,0));
}
int main(void) {
  check(0); check(1);
  route_test(0); route_test(1);
  setup(1); cht[8]=1; step_ant(&ant[0]); assert(ant[0].x==9 && rng_calls==4);
  puts("GBA Classic defense: both colonies, stale/unreachable targets, carriers, peaceful reds, offensive march, long reachable routes and stale-map fallback PASS");
  return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="classic-defense-") as d:
    p=Path(d)/"test.c"; exe=Path(d)/"test"
    p.write_text(fixture+"\n"+function("refresh_defense")+"\n"+function("step_ant")+"\n"+tests)
    cc=shlex.split(os.environ.get("HOSTCC","cc"))
    subprocess.run(cc+["-std=c99","-Wall","-Wextra","-Werror","-pedantic","-I"+str(root/"src"),str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
