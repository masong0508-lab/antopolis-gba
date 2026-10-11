# Generates src/art2.h: terrain auto-tiles, decor tiles (food, trails, nest mound), sprite tiles and palettes (GBA 4bpp).
import sys
from PIL import Image

def rgb(r, g, b): return (r, g, b)
# --- palettes (5-bit values) ---
TERR_PAL = [rgb(0,0,0), rgb(3,8,20), rgb(6,13,26), rgb(14,22,31), rgb(28,24,14), rgb(23,19,10), rgb(9,20,7), rgb(6,14,5),
            rgb(18,13,8), rgb(24,19,13), rgb(26,29,31), rgb(3,2,2), rgb(31,30,22), rgb(11,8,5), rgb(10,18,30), rgb(13,25,9)]
DEC_PAL = [rgb(0,0,0), rgb(3,2,1), rgb(31,25,6), rgb(31,31,18), rgb(5,20,4), rgb(29,5,8), rgb(15,9,4), rgb(22,15,8),
           rgb(1,1,1), rgb(31,24,12), rgb(10,6,3), rgb(26,19,11), rgb(8,14,28), rgb(29,29,30), rgb(16,16,18), rgb(18,8,24)]
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


# ---------------- big 32x32 sprites for the ANT EYE view (scaled by the hardware) ----------------
def canvas(): return [[0] * 32 for _ in range(32)]
def ell(img, cx, cy, rx, ry, c):
    for y in range(32):
        for x in range(32):
            dx, dy = (x - cx) / rx, (y - cy) / ry
            if dx * dx + dy * dy <= 1.0: img[y][x] = c
def line(img, x0, y0, x1, y1, c):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(n + 1):
        img[round(y0 + (y1 - y0) * i / n)][round(x0 + (x1 - x0) * i / n)] = c
