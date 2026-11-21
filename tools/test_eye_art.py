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
    return tiles[base+(y//8)*4+x//8][y%8][x%8]
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
print("GBA ANT EYE art PASS: 200 unchanged-size 4bpp tiles rebuilt, segmented ants and living nest")
