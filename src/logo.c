// DippInn logo sequence for Game Boy Advance (mode 0), ~6 s. Everything is drawn in code - no image files.
//   Phase 1 (frames 0-160):   parallax dusk scene; per-scanline HBlank DMA drives foliage edge, sun circle, ridge sway
//   Phase 2 (161-211):        grey screen + twisting scan line, cut to black
//   Phase 3 (212-358):        text fades in, rope underline grows, fade to black
// Use: call logo_play() once at boot (see logo.h).
#include "logo.h"

/* ---------- GBA hardware ---------- */
// Minimal GBA hardware definitions (no libgba/libtonc dependency).
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16; typedef int32_t s32;
#define R16(a) (*(volatile u16 *)(a))
#define REG_DISPCNT  R16(0x04000000)
#define REG_VCOUNT   R16(0x04000006)
#define REG_BGCNT(n) R16(0x04000008 + (n) * 2)
#define REG_BGHOFS(n) R16(0x04000010 + (n) * 4)
#define REG_BGVOFS(n) R16(0x04000012 + (n) * 4)
#define REG_WIN0H R16(0x04000040)
#define REG_WIN1H R16(0x04000042)
#define REG_WIN0V R16(0x04000044)
#define REG_WIN1V R16(0x04000046)
#define REG_WININ R16(0x04000048)
#define REG_WINOUT R16(0x0400004A)
#define REG_BLDCNT R16(0x04000050)
#define REG_BLDY   R16(0x04000054)
#define REG_KEYINPUT R16(0x04000130)
#define KEY_A 1
#define KEY_START 8

typedef struct { volatile u32 src, dst, cnt; } DmaRec;
#define DMA_REG ((DmaRec *)0x040000B0)
#define DMA_ENABLE    0x80000000u
#define DMA_HBLANK    0x20000000u
#define DMA_REPEAT    0x02000000u
#define DMA_DST_FIXED 0x00400000u

#define BG_PAL   ((volatile u16 *)0x05000000)
#define OBJ_PAL  ((volatile u16 *)0x05000200)
#define BG_TILES(cb) ((volatile u32 *)(0x06000000 + (cb) * 0x4000))
#define BG_MAP(sb)   ((volatile u16 *)(0x06000000 + (sb) * 0x800))
#define OBJ_TILES    ((volatile u32 *)0x06010000)
#define OAM          ((volatile u16 *)0x07000000)

#define DCNT_BG0 0x0100
#define DCNT_BG1 0x0200
#define DCNT_BG2 0x0400
#define DCNT_BG3 0x0800
#define DCNT_OBJ 0x1000
#define DCNT_WIN0 0x2000
#define DCNT_WIN1 0x4000
#define DCNT_OBJ_1D 0x0040
#define DCNT_BLANK 0x0080
// Black screen WITHOUT forced blank: forced blank (DISPCNT bit 7) shows WHITE on a GBA, which was the white flash between screens.
// All layers off -> only the backdrop shows, and BLDY=16 fades it to black.
#define BLACKOUT() do { REG_BLDCNT = 0xFF; REG_BLDY = 16; REG_DISPCNT = 0; } while (0)
#define BGCNT(prio, cb, sb) ((prio) | ((cb) << 2) | ((sb) << 8))   // 4bpp, 256x256

/* ---------- artwork, drawn at startup ---------- */
// All artwork is drawn in code at startup: no PNGs, no converters, no generated data.
// Charblock 0 tile slots (BG3 sky / BG2 foliage / BG1 sunlit / BG0 ridge)
#define T_FOL_BASE 32
#define T_SUN_BASE 96
#define T_RIDGE_BASE 160
// Charblock 1 tile slots (text phase + grey phase)
#define T_TEXT_BASE 1
#define TEXT_TW 15
#define TEXT_TH 2
#define TEXT_W (TEXT_TW * 8)
#define TEXT_H (TEXT_TH * 8)
#define T_ROPE_BASE 32
#define ROPE_TILES_W 16
#define T_SCAN_FLAT 96
#define T_SCAN_TWIST0 97
#define T_SCAN_TWIST1 98
// OBJ tile slots (1D mapping)
#define OBJT_FAR0 0      // 3 shapes x 2 tiles (16x8)
#define OBJT_NEAR0 6     // 3 shapes x 8 tiles (32x16)
#define OBJT_MOON 30     // 4 tiles (16x16)
#define OBJT_SUN 34      // 16 tiles (32x32): setting sun, OBJ palette bank 3

