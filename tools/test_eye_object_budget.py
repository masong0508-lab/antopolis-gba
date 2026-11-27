#!/usr/bin/env python3
"""Check the shipping ANT EYE projection pool under dense object loads."""
from pathlib import Path
import re
import os
import shlex
import subprocess
import tempfile

source=(Path(__file__).resolve().parents[1]/"src/main.c").read_text()
obj=re.search(r"static void eo_add\(s32 rx,.*?\n\}",source,re.S)
assert obj
fixture=r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef int16_t s16; typedef int32_t s32;
#define EOMAX 80
typedef struct {s32 rx,ry;s16 d,sd;u8 kind,a,b,c;} EObj;
static EObj eo[EOMAX];static u8 eon,eord[EOMAX];
"""
tests=r"""
static void check_order(void){
  u8 seen[EOMAX]={0};
  assert(eon<=EOMAX);
  for(unsigned j=0;j<eon;j++){
    assert(eord[j]<eon && !seen[eord[j]]);seen[eord[j]]=1;
    if(j)assert(eo[eord[j-1]].d<=eo[eord[j]].d);
  }
}
static void add(int d,u8 kind,u8 tag){eo_add(d,0,kind,tag,0,0,0,256,0);check_order();}
static unsigned count(u8 kind){
  unsigned n=0;for(unsigned i=0;i<eon;i++)n+=eo[i].kind==kind;return n;
}
static int exists(u8 kind,int d){
  for(unsigned i=0;i<eon;i++)if(eo[i].kind==kind&&eo[i].d==d)return 1;
  return 0;
}
int main(void){
  // Actual submission order: ants, queens, nest landmarks, food, decor.
  for(unsigned i=0;i<48;i++)add(300+(int)i*10,0,(u8)i);
  add(350,2,0);add(350,2,1);add(350,3,0);add(350,3,1);
  for(unsigned i=0;i<28;i++)add(1000+(int)i*10,4,0);
  assert(eon==80&&count(4)==28);
  for(unsigned i=0;i<100;i++)add(500+(int)i,4,0);
  for(unsigned i=0;i<28;i++)assert(exists(4,500+(int)i));
  assert(count(0)==48&&count(2)==2&&count(3)==2&&count(4)==28);
  for(unsigned i=0;i<48;i++)assert(exists(0,300+(int)i*10));
  // Decorations never evict food, ants, queens or nests.
  for(unsigned i=0;i<100;i++)add(200+(int)i,5,0);
  assert(count(5)==0&&count(4)==28);
  // In a less crowded scene, scenery uses the same nearest-first rule.
  eon=0;add(300,0,0);add(400,4,0);
  for(unsigned i=0;i<78;i++)add(2000+(int)i,5,0);
  for(unsigned i=0;i<100;i++)add(800+(int)i,5,1);
  assert(count(5)==78&&exists(0,300)&&exists(4,400));
  for(unsigned i=0;i<78;i++)assert(exists(5,800+(int)i));
  EObj before[EOMAX];u8 order[EOMAX];memcpy(before,eo,sizeof eo);memcpy(order,eord,sizeof eord);
  add(877,5,2); // equal to farthest retained decor, preserve old tie
  eo_add(220,2000,5,3,0,0,0,256,0); // outside view
  add(199,5,3);add(4097,5,3); // invalid depth
  assert(!memcmp(before,eo,sizeof eo)&&!memcmp(order,eord,sizeof eord));
  // Mixed-depth insertion must retain a permutation of all slots.
  eon=0;for(unsigned i=0;i<80;i++)add(200+(int)((i*137u)%3800u),4,0);
  for(unsigned i=0;i<4000;i++)add(200+(int)((i*379u)%3800u),4,0);
  check_order();assert(eon==80);
  puts("ANT EYE object budget PASS: nearest food/scenery, stable ties, protected ants and nests");
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"pool.c"; exe=Path(tmp)/"pool"
    p.write_text(fixture+obj.group()+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
       ["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
