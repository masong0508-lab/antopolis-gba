#!/usr/bin/env python3
"""Compile the shipping city block calculation and compare the previous implementation.
Verify map edges, saturation, every law combination, political modifiers and coverage.
"""
from pathlib import Path
import os, re, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "src/city.c").read_text()
def grab(pattern):
    m = re.search(pattern, source, re.S)
    assert m, pattern
    return m.group() + "\n"
fixture = r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16;
#define CBW 24
enum { B_AIR,B_GUARD,B_CLINIC,B_SCHOOL,B_PUMP,B_PARK,B_GARDEN,B_TEMPLE,B_STATUE,B_DANCE,B_STADIUM,B_ARCO,B_WONDER,B_PALACE,B_TOWER,B_HEAL,B_ACAD,B_ARCH };
enum { CV_AIR=1, CV_GUARD=2, CV_CLINIC=4, CV_SCHOOL=8, CV_PUMP=16 };
static u8 clv[CBW][CBW], cpol[CBW][CBW], ccri[CBW][CBW],
ccov[CBW][CBW], cpk[CBW][CBW], cwat[CBW][CBW], csrc[CBW][CBW], cden[CBW][CBW];
typedef struct {u8 t,x,y;} EB;
static EB ebl[24]; static u8 nbl, nestx[2], nesty[2], ecity, csmog, ccrime, claws, mart;
static struct {u8 on; s16 m_smog,m_crime,m_lv;} PZ_;
static unsigned law_calls, mart_calls;
static u8 law(u8 i){++law_calls;return (u8)((claws>>i)&1);}
static u8 pz_mart(void){++mart_calls;return mart;}
static u32 rng=0x92015831u;
static u32 next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
"""
reference = r"""
static void previous_blocks_calc(void) {                    // land value, smog, crime and coverage for every block
  u8 a, b, i; s16 v; s8 dx, dy; u16 nb = 0; u32 sp = 0, sc = 0;
  for (b = 0; b < CBW; b++) for (a = 0; a < CBW; a++) { ccov[b][a] = 0; cpk[b][a] = 0; }
  cover(nestx[0], nesty[0], 6, CV_AIR | CV_GUARD);
  for (i = 0; i < nbl; i++) {
    EB *e = &ebl[i];
    switch (e->t) {
    case B_AIR: cover(e->x, e->y, 10, CV_AIR); break;
    case B_GUARD: cover(e->x, e->y, 9, CV_GUARD); break;
    case B_CLINIC: cover(e->x, e->y, 9, CV_CLINIC); break;
    case B_SCHOOL: cover(e->x, e->y, 11, CV_SCHOOL); break;
    case B_PUMP: cover(e->x, e->y, 9, CV_PUMP); break;
    case B_PARK: case B_GARDEN: cover(e->x, e->y, 5, 0); break;
    case B_TEMPLE: case B_STATUE: case B_DANCE: cover(e->x, e->y, 6, 0); break;
    case B_STADIUM: case B_ARCO: case B_WONDER: case B_PALACE: cover(e->x, e->y, 12, 0); cover(e->x, e->y, 8, 0); break;
    case B_TOWER: if (ecity == 2) cover(e->x, e->y, 5, CV_GUARD); break;
    case B_HEAL: if (ecity == 2) cover(e->x, e->y, 6, CV_CLINIC); break;
    case B_ACAD: case B_ARCH: if (ecity == 2) cover(e->x, e->y, 7, CV_SCHOOL); break;
    }
  }
  for (b = 0; b < CBW; b++) for (a = 0; a < CBW; a++) {
    u16 s = (u16)csrc[b][a] * 3;
    for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) if ((dx || dy) && a + dx >= 0 && b + dy >= 0 && a + dx < CBW && b + dy < CBW) s += csrc[b + dy][a + dx];
    v = (s16)(s / 3) - cpk[b][a] * 4;
    if (law(0)) v = (s16)(v * 7 / 10);
    if (law(3)) v = (s16)(v * 11 / 10);
    if (PZ_.on) v += PZ_.m_smog;
    cpol[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    v = (s16)(cden[b][a] * 3 + cpol[b][a] / 4 - ((ccov[b][a] & CV_GUARD) ? 55 : 0) - (law(1) ? 20 : 0));
    if (PZ_.on) { v += PZ_.m_crime; if (pz_mart()) v -= 30; }
    ccri[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    v = (s16)(40 + cwat[b][a] * 12 + (cpk[b][a] > 4 ? 4 : cpk[b][a]) * 8 + ((ccov[b][a] & CV_GUARD) ? 6 : 0) + ((ccov[b][a] & CV_CLINIC) ? 6 : 0)
      + ((ccov[b][a] & CV_SCHOOL) ? 8 : 0) + ((ccov[b][a] & CV_PUMP) ? 4 : 0) + (law(2) ? 6 : 0) - cpol[b][a] * 6 / 10 - ccri[b][a] * 4 / 10);
    if (PZ_.on) v += PZ_.m_lv;
    clv[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    if (cden[b][a] || csrc[b][a]) { nb++; sp += cpol[b][a]; sc += ccri[b][a]; }
  }
  csmog = (u8)(nb ? sp / nb : 0); ccrime = (u8)(nb ? sc / nb : 0);
}
"""
tests = r"""
int main(void) {
  u8 expected[5][CBW][CBW], es, ec;
  unsigned cases=0;
  for(unsigned laws=0;laws<16;laws++) for(unsigned mode=0;mode<2;mode++)
    for(unsigned policy=0;policy<3;policy++) for(unsigned pattern=0;pattern<12;pattern++) {
      claws=(u8)laws; ecity=(u8)(mode+1); PZ_.on=(u8)(policy!=0); mart=(u8)(policy==2);
      PZ_.m_smog=(s16)((int)(next()%101)-50);
      PZ_.m_crime=(s16)((int)(next()%101)-50);
      PZ_.m_lv=(s16)((int)(next()%101)-50);
      nestx[0]=(u8)(next()%96); nesty[0]=(u8)(next()%96);
      nbl=(u8)(pattern==0?0:next()%25);
      for(unsigned i=0;i<nbl;i++) {ebl[i].t=(u8)(next()%(18u));ebl[i].x=(u8)(next()%96);ebl[i].y=(u8)(next()%96);}
      for(unsigned y=0;y<CBW;y++) for(unsigned x=0;x<CBW;x++) {
        csrc[y][x]=(u8)(pattern==0?0:pattern==1?250:pattern==2?(x==0&&y==0?250:0):next()%251);
        cden[y][x]=(u8)(pattern==0?0:pattern==1?250:next()%251);
        cwat[y][x]=(u8)(next()&1);
      }
      previous_blocks_calc();
      memcpy(expected[0],clv,sizeof clv);memcpy(expected[1],cpol,sizeof cpol);
      memcpy(expected[2],ccri,sizeof ccri);memcpy(expected[3],ccov,sizeof ccov);
      memcpy(expected[4],cpk,sizeof cpk);es=csmog;ec=ccrime;
      memset(clv,0xFF,sizeof clv);memset(cpol,0xFF,sizeof cpol);
      memset(ccri,0xFF,sizeof ccri);memset(ccov,0xFF,sizeof ccov);memset(cpk,0xFF,sizeof cpk);
      law_calls=mart_calls=0;blocks_calc();
      assert(!memcmp(expected[0],clv,sizeof clv));assert(!memcmp(expected[1],cpol,sizeof cpol));
      assert(!memcmp(expected[2],ccri,sizeof ccri));assert(!memcmp(expected[3],ccov,sizeof ccov));
      assert(!memcmp(expected[4],cpk,sizeof cpk));assert(es==csmog&&ec==ccrime);
      assert(law_calls<=4 && mart_calls<=1);++cases;
    }
  printf("City block optimization: %u complete 24x24 city comparisons, policy lookup budget PASS\n",cases);
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"blocks.c"; exe=Path(tmp)/"blocks"
    p.write_text(fixture+grab(r"static void cover\([^)]*\) \{.*?\n\}")+reference+
        grab(r"static u16 city_source_column\([^)]*\) \{.*?\n\}")+
        grab(r"static void blocks_calc\([^)]*\) \{.*?\n\}")+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
        ["-std=c99","-O2","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