#define RGB15(r, g, b) ((u16)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

static s16 gfx_sin_lut[256];
static u32 gfx_hash(int x, int y) { u32 n = (u32)x * 374761393u + (u32)y * 668265263u; n = (n ^ (n >> 13)) * 1274126177u; return (n ^ (n >> 16)) & 255; }

static void zero32(volatile u32 *p, int n) { for (int i = 0; i < n; i++) p[i] = 0; }
static void zero16(volatile u16 *p, int n) { for (int i = 0; i < n; i++) p[i] = 0; }
static void px(volatile u32 *t, int tile, int x, int y, int v) {      // 4bpp pixel inside tile
    volatile u32 *w = &t[tile * 8 + (y & 7)]; int s = (x & 7) * 4;
    *w = (*w & ~(15u << s)) | ((u32)v << s);
}
static void setpal(volatile u16 *base, int bank, const u16 *c, int n) { for (int i = 0; i < n; i++) base[bank * 16 + i] = c[i]; }
static void rect(volatile u32 *t, int base, int wtiles, int x0, int y0, int w, int h) {   // filled rect on a sprite, colour 1
    for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) px(t, base + (y >> 3) * wtiles + (x >> 3), x, y, 1);
}

// ---------- BG0 charblock: sky (dithered, de-duplicated), foliage, sunlit, ridge ----------
static void build_sky(volatile u32 *t, volatile u16 *map) {
    static const u8 bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    u32 uniq[32][8]; int n = 1;
    for (int j = 0; j < 8; j++) uniq[0][j] = 0;
    for (int ty = 0; ty < 20; ty++) for (int tx = 0; tx < 30; tx++) {
        u32 tile[8];
        for (int r = 0; r < 8; r++) { u32 word = 0;
            for (int c = 0; c < 8; c++) {
                int x = tx * 8 + c, y = ty * 8 + r;
                int v = y * 5 * 256 / 160 + bayer[(y & 3) * 4 + (x & 3)] * 16 - 128;   // Q8 band value
                int band = (v + 128) >> 8; band = band < 0 ? 0 : band > 5 ? 5 : band;
                word |= (u32)(band + 1) << (c * 4);
            }
            tile[r] = word; }
        int k = 0;
        for (; k < n; k++) { int same = 1; for (int j = 0; j < 8; j++) if (uniq[k][j] != tile[j]) { same = 0; break; } if (same) break; }
        if (k == n && n < 32) { for (int j = 0; j < 8; j++) uniq[n][j] = tile[j]; n++; }
        map[ty * 32 + tx] = (u16)(k < 32 ? k : 0);
    }
    for (int i = 0; i < n; i++) for (int j = 0; j < 8; j++) t[i * 8 + j] = uniq[i][j];
}
static void build_foliage(volatile u32 *t) {
    for (int ty = 0; ty < 8; ty++) for (int tx = 0; tx < 8; tx++) for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        int x = tx * 8 + c, y = ty * 8 + r; u32 n = gfx_hash(x, y);
        px(t, T_FOL_BASE + ty * 8 + tx, c, r, n < 46 ? 1 : n > 183 ? 3 : 2);             // dark speckle texture
        px(t, T_SUN_BASE + ty * 8 + tx, c, r, n > 89 ? 4 : 0);                            // sunlit overlay (0 = see-through)
    }
}
static void build_ridge(volatile u32 *t) {
    for (int y = 0; y < 8; y++) for (int x = 0; x < 10; x++) px(t, T_RIDGE_BASE + (x >> 3), x, y, (x >= 3 && x < 6) ? 2 : 1);
}
static void build_sprites(void) {                                  // clouds (3 shapes, far 1x / near 2x) + moon
    static const u8 CL[3][3][4] = {{{2,2,10,4},{0,4,14,4},{4,0,6,3}}, {{0,2,8,3},{3,0,7,3},{6,3,9,3}}, {{1,1,12,3},{0,3,16,4},{5,0,6,2}}};
    volatile u32 *t = OBJ_TILES; zero32(t, 50 * 8);
    for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) {
        const u8 *r = CL[i][k];
        rect(t, OBJT_FAR0 + i * 2, 2, r[0], r[1], r[2], r[3]);
        rect(t, OBJT_NEAR0 + i * 8, 4, r[0] * 2, r[1] * 2, r[2] * 2, r[3] * 2);
    }
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
        if ((2 * x - 15) * (2 * x - 15) + (2 * y - 15) * (2 * y - 15) <= 108) px(t, OBJT_MOON + (y >> 3) * 2 + (x >> 3), x, y, 1);
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {         // sun: bright core (1), orange rim (2), dithered glow (3)
        int d = (2 * x - 31) * (2 * x - 31) + (2 * y - 31) * (2 * y - 31);     // 4 x squared distance from the centre
        int c = d <= 196 ? 1 : d <= 324 ? 2 : (d <= 676 && ((x + y) & 1)) ? 3 : (d <= 900 && !(x & 1) && !(y & 1)) ? 3 : 0;
        if (c) px(t, OBJT_SUN + (y >> 3) * 4 + (x >> 3), x, y, c);
    }
}

