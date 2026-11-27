#!/usr/bin/env python3
"""Compile actual emergency navigation and check city menu integration."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

source=(Path(__file__).resolve().parents[1]/"src/city.c").read_text()
fn=re.search(r"static u8 city_active_hazard\(u8 \*cxp, u8 \*cyp\) \{.*?\n\}",source,re.S)
assert fn
assert "city_active_hazard(&cx, &cy)" in source
assert 'ps(16, y, "ACTIVE", 8); pn(23, y, cfires_n, 8);' in source
assert 'ps(16, y, "HP", 8); pn(20, y, bt_hp, 8);' in source
fixture=r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16;
#define W 96
#define H 96
#define NFIRE 20
#define NFLOOD 40
static u8 ftime[NFIRE],fltime,hgt[H][W],fuel[H][W];
static u16 fcell[NFIRE],flcell[NFLOOD];
static u8 dist(u8 a,u8 b){return a>b?a-b:b-a;}
static u8 burnable(u8 x,u8 y){return fuel[y][x];}
"""
tests=r"""
static void reset(void){
  memset(ftime,0,sizeof ftime);memset(flcell,255,sizeof flcell);
  memset(hgt,1,sizeof hgt);memset(fuel,1,sizeof fuel);fltime=0;
}
static void fire(unsigned i,u8 x,u8 y){ftime[i]=1;fcell[i]=(u16)((y<<7)|x);}
static void flood(unsigned i,u8 x,u8 y){flcell[i]=(u16)((y<<7)|x);fltime=1;}
int main(void){
  u8 x=95,y=95; reset();
  assert(!city_active_hazard(&x,&y)&&x==95&&y==95);
  fire(0,0,0);fire(1,95,94);fire(2,94,95);flood(0,95,95);
  assert(city_active_hazard(&x,&y)==1&&x==95&&y==94);
  hgt[94][95]=0; fuel[95][94]=0; x=y=95;
  assert(city_active_hazard(&x,&y)==1&&x==0&&y==0);
  // Fire priority, invalid packed addresses, dead slots and flood ties.
  ftime[0]=0; fire(3,127,127); ftime[4]=1;fcell[4]=0xffff;
  x=y=95;assert(city_active_hazard(&x,&y)==2&&x==95&&y==95);
  flcell[0]=0xffff;flood(1,95,94);flood(2,94,95);
  assert(city_active_hazard(&x,&y)==2&&x==95&&y==94);
  fltime=0; x=y=48;assert(!city_active_hazard(&x,&y)&&x==48&&y==48);
  // Compare closest selection over all map positions, both categories.
  reset();fire(0,0,0);fire(1,95,95);
  for(unsigned yy=0;yy<H;yy++)for(unsigned xx=0;xx<W;xx++){
    x=(u8)xx;y=(u8)yy;unsigned far_corner=xx+yy>95;
    assert(city_active_hazard(&x,&y)==1&&x==(far_corner?95:0)&&y==x);
  }
  memset(ftime,0,sizeof ftime);flood(0,0,0);flood(1,95,95);
  for(unsigned yy=0;yy<H;yy++)for(unsigned xx=0;xx<W;xx++){
    x=(u8)xx;y=(u8)yy;unsigned far_corner=xx+yy>95;
    assert(city_active_hazard(&x,&y)==2&&x==(far_corner?95:0)&&y==x);
  }
  puts("City emergency navigation: 18432 cursor cases plus priority, stale/invalid slots and ties PASS");
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/"emergency.c";exe=Path(tmp)/"emergency"
    src.write_text(fixture+fn.group()+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
       ["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
