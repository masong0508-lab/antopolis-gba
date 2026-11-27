#!/usr/bin/env python3
"""Exercise production disaster functions without changing emulator memory."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

source=(Path(__file__).resolve().parents[1]/"src/city.c").read_text()
def extract(name):
    m=re.search(r"static (?:u8|void) "+name+r"\([^\n]*?\) \{",source)
    assert m,name
    depth=1; end=m.end()
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[m.start():end]+"\n"
fixture=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef int8_t s8; typedef uint16_t u16;
#define W 32
#define H 32
#define NFIRE 20
#define B_NEST 1
#define CV_PUMP 16
#define FX_PUFF 2
static u8 hgt[H][W],ezt[H][W],ebi[H][W],ccov[8][8],ftime[NFIRE],cht[20];
static u16 fcell[NFIRE],alt;
static u8 alx,aly,cfires_n,c_fout;
typedef struct {u8 t;} EB;
static EB ebl[1];
static const s8 DX[4]={1,-1,0,0},DY[4]={0,0,1,-1};
#define ELV(y,x) ((ezt[y][x]>>5)&3)
static void set_lv(u8 y,u8 x,u8 l){ezt[y][x]=(u8)((ezt[y][x]&~0x60)|(l<<5));}
static unsigned calls, alarms, redraws, saves, rng_pos, rng_len, deletions;
static u8 rolls[8];
static unsigned rnd(void){return rng_pos<rng_len?rolls[rng_pos++]:1u;}
static u8 law(u8 i){(void)i;return 0;}
static void emp_dec(u8 x,u8 y){(void)x;(void)y;}
static void say(const char *s){(void)s;}
static void sfx_alarm(void){++alarms;}
static void ach_get(u8 n){assert(n==41);++saves;}
static void del_b(u8 i){assert(i==0);++deletions;memset(ebi,0,sizeof ebi);}
static void terr_calc(void){}
static void eredraw(u8 x,u8 y,u8 radius){(void)x;(void)y;(void)radius;++redraws;}
static void fx_add(u8 x,u8 y,u8 k){(void)x;(void)y;assert(k==FX_PUFF);++calls;}
"""
tests=r"""
static void reset(void){
  memset(hgt,1,sizeof hgt);memset(ezt,0,sizeof ezt);memset(ebi,0,sizeof ebi);
  memset(ccov,0,sizeof ccov);memset(ftime,0,sizeof ftime);memset(cht,0,sizeof cht);
  memset(fcell,0,sizeof fcell);cfires_n=c_fout=alx=aly=0;alt=0;
  calls=alarms=redraws=saves=rng_pos=rng_len=deletions=0;
}
static void light(u8 i,u8 x,u8 y,u8 time){
  set_lv(y,x,1);fcell[i]=(u16)((y<<7)|x);ftime[i]=time;++cfires_n;
}
static void invariant(void){
  unsigned n=0;
  for(unsigned i=0;i<NFIRE;i++)if(ftime[i]){
    ++n;
    for(unsigned j=i+1;j<NFIRE;j++)assert(!ftime[j]||fcell[i]!=fcell[j]);
  }
  assert(n==cfires_n);
}
int main(void){
  reset(); light(0,10,10,1);set_lv(10,11,1);
  rolls[0]=0;rolls[1]=0;rolls[2]=0;rng_len=3;
  city_fires_tick();
  assert(ftime[0]==0&&ELV(10,10)==0&&redraws==1&&calls==1);
  assert(ftime[1]==39&&fcell[1]==((10<<7)|11));invariant();
  // A full fire array cannot overwrite an expiring fire.
  reset();for(u8 i=0;i<NFIRE;i++)light(i,2,i,(u8)(i?8:1));
  set_lv(0,3,1);rolls[0]=0;rolls[1]=0;rng_len=2;
  city_fires_tick();assert(!ftime[0]&&ELV(0,2)==0&&cfires_n==19);invariant();
  // Water, cleared fuel and pumps extinguish their actual slots.
  reset();light(0,3,3,5);hgt[3][3]=0;light(1,7,7,5);set_lv(7,7,0);
  light(2,12,12,6);ccov[3][3]=CV_PUMP;c_fout=9;
  city_fires_tick();assert(!cfires_n&&ELV(12,12)==1&&saves==1&&calls==0);invariant();
  // Real building destruction is distinct from a saved zone.
  reset();light(0,2,2,1);ebi[2][2]=1;ebl[0].t=2;
  city_fires_tick();assert(deletions==1&&!cfires_n&&calls==1);invariant();
  // Duplicate, water, cheat and full-table starts cannot create fake alerts.
  reset();set_lv(1,1,1);assert(fire_start(1,1));assert(!fire_start(1,1));
  fire_any();assert(!alarms&&cfires_n==1);invariant();
  reset();set_lv(1,1,1);cht[19]=1;fire_any();assert(!alarms&&!cfires_n);
  cht[19]=0;hgt[1][1]=0;fire_any();assert(!alarms&&!cfires_n);
  hgt[1][1]=1;fire_any();assert(alarms==1&&alt==240&&alx==1&&aly==1);invariant();
  reset();for(u8 i=0;i<NFIRE;i++)light(i,2,i,8);
  fire_any();assert(!alarms&&!fire_start(1,1));invariant();
  puts("City fire lifecycle: expiry, final-tick spread, full table, pump, water, fuel and alerts PASS");
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"fires.c"; exe=Path(tmp)/"fires"
    p.write_text(fixture+"".join(extract(n) for n in ("in_fire","fire_start","burnable","fire_any","city_fires_tick"))+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
      ["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
