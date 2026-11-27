#!/usr/bin/env python3
"""Compile the real city budget functions and test large-city income."""
from pathlib import Path
import re, os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "src/city.c").read_text()
palace = (root / "src/pz.c").read_text()
assert "static s32 inc_r, inc_f, inc_w, inc_x, exp_s, exp_t, exp_l, exp_n;" in source
assert "cash += city_net_income()" in source
assert "net = city_net_income();" in source
assert "s32 in = inc_r + inc_w + inc_f;" in palace
def extract(name):
    m = re.search(r"static (?:void|s32) " + name + r"\(void\) \{", source)
    assert m, name
    end, depth = m.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[m.start():end] + "\n"
fixture = r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
typedef uint8_t u8; typedef int8_t s8; typedef uint32_t u32;
typedef int16_t s16; typedef int32_t s32;
static u32 cpop, cjw, cjf;
static u8 taxR, taxW, taxF, cht[20], holiday, rich, green;
enum {E_TAXRICH, E_GREEN};
static struct {u8 on, con[5]; s8 m_inc;} PZ_;
static s32 inc_r, inc_w, inc_f, inc_x, exp_s, exp_t, exp_l, exp_n;
static u8 law(u8 i){return i==7 && holiday;}
static u8 pz_ed(u8 i){return i==E_TAXRICH?rich:green;}
"""
tests = r"""
static int64_t expected(u32 amount,u8 rate,int divisor,int m){
  int64_t v=(int64_t)amount*rate/divisor;
  if(holiday)v=v*8/10;
  v*=1+cht[18];
  if(PZ_.on)v=v*m/100;
  return v;
}
int main(void){
  const u32 pops[]={0,1,150,15000,99999};
  const u32 jobs[]={0,1,100,10000,331776};
  unsigned count=0,large=0;
  for(unsigned a=0;a<5;a++)for(unsigned j=0;j<5;j++)
    for(unsigned rate=0;rate<=20;rate++)
      for(unsigned h=0;h<2;h++)for(unsigned cheat=0;cheat<4;cheat++)
        for(unsigned p=0;p<2;p++)for(int mod=-128;mod<=127;mod+=17)
          for(unsigned flags=0;flags<8;flags++){
            cpop=pops[a];cjw=jobs[j];cjf=jobs[j]/2;
            taxR=taxW=taxF=(u8)rate;holiday=(u8)h;cht[18]=(u8)cheat;
            PZ_.on=(u8)p;PZ_.m_inc=(s8)mod;rich=(u8)(flags&1);
            green=(u8)((flags>>1)&1);PZ_.con[4]=(u8)((flags>>2)&1);
            int m=100+mod+(rich?10:0)+(PZ_.con[4]?8:0);
            if(m<20)m=20;
            city_tax_month();
            assert(inc_r==expected(cpop,taxR,150,m));
            assert(inc_f==expected(cjf,taxF,120,m));
            assert(inc_w==expected(cjw,taxW,100,m-(green?10:0)));
            assert(inc_r>=0&&inc_f>=0&&inc_w>=0);
            inc_x=599;exp_s=2000;exp_t=921;exp_l=65;exp_n=110;
            assert(city_income()==inc_r+inc_f+inc_w+599);
            assert(city_expenses()==3096);
            assert(city_net_income()==city_income()-3096);
            if(city_income()>32767)++large;
            ++count;
          }
  assert(large>0);
  printf("Wide city budget: %u cases, %u income totals above signed 16-bit PASS\n",count,large);
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/"budget.c"; exe=Path(tmp)/"budget"
    src.write_text(fixture+"".join(extract(n) for n in ("city_tax_month","city_income","city_expenses","city_net_income"))+tests)
    subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
       ["-std=c99","-Wall","-Wextra","-Werror","-pedantic","-O2",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
