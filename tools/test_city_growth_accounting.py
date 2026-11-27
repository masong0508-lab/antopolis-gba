#!/usr/bin/env python3
"""Compile the real city scan and cell counters with controlled growth."""
from pathlib import Path
import re
import os
import shlex
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / "src/city.c").read_text()
def extract(name):
    match = re.search(r"static void " + name + r"\([^)]*\) \{.*?\n\}", source, re.S)
    assert match, name
    return match.group()
fixture = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define W 8
#define H 8
#define CBW 2
static u8 ezt[H][W],hgt[H][W],cwat[CBW][CBW],csrc[CBW][CBW],cden[CBW][CBW];
#define EZN(y,x) ((ezt[y][x]>>3)&3)
#define ELV(y,x) ((ezt[y][x]>>5)&3)
#define ETUN(y,x) (ezt[y][x]>>7)
static u16 a_zn[4][4],a_tun,a_noacc,cmon,cscan;
static u32 a_pop,a_jw,a_jf,a_fp;
static u8 cchg, forced_level, accessible, night_shift, grows;
static u8 law(u8 i){return i==3 && night_shift;}
static u8 has_access(u8 x,u8 y){(void)x;(void)y;return accessible;}
static void grow_try(u8 x,u8 y){
  ++grows;
  ezt[y][x]=(u8)((ezt[y][x]&~0x60u)|(forced_level<<5));
}
"""
tests = r"""
int main(void){
  const u8 people[4]={0,8,24,64},works[4]={0,4,12,30};
  const u8 farmjobs[4]={0,3,8,16},farmfood[4]={0,2,5,10};
  const u8 pollution[4]={0,1,3,6};
  unsigned cases=0;
  for(unsigned zone=1;zone<=3;zone++)
    for(unsigned before=0;before<4;before++)
      for(unsigned after=0;after<4;after++)
        for(unsigned month=0;month<2;month++)
          for(unsigned access=0;access<2;access++)
            for(unsigned night=0;night<2;night++){
              memset(ezt,0,sizeof ezt); memset(hgt,1,sizeof hgt);
              scan_reset(); cmon=(u16)month; accessible=(u8)access;
              forced_level=(u8)after; night_shift=(u8)night; grows=0;
              ezt[3][3]=(u8)((zone<<3)|(before<<5));
              ezt[0][0]=128; hgt[7][7]=0;
              scan_rows(4); assert(cscan==4);
              scan_rows(4); assert(cscan==H);
              unsigned l=month?after:before;
              assert(grows==month);
              for(unsigned z=0;z<4;z++)for(unsigned k=0;k<4;k++)
                assert(a_zn[z][k]==(unsigned)(z==zone&&k==l));
              assert(a_pop==(zone==1?people[l]:0));
              assert(a_jw==(unsigned)(zone==3?works[l]+(night?works[l]/5:0):0));
              assert(a_jf==(zone==2?farmjobs[l]:0));
              assert(a_fp==(zone==2?farmfood[l]:0));
              assert(csrc[0][0]==(zone==3?pollution[l]:0));
              assert(cden[0][0]==(zone==1?l:0));
              assert(a_noacc==(unsigned)(l==0&&!access));
              assert(a_tun==1&&cwat[1][1]==1);
              ++cases;
            }
  printf("City growth accounting: %u production scan cases PASS\n",cases);
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/"city.c"
    exe=Path(tmp)/"city"
    src.write_text(fixture+extract("cell_info")+"\n"+extract("scan_rows")+"\n"+extract("scan_reset")+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
                   ["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
