#!/usr/bin/env python3
"""Rebuild only the GBA 2D/3D ant assets from source, with no Pillow.
All sprites must agree byte-for-byte with the shipped 4bpp cartridge
atlas, including front/back, two strides, carrying and nest masonry."""
from pathlib import Path
import re
source=Path("tools/art2.py").read_text()
assert "def eye_polish_art()" in source
scene=source.split("# ---------------- sprites ----------------",1)[1].split(
    "# ---------------- ANT EYE panorama:",1)[0]
def parse(rows):
    return [[int(c,16) if c!="." else 0 for c in row] for row in rows]
ns={"parse":parse}
exec(compile(scene,"art2.py:ANT EYE sprites","exec"),ns)
atlas=Path("src/art2.h").read_text()
def check(name,parts,word_count):
    match=re.search(r"static const u32 "+name+r"\["+str(word_count)+r"\]\s*=\s*\{(.*?)\};",
                    atlas,re.S)
    assert match,name
    actual=[int(v,16) for v in re.findall(r"0x([0-9A-Fa-f]{8})",match.group(1))]
    expected=[sum(tile[y][x]<<(4*x) for x in range(8))
              for tile in parts for y in range(8)]
    assert len(expected)==word_count and actual==expected,(name,len(expected),len(actual))
    for tile in parts:
        assert len(tile)==8 and all(len(row)==8 and all(0<=p<16 for p in row) for row in tile)
    return parts
big=check("BIGS",ns["BIG"],512)
eye=check("EYE2",ns["EYE2"],1088)
def pixel(tiles,base,x,y):
    # 32x32 creatures use four 8x8 tiles across each row.
    assert 0 <= x < 32 and 0 <= y < 32
    return tiles[base+(y//8)*4+x//8][y%8][x%8]

def pixel16(tiles,base,x,y):
    # Six 16x16 decorations are packed as four tiles apiece, two
    # tiles per row. Indexing them as 32x32 samples a different prop.
    assert 0 <= x < 16 and 0 <= y < 16
    return tiles[base+(y//8)*2+x//8][y%8][x%8]
for base in (0,16,32):
    assert pixel(big,base,9,18)==3,("shell glint",base)
    assert pixel(big,base,11,17)==4,("thorax articulation",base)
for base in (0,16):
    assert pixel(eye,base,9,18)==3 and pixel(eye,base,11,17)==4
assert pixel(eye,32,14,10)==3 and pixel(eye,32,14,17)==4
assert pixel(eye,48,15,15)==3 and pixel(eye,48,16,18)==4
assert pixel(eye,80,12,16)==7 and pixel(eye,80,7,23)==4
# Honest silhouettes: front/back are different, carrying fruit visible,
# ground shadow translucent-like dither, queen crown and nest tunnel open.
assert eye[:16]!=eye[16:32]
assert eye[32:48]!=eye[48:64]
assert pixel(big,16,29,20)==5
assert pixel(big,32,23,9)==5
assert pixel(eye,80,15,19)==8
assert all(pixel(eye,64,x,29) in (0,1) for x in range(32))
# The last row of each 16x16 decor sprite must include opaque soil
# contact pixels, and the existing 32x32 ant shadow must have a denser
# central weight to remain visible when hardware scaled for distance.
footprints=ns["FOOT_SHADOWS"]
assert len(footprints) == 6
for kind, xs in enumerate(footprints):
    base = 96 + kind * 4
    assert len(xs) > 0 and len(xs) == len(set(xs))
    for x in xs:
        assert pixel16(eye,base,x,15)==1, ("ground contact", kind, x)
    assert all(pixel16(eye,base,x,15)==0 for x in range(16)
               if x not in xs), ("shadow spread", kind)
    # Contact pixels add no sprite tiles, and every footprint is
    # actually attached to its own decoration, not its neighbor.
    assert any(pixel16(eye,base,x,14) for x in range(16)), ("visible prop", kind)
assert pixel(eye,64,15,29)==1
assert pixel(eye,64,16,29)==1
world=Path("src/main.c").read_text()
assert "eye_decor_ground_center((s16)yg, (u16)SCB[m])" in world
assert "eye_decor_visible((u8)(dist(x, vx) + dist(y, vy))" in world
print("GBA ANT EYE art PASS: 200 unchanged-size 4bpp tiles rebuilt, segmented ants and living nest")
print("GBA native-resolution ground-contact audit PASS: six decor shadows and grounded ant silhouettes")

# Compile the production affine selector and compare to the old linear
# scan. Every projected scale in the game is covered, including exact
# midpoint ties and a wide margin outside the camera's scale range.
import os
import shlex
import subprocess
import tempfile
scale_table = re.search(r"static const s16 SCB\[16\]\s*=\s*\{[^}]+\};", world).group(0)
selector = re.search(r"static u8 scb_of\(s32 sc\) \{.*?\n\}", world, re.S).group(0)
program = r"""
#include <stdint.h>
#include <assert.h>
typedef uint8_t u8;
typedef int16_t s16;
typedef int32_t s32;
""" + scale_table + "\n" + selector + r"""
static u8 reference(s32 sc) {
  u8 i;
  for (i=15; i && SCB[i]>sc; --i) {}
  if (i<15 && sc-SCB[i]>SCB[i+1]-sc) ++i;
  return i;
}
int main(void) {
  for (s32 scale=-8192; scale<=8192; ++scale)
    assert(scb_of(scale)==reference(scale));
  return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "scale.c"
    exe = Path(tmp) / "scale"
    src.write_text(program)
    subprocess.run(shlex.split(os.environ.get("HOSTCC", "cc")) +
                   ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
assert "eye_ant_stride(a->age, (u8)(a->x != a->ox || a->y != a->oy))" in world
print("GBA render scale: 16385 equivalent input scales; overhead gait follows real movement")
