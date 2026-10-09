# Generates src/art2.h: terrain auto-tiles, decor tiles (food, trails, nest mound), sprite tiles and palettes (GBA 4bpp).
import sys
from PIL import Image

def rgb(r, g, b): return (r, g, b)
# --- palettes (5-bit values) ---
TERR_PAL = [rgb(0,0,0), rgb(3,8,20), rgb(6,13,26), rgb(14,22,31), rgb(28,24,14), rgb(23,19,10), rgb(9,20,7), rgb(6,14,5),
            rgb(18,13,8), rgb(24,19,13), rgb(26,29,31), rgb(3,2,2), rgb(31,30,22), rgb(11,8,5), rgb(10,18,30), rgb(13,25,9)]
DEC_PAL = [rgb(0,0,0), rgb(3,2,1), rgb(31,25,6), rgb(31,31,18), rgb(5,20,4), rgb(29,5,8), rgb(15,9,4), rgb(22,15,8),
           rgb(1,1,1), rgb(31,24,12), rgb(10,6,3), rgb(26,19,11), rgb(0,0,0), rgb(0,0,0), rgb(0,0,0), rgb(0,0,0)]
ANT_PAL = [  # bank 0 black ants, bank 1 red ants: 1 outline 2 body 3 shine 4 accent 5 gold 6 wing 7 green
  [rgb(0,0,0), rgb(1,1,2), rgb(6,6,8), rgb(16,16,19), rgb(14,15,22), rgb(31,26,6), rgb(29,30,31), rgb(8,22,6)] + [rgb(0,0,0)]*8,
  [rgb(0,0,0), rgb(11,1,1), rgb(27,5,3), rgb(31,17,12), rgb(31,22,6), rgb(31,26,6), rgb(31,28,26), rgb(8,22,6)] + [rgb(0,0,0)]*8]
FX_PAL = [rgb(0,0,0), rgb(2,2,2), rgb(31,26,4), rgb(31,31,31), rgb(10,18,31), rgb(20,20,20), rgb(31,4,6)] + [rgb(0,0,0)]*9

# ---------------- terrain ----------------
def hsh(x, y, l): return ((x * 37 + y * 91 + l * 53) ^ (x * y * 13) ^ (l * 7)) & 15
def base(l, x, y):
    h = hsh(x, y, l)
    if l == 0:
        if y % 4 == 1 and (x + y * 2) % 8 < 3: return 3
        if y % 4 == 3 and (x + y * 2 + 4) % 8 < 3: return 14
        return 1 if h < 3 else 2
    if l == 1: return 5 if h < 3 else 4
    if l == 2: return 7 if h < 3 else 15 if h == 5 else 6
    return 9 if (h < 3 or (x + y) % 7 == 0 and h < 8) else 13 if h == 9 else 8
def terr_tile(l, code):
    n, e, s, w = code % 3, code // 3 % 3, code // 9 % 3, code // 27
    p = [[base(l, x, y) for x in range(8)] for y in range(8)]
    if l == 0:
        for i in range(8):
            if n: p[0][i] = 10
            if s: p[7][i] = 10
            if w: p[i][0] = 10
            if e: p[i][7] = 10
        return p
    for i in range(8):   # N and W first (lit side), then S and E (shadow side) on top
        if n == 1: p[0][i] = 12
        if n == 2: p[0][i] = 11; p[1][i] = 12
        if w == 1: p[i][0] = 12
        if w == 2: p[i][0] = 11; p[i][1] = 12
    for i in range(8):
        if s == 1: p[7][i] = 13
        if s == 2: p[6][i] = 13; p[7][i] = 11
        if e == 1: p[i][7] = 13
        if e == 2: p[i][6] = 13; p[i][7] = 11
    return p
TERR = [terr_tile(l, c) for l in range(4) for c in range(81)]

# ---------------- decor ----------------
def parse(rows): return [[int(ch, 16) if ch not in '.' else 0 for ch in r] for r in rows]
FOOD_A = parse(["........", "...11...", "..1231..", ".112211.", "12311231", "12211221", ".11..11.", "........"])
FOOD_B = parse(["........", "....44..", "...44...", ".11.411.", "15311531", "15511551", ".11..11.", "........"])
TRAIL1 = parse(["........", "........", ".....9..", "........", "........", "..9.....", "........", "........"])
TRAIL2 = parse(["........", "......9.", "....9...", "........", "...9....", ".9......", "......9.", "........"])
# nest mound 24x24: a dome with a dark hole in the middle
M = [[0] * 24 for _ in range(24)]
cx, cy = 11.5, 12.5
for y in range(24):
    for x in range(24):
        dx, dy = (x - cx) / 11.0, (y - cy) / 9.5
        d = dx * dx + dy * dy
        if d <= 1.0:
            c = 6
            if dx + dy < -0.45: c = 7
            if dx + dy > 0.55: c = 10
            if d > 0.80: c = 1 if d > 0.92 else (10 if dy > 0 else c)
            if ((x * 7 + y * 13) % 11 == 0) and d < 0.8: c = 11
            hx, hy = (x - 11.5) / 3.6, (y - 11.5) / 2.6
            hd = hx * hx + hy * hy
            if hd <= 1.0: c = 8
            elif hd <= 1.6 and hy < 0: c = 10
            elif hd <= 1.6: c = 11
            M[y][x] = c
