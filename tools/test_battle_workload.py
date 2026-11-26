#!/usr/bin/env python3
"""Compile the production Empire casualty resolver, compare old semantics,
and guard the live battle simulation workload threshold."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/empire.c").read_text()
battle = (ROOT / "src/battle.c").read_text()

def function(source, name):
    match = re.search(r"static void " + re.escape(name) + r"\([^)]*\)\s*\{", source)
    assert match, name
    pos, depth = match.end(), 1
    while depth:
        assert pos < len(source)
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[match.start():pos]

# The visible battle must recover normal step cadence as the compacted
# population shrinks, not remain throttled by the original spawn count.
assert "if (bn[0] + bn[1] <= 600u) { bstep(par); par ^= 1; }" in battle
assert "if (bcount > 600)" not in battle
assert "bstep(par); par ^= 1;" in battle
assert "if (!(fr & 7)) { bcompact(); bhud(bbg); }" in battle

FIXTURE = r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t u16;
"""
TESTS = r"""
static void old_loss(u16 *w, u16 *s, u16 *m, u16 n) {
  while(n) {
    if(*w) (*w)--;
    else if(*s) (*s)--;
    else if(*m) (*m)--;
    else break;
    n--;
  }
}
static void compare(u16 w,u16 s,u16 m,u16 n) {
  u16 a=w,b=s,c=m;
  old_loss(&a,&b,&c,n);
  loss(&w,&s,&m,n);
  assert(a==w && b==s && c==m);
}
int main(void) {
  unsigned w,s,m,n;
  uint32_t seed=0x207391u;
  for(w=0;w<=8;w++) for(s=0;s<=8;s++)
    for(m=0;m<=8;m++) for(n=0;n<=32;n++)
      compare((u16)w,(u16)s,(u16)m,(u16)n);
  for(unsigned i=0;i<10000;i++) {
    u16 p[4];
    for(unsigned k=0;k<4;k++) {
      seed=seed*1664525u+1013904223u;
      p[k]=(u16)((seed>>16)%1501u);
    }
    compare(p[0],p[1],p[2],p[3]);
  }
  compare(65535u,65535u,65535u,65535u);
  compare(1u,1u,1u,65535u);
  compare(0u,0u,65535u,65535u);
  compare(0u,0u,0u,65535u);
  compare(500u,250u,200u,0u);
  puts("GBA casualty resolution: 24057 exhaustive small cases, 10000 large randomized cases and maximum damage PASS");
  return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="battle-loss-") as td:
    path = Path(td) / "loss.c"
    exe = Path(td) / "loss"
    path.write_text(FIXTURE + "\n" + function(source, "loss") + "\n" + TESTS)
    cc = shlex.split(os.environ.get("HOSTCC", "cc"))
    subprocess.run(cc + ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                         "-pedantic", str(path), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("GBA battle cadence: compacted surviving count controls half/full simulation passes")