static void gfx_scene(int sb_sky, int sb_fol, int sb_sun, int sb_ridge) {
    for (int a = 0; a < 256; a++) {                                // integer sine (Bhaskara), no libm
        int h = a & 127, p = h * (128 - h), v = 4096 * p / (81920 - 4 * p);
        gfx_sin_lut[a] = (s16)(a >= 128 ? -v : v);
    }
    u16 sky[7];
    for (int b = 0; b < 6; b++) sky[b + 1] = RGB15(20 + b * 9, 50 + b * 14, 190 + b * 8);
    sky[0] = sky[1];                                               // backdrop = top sky band
    setpal(BG_PAL, 0, sky, 7);
    static const u16 fol[5] = {0, RGB15(2, 4, 28), RGB15(8, 16, 60), RGB15(20, 34, 100), RGB15(190, 170, 170)};
    static const u16 ridge[3] = {0, RGB15(0x4f, 0x92, 0xe8), RGB15(0x8f, 0xc0, 0xf5)};
    setpal(BG_PAL, 1, fol, 5); setpal(BG_PAL, 2, ridge, 3);
    static const u16 far_c[2] = {0, RGB15(0xc7, 0x7f, 0xb8)}, near_c[2] = {0, RGB15(0xe6, 0xdc, 0xf5)}, moon_c[2] = {0, RGB15(0x2a, 0x3f, 0x9a)};
    setpal(OBJ_PAL, 0, far_c, 2); setpal(OBJ_PAL, 1, near_c, 2); setpal(OBJ_PAL, 2, moon_c, 2);
    static const u16 sun_c[4] = {0, RGB15(0xff, 0xf4, 0xaa), RGB15(0xff, 0xa8, 0x3c), RGB15(0xf0, 0x96, 0x6e)}; setpal(OBJ_PAL, 3, sun_c, 4);

    volatile u32 *t = BG_TILES(0); zero32(t, (T_RIDGE_BASE + 2) * 8);
    zero16(BG_MAP(sb_sky), 1024);
    build_sky(t, BG_MAP(sb_sky)); build_foliage(t); build_ridge(t); build_sprites();
    volatile u16 *mf = BG_MAP(sb_fol), *ms = BG_MAP(sb_sun), *mr = BG_MAP(sb_ridge);
    for (int r = 0; r < 32; r++) for (int c = 0; c < 32; c++) {
        mf[r * 32 + c] = (T_FOL_BASE + (r & 7) * 8 + (c & 7)) | (1 << 12);
        ms[r * 32 + c] = (T_SUN_BASE + (r & 7) * 8 + (c & 7)) | (1 << 12);
        mr[r * 32 + c] = c < 2 ? (T_RIDGE_BASE + c) | (2 << 12) : 0;
    }
}