MOUND = [[[M[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(3) for tx in range(3)]
DECO = [FOOD_A, FOOD_B, TRAIL1, TRAIL2] + MOUND

# ---------------- sprites ----------------
ANT_A = parse(["........", ".......1", ".11...1.", "23321220", "22221232", ".22.122.", "..1.1.1.", ".1.1.1.."])
ANT_B = parse(["........", ".......1", ".11...1.", "23321220", "22221232", ".22.122.", ".1.1.1..", "..1.1.1."])
CAR_A = parse(["........", ".....55.", ".11..55.", "23321220", "22221232", ".22.122.", "..1.1.1.", ".1.1.1.."])
CAR_B = parse(["........", ".....55.", ".11..55.", "23321220", "22221232", ".22.122.", ".1.1.1..", "..1.1.1."])
SOL_A = parse(["......1.", ".......1", ".11..44.", "23321442", "22221231", ".22.1221", "..1.1.1.", ".1.1.1.."])
SOL_B = parse(["......1.", ".......1", ".11..44.", "23321442", "22221231", ".22.1221", ".1.1.1..", "..1.1.1."])
CUR8 = parse(["333..333", "31....13", "3......3", "........", "........", "3......3", "31....13", "333..333"])
QUEEN8 = parse(["....5.5.", "....555.", ".66.1221", "6666232.", "23321220", "22221232", ".22.122.", ".1.1.1.."])
PLUS = parse(["........", "..11....", ".1221111", "1222122.", ".1221111", "..1111.1", "...1.1..", "........"])
# "+1" in gold with a dark rim
PLUS = parse(["........", ".1.....1", "121...12", "222..122", "121...12", ".1....12", "......12", "........"])
SPLASH = parse(["........", "...4....", ".4.3.4..", "..434...", "4.444.4.", ".44344..", "..4.4...", "........"])
PUFF = parse(["........", ".5...5..", "..5.5...", "...5....", "..5.5...", ".5...5..", "........", "........"])
HEART = parse(["........", ".66.66..", "6336666.", "6666666.", ".66666..", "..666...", "...6....", "........"])
EMPTY = [[0] * 8 for _ in range(8)]
# queen 16x16, facing right: crown, wings, big gaster
Q = [[0] * 16 for _ in range(16)]
def disc(img, cx, cy, rx, ry, c, w=16):
    for y in range(len(img)):
        for x in range(w):
            dx, dy = (x - cx) / rx, (y - cy) / ry
            if dx * dx + dy * dy <= 1.0: img[y][x] = c
# outline pass: draw each part slightly larger in colour 1 first
disc(Q, 4.5, 9.0, 4.6, 3.6, 1); disc(Q, 9.0, 9.0, 2.4, 2.2, 1); disc(Q, 12.5, 8.0, 2.8, 2.6, 1)
disc(Q, 4.5, 9.0, 3.6, 2.6, 2); disc(Q, 9.0, 9.0, 1.4, 1.3, 2); disc(Q, 12.5, 8.0, 1.8, 1.6, 2)
for (x, y) in [(3, 8), (4, 7), (5, 7), (12, 7)]: Q[y][x] = 3
Q[8][13] = 6
# wings
for (x, y) in [(5,3),(6,3),(7,3),(4,4),(5,4),(6,4),(7,4),(8,4),(4,5),(5,5),(6,5),(7,5),(8,5),(6,6),(7,6)]: Q[y][x] = 6
for (x, y) in [(5,2),(6,2),(3,4),(3,5),(8,6)]: Q[y][x] = 1
# crown
for (x, y) in [(11,3),(13,3),(15,3),(11,4),(12,4),(13,4),(14,4),(15,4),(11,5),(12,5),(13,5),(14,5)]: Q[y][x] = 5
Q[5][15] = 1
# antenna + legs
for (x, y) in [(15,6),(14,6)]: Q[y][x] = 1
for (x, y) in [(7,12),(6,13),(9,12),(9,13),(11,11),(12,12),(13,13),(5,12),(4,13)]: Q[y][x] = 1
QUEEN = [[[Q[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(2) for tx in range(2)]
# big cursor 16x16: corner brackets around the 8x8 cell at (4..11)
C = [[0] * 16 for _ in range(16)]
for (a, b) in [(1, 1), (14, 1), (1, 14), (14, 14)]:
    sx = 1 if a < 8 else -1; sy = 1 if b < 8 else -1
    for k in range(5):
        C[b][a + sx * k] = 3; C[b + sy * k][a] = 3
        C[b + sy][a + sx * k] = 1 if k > 0 else 3; C[b + sy * k][a + sx] = 1 if k > 0 else 3
    C[b + sy][a + sx] = 1
CUR16 = [[[C[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(2) for tx in range(2)]
SPR = [ANT_A, CUR8, QUEEN8, ANT_A, ANT_B, CAR_A, CAR_B, SOL_A, SOL_B, PLUS, SPLASH, PUFF, HEART, EMPTY, EMPTY, EMPTY] + QUEEN + CUR16

def pack(t): return [sum(t[y][x] << (4 * x) for x in range(8)) for y in range(8)]
def emit(name, tiles):
    out = ["static const u32 %s[%d] = {" % (name, len(tiles) * 8)]
    for t in tiles: out.append("  " + ",".join("0x%08X" % v for v in pack(t)) + ",")
    out.append("};")
    return "\n".join(out)
def emit_pal(name, pal): return "static const u16 %s[16] = {%s};" % (name, ",".join(str(r | g << 5 | b << 10) for r, g, b in pal))

with open(sys.argv[1], "w") as f:
    f.write("// GENERATED by art2.py - do not edit. 4bpp tiles + palettes for the remastered look.\n#pragma once\n")
    f.write("// TERR: 4 levels x 81 auto-tiles (code = N + 3E + 9S + 27W; 0 plain, 1 step/shore, 2 cliff)\n")
    f.write(emit("TERR", TERR) + "\n")
    f.write("// DECO: food A, food B, trail weak, trail strong, nest mound 3x3\n" + emit("DECO", DECO) + "\n")
    f.write("// SPR: 0 ant, 1 small cursor, 2 small queen, 3/4 ant walk, 5/6 carrying, 7/8 soldier, 9 +1, 10 splash, 11 puff, 12 heart, 16-19 queen 16x16, 20-23 cursor 16x16\n")
    f.write(emit("SPRT", SPR) + "\n")
    f.write(emit_pal("PAL_TERR", TERR_PAL) + "\n" + emit_pal("PAL_DEC", DEC_PAL) + "\n")
    f.write(emit_pal("PAL_ANT0", ANT_PAL[0]) + "\n" + emit_pal("PAL_ANT1", ANT_PAL[1]) + "\n" + emit_pal("PAL_FX", FX_PAL) + "\n")

# ---------- preview ----------
def c8(c): return tuple(v * 255 // 31 for v in c)
def put(img, t, ox, oy, pal, sc, transparent_bg=None):
    for y in range(8):
        for x in range(8):
            v = t[y][x]
            if v == 0 and transparent_bg is not None: col = transparent_bg
            else: col = c8(pal[v])
            for a in range(sc):
                for b in range(sc): img.putpixel((ox + x * sc + a, oy + y * sc + b), col)
if len(sys.argv) > 2:
    sc = 3
    img = Image.new("RGB", (81 * 8 * 1 + 10, 4 * 8 + 300), (40, 40, 40))
    for l in range(4):
        for c in range(81): put(img, TERR[l * 81 + c], c * 8, l * 8, TERR_PAL, 1)
    for i, t in enumerate(DECO): put(img, t, 10 + i * 26, 50, DEC_PAL, 3, (60, 120, 50))
    for i, t in enumerate(SPR): put(img, t, 10 + i * 26, 100, ANT_PAL[0] if i < 9 or 16 <= i < 20 else FX_PAL, 3, (200, 180, 120))
    for i, t in enumerate(SPR[:9] + QUEEN): put(img, t, 10 + i * 26, 140, ANT_PAL[1], 3, (60, 120, 50))
    # big preview of queen + mound
    for i in range(9): put(img, MOUND[i], 10 + (i % 3) * 32, 180 + (i // 3) * 32, DEC_PAL, 4, (60, 120, 50))
    for i in range(4): put(img, QUEEN[i], 140 + (i % 2) * 32, 180 + (i // 2) * 32, ANT_PAL[0], 4, (60, 120, 50))
    for i in range(4): put(img, CUR16[i], 230 + (i % 2) * 32, 180 + (i // 2) * 32, [(0,0,0),(2,2,2),(0,0,0),(31,28,4)]+[(0,0,0)]*12, 4, (60, 120, 50))
    img.save(sys.argv[2])
