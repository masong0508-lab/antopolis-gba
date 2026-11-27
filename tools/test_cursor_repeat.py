#!/usr/bin/env python3
"""Exercise the exact production cursor repeat expressions across modes."""
from pathlib import Path
import re, os, shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
main=(root/"src/main.c").read_text()
table=re.search(r"static const u8 CDLY\[3\].*?;", main).group()
pairs=[]
for name in ("main","city","empire","mp"):
    source=(root/"src"/(name+".c")).read_text()
    pairs.extend(re.findall(r"fire = ([^;]+);\n\s*(if \(\+\+(?:s->)?rep == 250u\) (?:s->)?rep = 40u;)",source))
assert len(pairs)==5, len(pairs)
assert "p |= k & (J_UP | J_DOWN | J_LEFT | J_RIGHT);" in main
assert "if (rep < 13u) ++rep;" in main
assert "if (++rep == 252u) rep = 16u;" in main
for condition,advance in pairs:
    program=r"""
#include <assert.h>
#include <stdint.h>
typedef uint8_t u8;
static u8 speed;
#define O(x) speed
""" + table + r"""
int main(void){
  for(speed=0;speed<3;speed++){
    u8 rep=0,fire;
    for(unsigned frame=0;frame<20000;frame++){
      fire="""+condition.replace("s->","")+r""";
      """+advance.replace("s->","")+r"""
      assert(fire==(frame==0||(frame>=CDLY[speed]&&(frame-CDLY[speed])%CRAT[speed]==0)));
    }
    rep=0; fire="""+condition.replace("s->","")+r"""; assert(fire);
  }
  return 0;
}
"""
    with tempfile.TemporaryDirectory() as tmp:
        p=Path(tmp)/"repeat.c";exe=Path(tmp)/"repeat"
        p.write_text(program)
        subprocess.run(shlex.split(os.environ.get("HOSTCC","cc"))+
          ["-std=c99","-Wall","-Wextra","-Werror","-pedantic",str(p),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print("All five live map cursor loops: 300000 polls, three speeds, no hold reset or acceleration")