// ---------- grey phase: scan-line tiles (row of light + dark pixels) ----------
static void gfx_grey(void) {
    static const u16 scan[3] = {0, RGB15(0xbd, 0xb8, 0xcc), RGB15(0x6b, 0x67, 0x84)};
    setpal(BG_PAL, 5, scan, 3);
    volatile u32 *t = BG_TILES(1); zero32(t, (T_SCAN_TWIST1 + 1) * 8);
    for (int x = 0; x < 8; x++) {
        int l[3] = {0, (x >> 1) & 1, ((x >> 1) + 1) & 1};
        for (int k = 0; k < 3; k++) { px(t, T_SCAN_FLAT + k, x, l[k], 1); px(t, T_SCAN_FLAT + k, x, l[k] + 1, 2); }
    }
}

// ---------- text phase: 5x9 pixel font, outline computed at runtime; rope underline frames ----------
typedef struct { char ch; const char *row[9]; } Glyph;
static const Glyph FONT[] = {
 {'D', {"####.","#...#","#...#","#...#","#...#","#...#","####.",".....","....."}},
 {'P', {"####.","#...#","#...#","####.","#....","#....","#....",".....","....."}},
 {'I', {".###.","..#..","..#..","..#..","..#..","..#..",".###.",".....","....."}},
 {'i', {"..#..",".....",".##..","..#..","..#..","..#..",".###.",".....","....."}},
 {'p', {".....",".....","####.","#...#","#...#","#...#","####.","#....","#...."}},
 {'n', {".....",".....","####.","#...#","#...#","#...#","#...#",".....","....."}},
 {'r', {".....",".....","#.##.","##..#","#....","#....","#....",".....","....."}},
 {'o', {".....",".....",".###.","#...#","#...#","#...#",".###.",".....","....."}},
 {'d', {"....#","....#",".####","#...#","#...#","#...#",".####",".....","....."}},
 {'u', {".....",".....","#...#","#...#","#...#","#..##",".##.#",".....","....."}},
 {'c', {".....",".....",".###.","#...#","#....","#...#",".###.",".....","....."}},
 {'t', {".....",".#...","####.",".#...",".#...",".#..#","..##.",".....","....."}},
 {'s', {".....",".....",".####","#....",".###.","....#","####.",".....","....."}},
};
static u8 mask[TEXT_H][TEXT_W];
static void gfx_text(int sb_text) {
    static const u16 txt[3] = {0, RGB15(14, 9, 30), RGB15(128, 110, 215)};
    static const u16 rope[4] = {0, RGB15(0x8a, 0x7a, 0xe0), RGB15(0x4a, 0x3f, 0x8f), RGB15(0xd9, 0xd2, 0xff)};
    setpal(BG_PAL, 3, txt, 3); setpal(BG_PAL, 4, rope, 4);
    volatile u32 *t = BG_TILES(1); zero32(t, (T_ROPE_BASE + 64) * 8);
    for (int y = 0; y < TEXT_H; y++) for (int x = 0; x < TEXT_W; x++) mask[y][x] = 0;
    const char *s = "DippInn Productions"; int cx = 4;
    for (; *s; s++) {
        if (*s == ' ') { cx += 4; continue; }
        for (unsigned g = 0; g < sizeof FONT / sizeof FONT[0]; g++) if (FONT[g].ch == *s)
            for (int y = 0; y < 9; y++) for (int x = 0; x < 5; x++) if (FONT[g].row[y][x] == '#') mask[3 + y][cx + x] = 1;
        cx += 6;
    }
    for (int y = 0; y < TEXT_H; y++) for (int x = 0; x < TEXT_W; x++) {
        int v = mask[y][x] ? 1 : 0;
        if (!v) for (int dy = -1; dy <= 1 && !v; dy++) for (int dx = -1; dx <= 1; dx++) {
            int yy = y + dy, xx = x + dx;
            if (yy >= 0 && yy < TEXT_H && xx >= 0 && xx < TEXT_W && mask[yy][xx]) { v = 2; break; } }
        if (v) px(t, T_TEXT_BASE + (y >> 3) * TEXT_TW + (x >> 3), x, y, v);
    }
    for (int f = 0; f < 4; f++) {                                  // 4-frame twisted rope, 128 px wide
        for (int x = 0; x < 126; x++) { int w = ((x >> 1) + f) & 1;
            px(t, T_ROPE_BASE + f * 16 + ((x + 1) >> 3), x + 1, 2 + w, 1); px(t, T_ROPE_BASE + f * 16 + ((x + 1) >> 3), x + 1, 4 - w, 2); }
        for (int y = 2; y <= 4; y++) { px(t, T_ROPE_BASE + f * 16, 0, y, 3); px(t, T_ROPE_BASE + f * 16 + 15, 7, y, 3); }
    }
    volatile u16 *m = BG_MAP(sb_text); zero16(m, 1024);
    for (int r = 0; r < TEXT_TH; r++) for (int c = 0; c < TEXT_TW; c++) m[r * 32 + c] = (T_TEXT_BASE + r * TEXT_TW + c) | (3 << 12);
}