def big_ant(carry=False, queen=False):
    A = canvas()
    # legs (behind the body), 3 pairs from the thorax, feet on the ground (row 29)
    for (x0, x1) in [(13, 8), (15, 15), (17, 22)]:
        line(A, x0, 20, (x0 + x1) // 2, 24, 1); line(A, (x0 + x1) // 2, 24, x1, 29, 1)
    # outline shapes, then fills: gaster (rear), thorax, head
    ell(A, 7.5, 17.5, 7.0, 5.6, 1); ell(A, 15.5, 18.5, 3.6, 3.0, 1); ell(A, 23.5, 15.5, 5.0, 4.4, 1)
    ell(A, 7.5, 17.5, 6.0, 4.6, 2); ell(A, 15.5, 18.5, 2.6, 2.0, 2); ell(A, 23.5, 15.5, 4.0, 3.4, 2)
    ell(A, 5.5, 15.0, 2.6, 1.6, 3); ell(A, 22.0, 13.5, 1.6, 1.0, 3)
    A[14][25] = 6; A[14][26] = 6; A[15][25] = 1                  # eye
    line(A, 26, 12, 29, 6, 1); line(A, 29, 6, 31, 5, 1)          # antenna
    line(A, 25, 11, 26, 5, 1)
    line(A, 27, 19, 30, 21, 1)                                    # mandible
    if carry:
        ell(A, 29.0, 20.5, 2.6, 2.6, 1); ell(A, 29.0, 20.5, 1.7, 1.7, 5); A[19][28] = 6
    if queen:
        for (x, y) in [(5,8),(6,8),(7,8),(8,8),(9,8),(10,8),(11,8),(4,9),(5,9),(6,9),(7,9),(8,9),(9,9),(10,9),(11,9),(12,9),
                       (6,10),(7,10),(8,10),(9,10),(10,10),(11,10),(12,10),(13,10),(8,11),(9,11),(10,11),(11,11),(12,11),(13,11),(14,11)]:
            A[y][x] = 6                                           # wings
        for (x, y) in [(5,7),(6,7),(7,7),(8,7),(9,7),(10,7),(3,9),(14,10),(15,11)]: A[y][x] = 1
        for x in range(21, 27): A[10][x] = 5; A[9][x] = 5
        for x in (21, 23, 25): A[8][x] = 5; A[7][x] = 5
        A[8][26] = 5; A[7][26] = 5
    return A
def big_food():
    F = canvas()
    for (cx, cy) in [(10, 24), (16, 22), (22, 24), (13, 27.5), (19, 27.5), (16, 17)]:
        ell(F, cx, cy, 4.2, 3.4, 1); ell(F, cx, cy, 3.2, 2.4, 2); F[int(cy) - 1][int(cx) - 1] = 3
    line(F, 16, 13, 18, 9, 4); line(F, 18, 9, 21, 8, 4); F[8][19] = 4; F[7][20] = 4
    return F
def split32(img): return [[[img[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(4) for tx in range(4)]
BIG = split32(big_ant()) + split32(big_ant(carry=True)) + split32(big_ant(queen=True)) + split32(big_food())


# ---------------- ANT EYE 2.0 sprites (OBJ tiles 192..), drawn for the first-person view ----------------
def thick(img, x0, y0, x1, y1, r, c):
    n = max(abs(x1 - x0), abs(y1 - y0), 1) * 2
    for i in range(n + 1):
        cx = x0 + (x1 - x0) * i / n; cy = y0 + (y1 - y0) * i / n
        for y in range(32):
            for x in range(32):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r: img[y][x] = c
def ant_step(carry=False):                        # side view, the other leg pose (walk cycle frame 2)
    A = big_ant(carry=carry)
    for y in range(20, 32):
        for x in range(32):
            if A[y][x] == 1 and y >= 21 and not (ell_in(x, y, 7.5, 17.5, 7.0, 5.6) or ell_in(x, y, 15.5, 18.5, 3.6, 3.0) or ell_in(x, y, 23.5, 15.5, 5.0, 4.4)) and not (carry and ell_in(x, y, 29, 20.5, 2.6, 2.6)) and not (x >= 27 and y <= 21):
                A[y][x] = 0
    for (x0, x1) in [(13, 11), (15, 19), (17, 25)]:
        line(A, x0, 20, (x0 + x1) // 2 + (2 if x1 > x0 else -2), 23, 1); line(A, (x0 + x1) // 2 + (2 if x1 > x0 else -2), 23, x1, 29, 1)
    return A
def ell_in(x, y, cx, cy, rx, ry):
    dx, dy = (x - cx) / rx, (y - cy) / ry
    return dx * dx + dy * dy <= 1.0
def ant_front():
    A = canvas()
    for (sx, ex, ey) in [(14, 5, 23), (15, 4, 27), (16, 7, 30)]:
        line(A, sx, 21, ex, ey - 4, 1); line(A, ex, ey - 4, ex - 1, ey, 1)
        line(A, 31 - sx, 21, 31 - ex, ey - 4, 1); line(A, 31 - ex, ey - 4, 32 - ex, ey, 1)
    ell(A, 15.5, 23, 4.5, 4, 1); ell(A, 15.5, 23, 3.5, 3, 2)
    ell(A, 15.5, 14.5, 7.0, 6.2, 1); ell(A, 15.5, 14.5, 6.0, 5.2, 2); ell(A, 13.5, 11.5, 2.2, 1.2, 3)
    for ex in (12, 19):
        A[13][ex] = 6; A[13][ex + 1] = 6; A[14][ex] = 6; A[14][ex + 1] = 1
    line(A, 12, 9, 9, 4, 1); line(A, 9, 4, 5, 2, 1); line(A, 19, 9, 22, 4, 1); line(A, 22, 4, 26, 2, 1)
    line(A, 13, 19, 12, 23, 1); line(A, 12, 23, 14, 24, 1); line(A, 18, 19, 19, 23, 1); line(A, 19, 23, 17, 24, 1)
    return A
def ant_back():
    A = canvas()
    for (sx, ex, ey) in [(14, 4, 22), (15, 3, 26), (16, 6, 30)]:
        line(A, sx, 17, ex, ey - 4, 1); line(A, ex, ey - 4, ex - 1, ey, 1)
        line(A, 31 - sx, 17, 31 - ex, ey - 4, 1); line(A, 31 - ex, ey - 4, 32 - ex, ey, 1)
    line(A, 13, 8, 9, 2, 1); line(A, 18, 8, 22, 2, 1)
    ell(A, 15.5, 9.5, 4.0, 3.2, 1); ell(A, 15.5, 9.5, 3.0, 2.2, 2)
    ell(A, 15.5, 20, 8.0, 7.5, 1); ell(A, 15.5, 20, 7.0, 6.5, 2); ell(A, 12.5, 16.5, 2.6, 1.8, 3)
    for x in range(10, 22): A[21][x] = 1 if A[21][x] == 2 and x % 2 == 0 else A[21][x]
    return A
def shadow():
    A = canvas()
    for y in range(32):
        for x in range(32):
            if ell_in(x, y, 15.5, 29.0, 12.5, 2.8) and (x + y) % 2 == 0: A[y][x] = 1
    return A
def mound():
    A = canvas()
    ell(A, 15.5, 31, 15.5, 16, 1); ell(A, 15.5, 31, 14.5, 15, 6)
    for (x, y) in [(6, 22), (24, 21), (12, 18), (20, 26), (9, 27), (17, 20)]: A[y][x] = 7; A[y][x + 1] = 7
    ell(A, 15.5, 18, 3.5, 2.5, 1); ell(A, 15.5, 18.5, 2.5, 1.5, 8)
    return A
def small(rows):
    t = [[0] * 16 for _ in range(16)]
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != '.': t[y][x] = int(ch, 16)
    return [[[t[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(2) for tx in range(2)]
TUFT = small(["................", "................", "................", "................", "........4.......", "...4....4...4...",
  "...4...44..4....", "....4..4...4....", "....4..4..4..4..", ".4..44.4..4.4...", "..4..4.4.4..4...", "..44.4444..4....",
  "...4.44444.4....", "....444444......", "......4444......", "................"])
FLOWER = small(["................", "................", "................", "......5.5.......", ".....55555......", "......535.......",
  ".....55555......", "......5.5.......", ".......4........", ".......4..4.....", "....4..4.44.....", ".....4.444......",
  "......444.......", ".......4........", ".......4........", "................"])
PEBBLE = small(["................", "................", "................", "................", "................", "................",
  "................", "................", "................", "......1111......", "....11eeee11....", "...1eeddeeee1...",
  "..1eeedeeeeee1..", "..1eeeeeeeeee1..", "...11111111111..", "................"])
ROCK = small(["................", "................", "................", "......11111.....", "....11eeeee11...", "...1eedeeeeee1..",
  "..1eeddeeeeeee1.", "..1eedeeeeeeee1.", ".1eeeeeeeeeeeee1", ".1eeeeeeeeaeee1.", ".1eeeeeeeeaaee1.", ".1eeeeeeeeeeee1.",
  "..1eeeeeeeeeee1.", "..1eeeeeeeeee1..", "...11111111111..", "................"])
MUSH = small(["................", "................", "................", "................", ".....111111.....", "...1155d5511....",
  "..155d55555d51..", "..155555d55551..", "...1111111111...", "......1dd1......", "......1dd1......", "......1dd1......",
  "......1dd1......", ".....11dd11.....", "....44444444....", "................"])
REED = small(["....3...........", "....a...........", "....a......3....", "....4......a....", "....4..3...a....", "....4..a...4....",
  "...44..a...4....", "...4...4..44....", "...4...4..4.....", "...4..44..4.....", "...4..4..44.....", "..44..4..4......",
  "..4...4..4......", ".44..44.44......", ".4...4..4.......", "................"])
def mandible():                                   # the left jaw, seen from inside the head (the right one is flipped)
    A = canvas()
    thick(A, 2, 34, 13, 18, 5.5, 1); thick(A, 13, 18, 24, 8, 4.0, 1); thick(A, 24, 8, 29, 6, 2.5, 1)
    thick(A, 2, 34, 13, 18, 4.3, 2); thick(A, 13, 18, 24, 8, 2.8, 2); thick(A, 24, 8, 28, 6.5, 1.4, 2)
    thick(A, 4, 27, 12, 17, 1.2, 3); thick(A, 12, 17, 20, 10, 0.8, 3)
    for (x, y) in [(16, 17), (20, 14), (24, 11), (18, 16), (22, 12)]: A[y][x] = 1; A[y + 1][x - 1] = 1
    return A
EYE2 = (split32(ant_step()) + split32(ant_step(carry=True)) + split32(ant_front()) + split32(ant_back()) + split32(shadow()) + split32(mound())
        + TUFT + FLOWER + PEBBLE + ROCK + MUSH + REED + split32(mandible()))


# ---------------- ANT EYE panorama: 256x64 px, wraps around; far hills, trees, clouds (BG0 in the eye view, palette bank 5) ----------------
import math, random
random.seed(7)
PANO_PAL = [rgb(0,0,0), rgb(12,18,24), rgb(8,14,17), rgb(5,11,9), rgb(30,31,31), rgb(24,27,30), rgb(16,22,26), rgb(3,8,6)] + [rgb(0,0,0)]*8
P = [[0] * 256 for _ in range(64)]
def ridge(base, amp, freqs, seed):
    out = []
    for x in range(256):
        v = base
        for i, f in enumerate(freqs): v += amp / (i + 1) * math.sin(2 * math.pi * (x * f / 256.0) + seed * (i + 1))
        out.append(int(v))
    return out
far = ridge(44, 7, [2, 5, 9], 1.3); near = ridge(54, 5, [3, 7, 13], 4.1)
for x in range(256):
    for y in range(64):
        if y >= far[x]: P[y][x] = 1 if y > far[x] else 6
        if y >= near[x]: P[y][x] = 2
# little trees on the near ridge
for k in range(14):
    tx = random.randrange(256); ty = near[tx]; h = random.randrange(5, 9)
    for y in range(ty - h, ty + 1):
        w = (y - (ty - h)) // 2
        for x in range(tx - w, tx + w + 1): P[y % 64][x % 256] = 3 if x != tx - w else 7
# clouds
for k in range(6):
    cx0 = random.randrange(256); cy0 = random.randrange(8, 22); n = random.randrange(3, 6)
    for j in range(n):
        bx = cx0 + j * 6 - n * 3; by = cy0 + random.randrange(-2, 2); r = random.randrange(4, 7)
        for y in range(by - r, by + r + 1):
            for x in range(bx - r * 2, bx + r * 2 + 1):
                dx, dy = (x - bx) / (r * 2.0), (y - by) / float(r)
                if dx * dx + dy * dy <= 1 and 0 <= y < 64: P[y][x % 256] = 5 if y > by + r // 3 else 4
PANO_TILES = []; PANO_MAP = []
for ty in range(8):
    for tx in range(32):
        t = [[P[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)]
        if all(v == 0 for r in t for v in r): PANO_MAP.append(-1)
        else: PANO_MAP.append(len(PANO_TILES)); PANO_TILES.append(t)


# ---------------- UI: termite-world tiles (BG3, palette bank 0 colours 4..15) ----------------
# 4 soil dark  5 soil mid  6 soil light  7 pebble  8 bark dark  9 bark mid  10 bark light  11 tunnel  12 tunnel rim
# 13 grass  14 grass light  15 root
random.seed(11)
def soil(seed):
    r = random.Random(seed); t = [[4] * 8 for _ in range(8)]
    for _ in range(9): t[r.randrange(8)][r.randrange(8)] = 5
    for _ in range(3): t[r.randrange(8)][r.randrange(8)] = 6
    if seed % 2 == 0:
        px, py = r.randrange(1, 6), r.randrange(1, 6); t[py][px] = 7; t[py][px + 1] = 7; t[py + 1][px] = 5
    return t
UI_SOIL = [soil(k) for k in range(4)]
def framed(sides):   # bark edge on the given sides (n e s w), 3 px: dark outer, mid, light inner line
    t = soil(7)
    for i in range(8):
        for d, c in ((0, 8), (1, 9), (2, 10)):
            if 'n' in sides: t[d][i] = c
            if 's' in sides: t[7 - d][i] = c
            if 'w' in sides: t[i][d] = c
            if 'e' in sides: t[i][7 - d] = c
    if 'n' in sides and 'w' in sides: t[0][0] = 0; t[0][1] = 8; t[1][0] = 8
    if 'n' in sides and 'e' in sides: t[0][7] = 0; t[0][6] = 8; t[1][7] = 8
    if 's' in sides and 'w' in sides: t[7][0] = 0; t[7][1] = 8; t[6][0] = 8
    if 's' in sides and 'e' in sides: t[7][7] = 0; t[7][6] = 8; t[6][7] = 8
    return t
UI_FRAME = [framed(x) for x in ('nw', 'n', 'ne', 'w', 'e', 'sw', 's', 'se')]
def fringe(seed):   # top of the HUD: grass blades and roots hanging into soil
    r = random.Random(seed); t = soil(seed + 20)
    for x in range(8):
        h = r.randrange(1, 3)
        for y in range(h): t[y][x] = 13 if (x + y) % 3 else 14
    rx = r.randrange(1, 7)
    for y in range(2, 7): t[y][rx + (y > 4)] = 15
    return t
UI_FRINGE = [fringe(1), fringe(2)]
def tunnel(mask):   # mask bits: 1 n open, 2 e, 4 s, 8 w (neighbour is tunnel too)
    t = [[11] * 8 for _ in range(8)]
    for i in range(8):
        if not mask & 1: t[0][i] = 4; t[1][i] = 12
        if not mask & 4: t[7][i] = 6; t[6][i] = 12
        if not mask & 8: t[i][0] = 4; t[i][1] = 12
        if not mask & 2: t[i][7] = 5; t[i][6] = 12
    for (cx, cy, m1, m2) in ((0, 0, 1, 8), (7, 0, 1, 2), (0, 7, 4, 8), (7, 7, 4, 2)):
        if (mask & m1) and (mask & m2): t[cy][cx] = 12
    return t
UI_TUN = [tunnel(m) for m in range(16)]
def root():
    t = soil(30)
    for y in range(8): t[y][3 + (y // 3) % 2] = 15
    t[5][5] = 15; t[6][6] = 15
    return t
UI = UI_SOIL + UI_FRAME + UI_FRINGE + UI_TUN + [root(), soil(31)]
# themes: SOIL, CLAY, MOSS, NIGHT (colours 4..15)
UI_THEMES = [
  [rgb(7,4,2), rgb(10,6,3), rgb(13,9,5), rgb(17,15,12), rgb(6,3,1), rgb(14,8,3), rgb(22,14,6), rgb(2,1,1), rgb(5,3,2), rgb(6,15,3), rgb(10,22,5), rgb(12,8,4)],
  [rgb(11,4,2), rgb(15,6,3), rgb(19,9,5), rgb(22,16,12), rgb(7,2,1), rgb(17,8,3), rgb(25,14,7), rgb(3,1,1), rgb(7,2,1), rgb(7,15,3), rgb(11,22,5), rgb(15,7,3)],
  [rgb(3,6,2), rgb(5,9,3), rgb(8,12,5), rgb(15,16,12), rgb(3,4,1), rgb(8,11,3), rgb(14,18,6), rgb(1,2,1), rgb(2,4,2), rgb(8,18,4), rgb(13,25,7), rgb(6,9,4)],
  [rgb(2,3,7), rgb(4,5,10), rgb(6,8,14), rgb(13,14,18), rgb(2,2,5), rgb(6,7,14), rgb(11,13,22), rgb(1,1,2), rgb(3,3,6), rgb(4,10,8), rgb(7,15,12), rgb(6,6,10)],
  [rgb(14,8,1), rgb(18,11,2), rgb(23,15,4), rgb(29,24,10), rgb(9,5,1), rgb(22,14,3), rgb(30,22,8), rgb(5,3,0), rgb(10,6,1), rgb(9,16,3), rgb(14,23,5), rgb(19,11,3)],
  [rgb(3,6,10), rgb(5,10,15), rgb(9,15,21), rgb(22,27,31), rgb(3,5,9), rgb(10,16,24), rgb(20,26,31), rgb(1,2,4), rgb(4,7,12), rgb(8,18,16), rgb(14,26,24), rgb(9,13,20)]]


# ---------------- EMPIRE MODE building tiles (decor layer, decor palette; 1 outline 2 gold 3 light 4 leaf 5 red 6 mound 7 mound light
#                  8 black 9 trail 10 dark brown 11 tan 12 blue 13 white 14 stone 15 purple) ----------------
def PE(rows): return parse(rows)
EMP = [
PE(["........", "..1111..", ".166761.", "16677761", "16688661", "16688661", ".111111.", "........"]),   # 0 your nest (big mound + door)
PE(["........", ".4..4.4.", "434.434.", ".4...4..", "aaaaaaaa", "6a6a6a6a", "aaaaaaaa", "........"]),   # 1 fungus farm (mushrooms in rows)
PE(["........", "..1111..", ".1aaaa1.", "1a8888a1", "1a8888a1", ".1aaaa1.", "..1111..", "........"]),   # 2 dirt pit
PE(["........", "...11...", "..1771..", ".177771.", "1788887.", "1787787.", "1777777.", "11111111"]),   # 3 chambers (housing)
PE(["..1.....", ".151....", ".1551...", ".1511...", "e1e1e1e1", "eeeeeeee", "e1ee1eee", "eeeeeeee"]),   # 4 barracks (flag + stone)
PE(["..eeee..", "..e88e..", "..eeee..", "...ee...", "...ee...", "..eeee..", ".eeeeee.", "........"]),   # 5 tower
PE(["........", ".1111111", ".1dbdbd1", ".1bdbdb1", ".1dbdbd1", ".1111111", "..b..b..", "........"]),   # 6 archive (scent library)
PE(["..5555..", ".555555.", "1......1", "1.2.3.21", "1.3.2.31", "11111111", ".1....1.", "........"]),   # 7 market (awning + goods)
PE(["...f....", "..fff...", ".f.f.f..", "...f....", "..fff...", ".fffff..", "fffffff.", "........"]),   # 8 spore temple
PE(["........", ".eeeee..", "e.2..ee.", "e.22.e..", "ee.2.ee.", ".e.ee...", "..e.....", "........"]),   # 9 amber mine
PE(["....5...", "....55..", "....555.", "....1...", "....1...", "...777..", "..77777.", "........"]),   # 10 outpost (flag on a mound)
PE(["...22...", "..2332..", ".272272.", "27777772", "77777777", "76666667", "66666666", "........"]),   # 11 great mound (finished)
PE(["........", "...e....", "..e.e...", ".e...e..", "e.....e.", "e.1.1.e.", "eeeeeee.", "........"]),   # 12 scaffold (wonder in progress)
PE(["........", "........", "..1.1...", ".1a1a1..", "1aaaaa1.", "........", "........", "........"]),   # 13 rubble
PE(["2.......", "........", "........", "........", "........", "........", "........", ".......2"]),   # 14 territory corner marks
PE(["........", "..5..5..", "...55...", "...55...", "..5..5..", "........", "........", "........"]),   # 15 enemy claim mark
PE(["........", ".1..1...", "1b11b1..", "1bbbb1.4", ".1bb1.44", "..11..4.", "4..44...", ".44....."]),   # 16 aphid ranch
PE(["..1111..", ".1bbbb1.", "1bbbbbb1", "1b2b2bb1", "1bbbbbb1", "1bb22bb1", "11111111", "........"]),   # 17 granary
PE(["eeeeeeee", "e1ee1ee1", "eeeeeeee", "1ee1ee1e", "eeeeeeee", "e1ee1ee1", "eeeeeeee", "........"]),   # 18 wall
PE(["........", "........", ".e.e.e..", "eeeeeee.", "a8a8a8a.", "aaaaaaa.", "........", "........"]),   # 19 spike pit
PE(["........", "..7777..", ".7d77d7.", "7777777.", "7d7777d.", "7777777.", ".11111..", "........"]),   # 20 nursery (eggs)
PE(["........", "...55...", "...55...", ".555555.", ".555555.", "...55...", "...55...", "........"]),   # 21 healer
PE(["...e....", "..eee...", "...e....", "1..e..1.", "1e.e.e1.", ".1eee1..", "..111...", "........"]),   # 22 armory
PE(["....f...", "...fff..", "..f3f...", "...f....", "..fff...", ".eeeee..", "eeeeeee.", "........"]),   # 23 spitter nest
PE(["........", ".5.4.5..", "454.454.", ".4.4.4..", "4.444.4.", "..444...", "aaaaaaa.", "........"]),   # 24 garden
PE(["...2....", "..2.2...", ".2...2..", "2222222.", "2.d.d.2.", "2.d.d.2.", "2222222.", "........"]),   # 25 dance hall
PE(["...2....", "..222...", "...2....", "..222...", ".2.2.2..", "...2....", "..eee...", ".eeeee.."]),   # 26 statue
PE(["...c....", "..ccc...", ".c.c.c..", "ccccccc.", "c1c1c1c.", "ccccccc.", "c1c1c1c.", "ccccccc."]),   # 27 academy
PE(["........", ".111111.", "1222222.", "12111121", "12122121", "12111121", "1222222.", ".111111."]),   # 28 amber vault
PE(["...2....", "..222...", ".22222..", "2222222.", "2d2d2d2.", "2d2d2d2.", "2222222.", "2222222."]),   # 29 palace
PE(["...d....", "...dd...", "...ddd..", "...1....", "..ccc...", ".ccccc..", "cc1c1cc.", "........"]),   # 30 embassy
PE(["aaaaaaaa", "b.b.b.b.", "aaaaaaaa", "........", "........", "aaaaaaaa", "b.b.b.b.", "aaaaaaaa"]),   # 31 bridge
PE(["........", "..5555..", ".5.2.35.", "5555555.", "1......1", "1.2.3.21", "11111111", "........"]),   # 32 trade post
PE(["7......7", "........", "..7..7..", "........", "........", "..7..7..", "........", "7......7"]),   # 33 housing zone dots
PE(["4......4", "........", "..4..4..", "........", "........", "..4..4..", "........", "4......4"]),   # 34 farm zone dots
PE(["c......c", "........", "..c..c..", "........", "........", "..c..c..", "........", "c......c"]),   # 35 work zone dots

# ---- ANT CITY tiles (36+): zone buildings by level, tunnels, city services ----
PE(["........", "........", "...11...", "..1771..", ".177771.", ".178871.", ".111111.", "........"]),   # 36 homes L1 (small chamber)
PE(["...11...", "..1771..", ".177771.", "1d7887d1", "17777771", "17d88d71", "11111111", "........"]),   # 37 homes L2 (two-room mound)
PE(["..1771..", ".17d871.", ".178871.", "17d77d71", "1787787.", "17d77d71", "17888871", "11111111"]),   # 38 homes L3 (tower mound, lit windows)
PE(["........", "........", "........", "..4..4..", ".434434.", "aaaaaaaa", "aaaaaaaa", "........"]),   # 39 farms L1 (sprouts)
PE(["........", ".4..4..4", "434434434"[:8], ".4..4..4", "aaaaaaaa", "6a6a6a6a", "aaaaaaaa", "........"]),   # 40 farms L2 (rows)
PE(["..1111..", ".1dddd1.", "1d4d4dd1", "1d434d41", "1dd4dd41", "1d4dd4d1", "11111111", "........"]),   # 41 farms L3 (fungus dome)
PE(["........", "........", "..1111..", ".1aaaa1.", "1a8888a1", ".1aaaa1.", "..1111..", "........"]),   # 42 works L1 (dig pit)
PE(["......e.", "......e.", ".1111.e.", "1eeee1e.", "1e88eeee", "1eeeeee1", "11111111", "........"]),   # 43 works L2 (workshop + chimney)
PE(["d.d...d.", ".e...e..", ".e1111e.", "1eeeeee1", "1e8e8e81", "1eeeeee1", "1e8e8e81", "11111111"]),   # 44 works L3 (factory, smoke)
]
def tun(m):
    t = [[0] * 8 for _ in range(8)]
    def box(x0, y0, x1, y1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): t[y][x] = 10
    box(2, 2, 5, 5)
    if m & 1: box(2, 0, 5, 2)
    if m & 2: box(5, 2, 7, 5)
    if m & 4: box(2, 5, 5, 7)
    if m & 8: box(0, 2, 2, 5)
    for y in range(8):
        for x in range(8):
            if t[y][x] == 10 and 3 <= x <= 4 and 3 <= y <= 4: t[y][x] = 8
            if t[y][x] == 10 and ((m & 1 and x in (3, 4) and y < 3) or (m & 4 and x in (3, 4) and y > 4) or (m & 2 and y in (3, 4) and x > 4) or (m & 8 and y in (3, 4) and x < 3)): t[y][x] = 8
    for y in range(8):                                   # a light rim along the tunnel walls
        for x in range(8):
            if t[y][x] == 0 and any(0 <= x + dx < 8 and 0 <= y + dy < 8 and t[y + dy][x + dx] in (8, 10) for dx, dy in ((1,0),(-1,0),(0,1),(0,-1))): t[y][x] = 11
    return t
EMP += [tun(m) for m in range(16)]                       # 45..60 tunnels by neighbour mask (n1 e2 s4 w8)
EMP += [
PE(["...5....", "..555...", "...1....", ".11111..", "1eeeee1.", "1e8e8e1.", "1eeeee1.", "1111111."]),   # 61 guard post
PE(["........", ".dddddd.", "dd5dddd.", "d555ddd.", "dd5dddd.", "dddddd1.", "d1dd1dd.", "1111111."]),   # 62 clinic
PE(["...c....", "..ccc...", ".ccccc..", "cdcdcdc.", "ccccccc.", "cdc8cdc.", "ccc8ccc.", "1111111."]),   # 63 school
PE(["...3....", "..3.3...", "...3....", ".11111..", ".18881..", ".18881..", ".11111..", "........"]),   # 64 air shaft
PE(["..cccc..", ".c3ccc..", ".cccccc.", "..cccc..", "...ee...", "...ee...", "..eeee..", ".eeeeee."]),   # 65 dew pump
PE(["........", ".4.4.4..", "4544454.", ".4.4.4..", "4444444.", ".4.5.4..", "4444444.", "........"]),   # 66 big park
PE(["2......2", "e222222e", "e444444e", "e4d44d4e", "e444444e", "e222222e", "eeeeeeee", "........"]),   # 67 stadium
PE(["...3....", "..333...", ".3c3c3..", "3cccccc3", "3c3c3c3.", "3cccccc3", "3c3c3c3.", "33333333"]),   # 68 sky mound (arcology)
PE(["..5.5...", ".535.5..", ".5335.5.", "53335.5.", "533335..", ".5335...", "..55....", "........"]),   # 69 fire
PE(["........", ".c.c.c..", "cccccccc", ".c.c.c..", "cccccccc", ".c.c.c..", "cccccccc", "........"]),   # 70 flood water
]


# buildings stand on a packed-dirt plot so they show up on any ground
for i, t in enumerate(EMP):
    if i in (13, 14, 15, 33, 34, 35, 69, 70) or 45 <= i <= 60: continue
    for y in range(8):
        for x in range(8):
            if t[y][x] == 0: t[y][x] = 10 if (x in (0, 7) or y in (0, 7)) else 11
# ---------------- EMPIRE MODE army banner 16x16 (three soldiers under a flag), OBJ tiles 88..91 ----------------
AR = [[0] * 16 for _ in range(16)]
def blit(dst, src, ox, oy):
    for y in range(8):
        for x in range(8):
            if src[y][x] and 0 <= ox + x < 16 and 0 <= oy + y < 16: dst[oy + y][ox + x] = src[y][x]
for y in range(0, 15): AR[y][1] = 1
for y in range(0, 5):
    for x in range(2, 8 - (y == 0 or y == 4)): AR[y][x] = 5
for y in (0, 4):
    for x in range(2, 7): AR[y][x] = 1
blit(AR, SOL_A, 7, 3); blit(AR, SOL_B, 2, 8); blit(AR, SOL_A, 8, 8)
ARMY = [[[AR[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)] for ty in range(2) for tx in range(2)]


# ---------------- home screen card icons 32x32 (OBJ tiles 96..159), drawn in the decor palette ----------------
def icon_book():
    I = canvas()
    for y in range(6, 27):
        for x in range(4, 28):
            I[y][x] = 13 if x != 15 and x != 16 else 1
    for y in range(6, 27): I[y][3] = 1; I[y][28] = 1
    for x in range(3, 29): I[5][x] = 1; I[27][x] = 1
    for y in range(9, 25, 3):
        for x in range(6, 14): I[y][x] = 14
        for x in range(18, 26): I[y][x] = 14
    for y in range(4, 12): I[y][22] = 5; I[y][23] = 5
    return I
def icon_map():
    I = canvas()
    for y in range(5, 27):
        for x in range(3, 27): I[y][x] = 4 if (x + y) % 9 < 6 else 12
    ell(I, 12, 14, 5, 4, 12); ell(I, 20, 21, 4, 3, 7)
    for y in range(5, 27): I[y][3] = 1; I[y][26] = 1
    for x in range(3, 27): I[5][x] = 1; I[26][x] = 1
    for k in range(12):                              # a pencil across the corner
        I[28 - k][16 + k] = 2; I[27 - k][16 + k] = 2; I[28 - k][17 + k] = 1
    I[29][15] = 1; I[28][15] = 8
    return I
def icon_trophy():
    I = canvas()
    ell(I, 16, 10, 10, 8, 2)
    for y in range(2, 10):
        for x in range(6, 27): I[y][x] = 2
    ell(I, 16, 9, 6, 5, 3)
    for y in range(17, 23): I[y][15] = 2; I[y][16] = 2
    for y in range(23, 28):
        for x in range(9, 24): I[y][x] = 2 if y < 27 else 1
    ell(I, 4, 8, 3, 4, 2); ell(I, 4, 8, 1.5, 2.5, 0); ell(I, 28, 8, 3, 4, 2); ell(I, 28, 8, 1.5, 2.5, 0)
    I[6][13] = 13; I[7][13] = 13; I[6][14] = 13
    return I
def icon_gear():
    I = canvas()
    import math as _m
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5; r = _m.hypot(dx, dy); a = _m.atan2(dy, dx)
            tooth = 13 if (int((a + _m.pi) / (2 * _m.pi) * 8 * 2) % 2 == 0) else 10.5
            if r <= tooth: I[y][x] = 14
            if r <= tooth and r > tooth - 1.2: I[y][x] = 1
            if r <= 5: I[y][x] = 1
            if r <= 4: I[y][x] = 0
    return I
def icon_city():
    I = canvas()
    for y in range(26, 31):
        for x in range(1, 31): I[y][x] = 10 if y > 26 else 1
    def tower(x0, x1, top, c):
        for y in range(top, 27):
            for x in range(x0, x1 + 1):
                I[y][x] = 1 if x in (x0, x1) or y == top else c
        for y in range(top + 3, 25, 4):
            for x in range(x0 + 2, x1 - 1, 3): I[y][x] = 2; I[y][x + 1 if x + 1 < x1 else x] = 2
    ell(I, 9, 26, 8, 9, 1); ell(I, 9, 26, 7, 8, 7)
    tower(13, 21, 4, 6); tower(22, 29, 12, 7)
    for y in range(18, 26, 3):
        for x in (6, 9, 12):
            if I[y][x] == 7: I[y][x] = 2
    for x in range(14, 21): I[2][x] = 0
    line(I, 17, 0, 17, 4, 1); I[0][18] = 5; I[1][18] = 5; I[1][19] = 5
    return I
def icon_verse():
    I = canvas()
    import math as _m
    ell(I, 16, 16, 11, 11, 1); ell(I, 16, 16, 10, 10, 12)
    ell(I, 12, 12, 4, 3, 4); ell(I, 20, 19, 5, 3, 4); ell(I, 14, 21, 2, 2, 4)
    for i in range(80):
        a = i / 80 * 2 * _m.pi; x = round(16 + 15 * _m.cos(a)); y = round(16 + 5 * _m.sin(a) - 4 * _m.cos(a))
        if 0 <= x < 32 and 0 <= y < 32 and (_m.sin(a) > -0.1 or I[y][x] in (0, 1)): I[y][x] = 2
    for x, y in ((3, 4), (28, 6), (5, 27), (27, 26), (24, 2)): I[y][x] = 3
    return I
ICONS = split32(icon_book()) + split32(icon_map()) + split32(icon_trophy()) + split32(icon_gear()) + split32(icon_city()) + split32(icon_verse())

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
    f.write("// BIGS: ANT EYE 32x32 sprites (OBJ tiles 24..87): ant, ant carrying food, queen (ant palettes), food pile (decor palette)\n")
    f.write(emit("BIGS", BIG) + "\n")
    f.write("// PANO: ANT EYE skyline tiles, PANOM: 32x8 map (0xFFFF = empty)\n" + emit("PANO", PANO_TILES) + "\n")
    f.write("#define PANO_N %d\n" % len(PANO_TILES))
    f.write("static const u16 PANOM[256] = {" + ",".join(str(v & 0xFFFF) for v in PANO_MAP) + "};\n")
    f.write(emit_pal("PAL_PANO", PANO_PAL) + "\n")
    f.write("// EYE2: ANT EYE 2.0 sprites (OBJ tiles 192..343): ant walk 2 (16), carrying walk 2 (16), front (16), back (16) [ant palette];\n//   shadow (16), nest mound (16), 6 small decor 16x16 (tuft flower pebble rock mushroom reed, 4 each) [decor palette]; jaw (16) [ant palette]\n" + emit("EYE2", EYE2) + "\n")
    f.write("// ICNT: home screen icons 32x32: book, map, trophy, gear, city, antverse (OBJ tiles 96..191, decor palette)\n" + emit("ICNT", ICONS) + "\n")
    f.write("// ARMT: EMPIRE MODE army banner 16x16 (OBJ tiles 88..91, ant palettes)\n" + emit("ARMT", ARMY) + "\n")
    f.write("// EMPT: EMPIRE MODE building tiles (decor palette), loaded at charblock 2 tile 512+\n" + emit("EMPT", EMP) + "\n")
    f.write("// UIT: termite UI tiles: 0-3 soil, 4-11 bark frame (nw n ne w e sw s se), 12-13 grass fringe, 14-29 tunnels (mask n1 e2 s4 w8), 30 root, 31 soil\n")
    f.write(emit("UIT", UI) + "\n")
    f.write("static const u16 UIPAL[%d][12] = {" % len(UI_THEMES) + "" + ",".join("{" + ",".join(str(r | g << 5 | b << 10) for r, g, b in th) + "}" for th in UI_THEMES) + "};\n")
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
    img = Image.new("RGB", (740, 4 * 8 + 300), (40, 40, 40))
    for l in range(4):
        for c in range(81): put(img, TERR[l * 81 + c], c * 8, l * 8, TERR_PAL, 1)
    for i, t in enumerate(DECO): put(img, t, 10 + i * 26, 50, DEC_PAL, 3, (60, 120, 50))
    for i, t in enumerate(SPR): put(img, t, 10 + i * 26, 100, ANT_PAL[0] if i < 9 or 16 <= i < 20 else FX_PAL, 3, (200, 180, 120))
    for i, t in enumerate(SPR[:9] + QUEEN): put(img, t, 10 + i * 26, 140, ANT_PAL[1], 3, (60, 120, 50))
    # big preview of queen + mound
    for i in range(9): put(img, MOUND[i], 10 + (i % 3) * 32, 180 + (i // 3) * 32, DEC_PAL, 4, (60, 120, 50))
    for i in range(4): put(img, QUEEN[i], 140 + (i % 2) * 32, 180 + (i // 2) * 32, ANT_PAL[0], 4, (60, 120, 50))
    for i in range(4): put(img, CUR16[i], 230 + (i % 2) * 32, 180 + (i // 2) * 32, [(0,0,0),(2,2,2),(0,0,0),(31,28,4)]+[(0,0,0)]*12, 4, (60, 120, 50))
    for k in range(4):
        for i in range(16):
            put(img, BIG[k * 16 + i], 330 + k * 100 + (i % 4) * 24, 180 + (i // 4) * 24, DEC_PAL if k == 3 else ANT_PAL[k == 1], 3, (60, 120, 50))
    pv = Image.new("RGB", (256, 64), (120, 170, 250))
    for y in range(64):
        for x in range(256):
            if P[y][x]: pv.putpixel((x, y), c8(PANO_PAL[P[y][x]]))
    pv.resize((768, 192), Image.NEAREST).save(sys.argv[2].replace(".png", "_pano.png"))
    ui = Image.new("RGB", (8 * 32 * 4, 8 * 4))
    upal = [(0,0,0)] * 4 + UI_THEMES[0]
    for i, t in enumerate(UI): put(ui, t, i * 32, 0, upal, 4, (200, 0, 200))
    ui.save(sys.argv[2].replace(".png", "_ui.png"))
    img.save(sys.argv[2])