/* ---------- sequence ---------- */
// DippInn logo sequence for GBA (mode 0). See docs/TIMELINE.md for the timeline.
//  Phase 1 (0-160):   parallax dusk scene  - BG3 sky, BG2 foliage, BG1 sunlit patch, BG0 ridge, OBJ clouds+moon
//                     per-scanline HBlank DMA: WIN0H (sun circle), WIN1H (foliage edge), BG0HOFS (ridge)
//  Phase 2 (161-211): grey screen with twisting scan line, cut to black
//  Phase 3 (212-358): text fades in, rope underline grows from the centre, fade to black

#define P2 161
#define P3 212
#define HOLD_EXTRA 120   // +2 s: logo stays fully visible this many extra frames before the fade to black
#define SB_SKY 28
#define SB_FOL 29
#define SB_SUN 30
#define SB_RIDGE 31
#define SB_SCAN 25
#define SB_TEXT 26
#define SB_ROPE 27

static u16 tabW0[2][160], tabW1[2][160], tabH0[2][160];   // HBlank DMA tables (double-buffered)
static int cur;

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int sin8(int a) { return gfx_sin_lut[a & 255]; }          // a: 256 = full turn, result Q8
#define hv gfx_hash
static int isqrt(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
static void vsync(void) { while (REG_VCOUNT >= 160); while (REG_VCOUNT < 160); }

// ---- copy helpers ----
static void clear_oam(void) { for (int i = 0; i < 128; i++) { OAM[i*4] = 0x200; OAM[i*4+1] = 0; OAM[i*4+2] = 0; } }
static void dma_off(void) { for (int i = 1; i < 4; i++) DMA_REG[i].cnt = 0; }
static void hdma(int ch, const u16 *tab, u32 dst) {      // one halfword per HBlank; line 0 is written directly
    DMA_REG[ch].cnt = 0;
    *(volatile u16 *)dst = tab[0];
    DMA_REG[ch].src = (u32)(tab + 1); DMA_REG[ch].dst = dst;
    DMA_REG[ch].cnt = DMA_ENABLE | DMA_REPEAT | DMA_HBLANK | DMA_DST_FIXED | 1;
}
static void oam_set(int i, int x, int y, int shape, int size, int tile, int prio, int pal) {
    OAM[i*4] = (y & 255) | (shape << 14); OAM[i*4+1] = (x & 511) | (size << 14); OAM[i*4+2] = tile | (prio << 10) | (pal << 12);
}
static void set_fade(int lvl) { REG_BLDY = clampi(lvl, 0, 16); }   // 0 = normal, 16 = black

// ---- Phase 1: scene ----
static int edge_x(int y, int tq) {           // foliage left edge, wobbles with sines
    int a1 = ((y * 521) >> 8) + ((tq * 5215) >> 16), a2 = (y * 1981) >> 8;
    return 128 + ((sin8(a1) * 6) >> 8) + ((sin8(a2) * 3) >> 8);
}
static void build_tables(int b, int tq) {
    int cx = 215 - (((tq - 768) * 4) >> 8), cy = 150 - (((tq - 768) * 5) >> 8), R = 30;
    for (int y = 0; y < 160; y++) {
        int e = clampi(edge_x(y, tq), 0, 240);
        tabW1[b][y] = (u16)((e << 8) | 240);
        int dy = y - cy, w0 = 0;
        if (dy > -R && dy < R) {
            int dx = isqrt(R * R - dy * dy), l = cx - dx > e ? cx - dx : e, r = cx + dx < 240 ? cx + dx : 240;
            if (l < r) w0 = (l << 8) | r;
        }
        tabW0[b][y] = (u16)w0;
        int rx = e - 14 + ((sin8(((y * 834) >> 8) + ((tq * 10430) >> 16)) * 2) >> 8);
        tabH0[b][y] = (u16)(-rx) & 0x1FF;
    }
}
static void scene_oam(int tq) {
    for (int i = 0; i < 9; i++) {              // far pink clouds, 5 px/s, behind foliage
        int x0 = i * 30 + (int)hv(i, 1) * 20 / 255, y0 = 8 + (int)hv(i, 2) * 110 / 255;
        int x = ((x0 - ((tq * 5) >> 8)) % 270 + 270) % 270 - 30;
        int y = y0 + ((sin8((tq * 8342 >> 16) + ((x0 * 10430) >> 8)) * 3) >> 9);
        oam_set(i, x, y, 1, 0, OBJT_FAR0 + (i % 3) * 2, 2, 0);
    }
    for (int j = 0; j < 4; j++) {              // near lilac clouds, 16 px/s, in front
        int x0 = j * 70 + (int)hv(j, 5) * 30 / 255, y0 = 20 + (int)hv(j, 6) * 100 / 255;
        int x = ((x0 - ((tq * 16) >> 8)) % 280 + 280) % 280 - 30;
        int y = y0 + ((sin8((tq * 8342 >> 16) + ((x0 * 10430) >> 8)) * 3) >> 9);
        oam_set(9 + j, x, y, 1, 2, OBJT_NEAR0 + ((j + 1) % 3) * 8, 0, 1);
    }
    int ts = tq - 512;                                              // 0 .. ~1440 over the scene
    oam_set(13, 150 - ((tq * 154) >> 16) - 8, 64 - ((ts * 57) >> 12), 0, 1, OBJT_MOON, 0, 2);          // moon climbs 20 px
    oam_set(14, 64 - ((ts * 29) >> 12) - 16, 24 + ((ts * 358) >> 12) - 16, 0, 2, OBJT_SUN, 1, 3);      // sun sinks ~126 px (centre 56,24 -> 54,150), drifting down past the horizon
}
static void scene_init(void) {
    BLACKOUT();
    for (int i = 0; i < 16 * 8; i++) BG_PAL[i] = 0;
    gfx_scene(SB_SKY, SB_FOL, SB_SUN, SB_RIDGE);
    REG_BGCNT(3) = BGCNT(3, 0, SB_SKY);  REG_BGCNT(2) = BGCNT(1, 0, SB_FOL);
    REG_BGCNT(1) = BGCNT(1, 0, SB_SUN);  REG_BGCNT(0) = BGCNT(1, 0, SB_RIDGE);
    REG_BGHOFS(3) = REG_BGVOFS(3) = 0; REG_BGVOFS(0) = 0; REG_BGVOFS(1) = REG_BGVOFS(2) = 0;
    REG_WIN0V = REG_WIN1V = 160;                                       // y 0..160
    REG_WININ  = 0x3E | (0x3C << 8);   // WIN0 (sun): BG1,2,3,OBJ,fx   WIN1 (foliage): BG2,3,OBJ,fx
    REG_WINOUT = 0x39;                 // outside: BG0 (ridge), BG3 (sky), OBJ, fx
    REG_BLDCNT = 0xFF;                 // brightness decrease on every layer + backdrop
    clear_oam();
    cur = 0; build_tables(0, 512);
    REG_DISPCNT = DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_WIN0 | DCNT_WIN1;
}
static void scene_frame(int f) {
    int tq = 512 + 9 * f;                                            // scene time in Q8 seconds (2 s + 2.1x speed)
    int fadein = f < 15 ? 16 - f * 16 / 15 : 0;                      // 0.25 s fade in
    int dim = 12 * clampi((f - 84) * 256 / 78, 0, 256) >> 8;         // dim to night from 1.4 s over 1.3 s
    set_fade(fadein > dim ? fadein : dim);
    hdma(1, tabW0[cur], 0x04000040); hdma(2, tabW1[cur], 0x04000042); hdma(3, tabH0[cur], 0x04000010);
    REG_BGHOFS(1) = REG_BGHOFS(2) = (tq * 22) >> 8;                  // foliage texture scroll
    scene_oam(tq);
    cur ^= 1; build_tables(cur, tq + 9);                             // next frame's tables, while DMA reads the other set
}

// ---- Phase 2: grey screen + scan line ----
static void grey_init(void) {
    BLACKOUT(); dma_off(); clear_oam();
    REG_WININ = 0; REG_WINOUT = 0;
    for (int i = 0; i < 16 * 8; i++) BG_PAL[i] = 0;
    BG_PAL[0] = RGB15(0x2B, 0x29, 0x33);                             // backdrop #2b2933
    gfx_grey();
    for (int i = 0; i < 1024; i++) BG_MAP(SB_SCAN)[i] = 0;
    REG_BGCNT(0) = BGCNT(0, 1, SB_SCAN); REG_BGHOFS(0) = REG_BGVOFS(0) = 0;
    REG_DISPCNT = DCNT_BG0;
}
static void grey_frame(int f) {
    int lf = f - P2;
    int lvl = lf < 15 ? 12 - lf * 12 / 15 : 0;                       // haze clears
    if (f >= 197) lvl = (f - 197) * 16 / 15;                         // cut to black
    set_fade(lvl);
    for (int c = 0; c < 30; c++)                                     // rope-like twist on the left, flat elsewhere (row 10 = y 80)
        BG_MAP(SB_SCAN)[10 * 32 + c] = (c < 9 ? ((lf & 1) ? T_SCAN_TWIST1 : T_SCAN_TWIST0) : T_SCAN_FLAT) | (5 << 12);
}

// ---- Phase 3: text + underline ----
static void text_init(void) {
    BLACKOUT(); dma_off(); clear_oam();
    for (int i = 0; i < 16 * 8; i++) BG_PAL[i] = 0;
    gfx_text(SB_TEXT);                  // backdrop black
    for (int i = 0; i < 1024; i++) BG_MAP(SB_ROPE)[i] = 0;
    REG_BGCNT(0) = BGCNT(0, 1, SB_TEXT); REG_BGCNT(1) = BGCNT(1, 1, SB_ROPE);
    REG_BGHOFS(0) = (u16)(-((240 - TEXT_W) / 2)) & 0x1FF; REG_BGVOFS(0) = (u16)(-(76 - TEXT_H / 2)) & 0x1FF;   // centre text at y=76
    REG_BGHOFS(1) = REG_BGVOFS(1) = 0;
    REG_WIN0V = (88 << 8) | 96;                                      // underline row (tile row 11)
    REG_WININ = 0x22; REG_WINOUT = 0x21;                             // inside: BG1 only, outside: BG0 only
    REG_DISPCNT = DCNT_BG0 | DCNT_BG1 | DCNT_WIN0;
}
static void text_frame(int f) {
    int lf = f - P3;
    int lvl = 0;
    if (lf < 42) lvl = 16 - lf * 16 / 42;                            // 0.7 s text fade-in
    if (lf >= 110 + HOLD_EXTRA) lvl = (lf - 110 - HOLD_EXTRA) * 16 / 36;                       // 0.6 s fade to black
    set_fade(lvl);
    int L = clampi((lf - 18) * 256 / 54, 0, 256), u = 256 - L;       // rope grows after 0.3 s over 0.9 s, cubic ease-out
    int ease = 256 - (((u * u) >> 8) * u >> 8), half = (64 * ease) >> 8;
    REG_WIN0H = ((120 - half) << 8) | (120 + half);
    if (lf % 5 == 0) {                                               // 12 fps rope twist
        int fr = (lf / 5) & 3;
        for (int i = 0; i < ROPE_TILES_W; i++) BG_MAP(SB_ROPE)[11 * 32 + 7 + i] = (T_ROPE_BASE + fr * ROPE_TILES_W + i) | (4 << 12);
    }
}

void logo_play(void) {
    BLACKOUT();
    scene_init();
    for (int f = 0; f < LOGO_FRAMES; f++) {
        vsync();
        if (f > 20 && !(REG_KEYINPUT & KEY_START)) break;      // START skips the logo
        if (f == P2) grey_init();
        if (f == P3) text_init();
        if (f < P2) scene_frame(f); else if (f < P3) grey_frame(f); else text_frame(f);
    }
    vsync(); BLACKOUT(); dma_off(); clear_oam(); BG_PAL[0] = 0;
}
