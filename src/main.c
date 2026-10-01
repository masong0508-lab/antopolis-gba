// EMPIRE-ANTS for Game Boy Advance - port of the GBC game. Raw hardware registers, no libraries.
// Mode 0: BG0 = 32x32 world (scrolls), BG1 = HUD / overlay text, OBJ = ants, queens, cursor.
// All graphics come from art.h (the GBC 2bpp tiles, expanded to 4bpp at boot) and the 3x5 font.
// New on GBA: 30x18 tile view, 24 ants per colony, difficulty levels, game timer, SRAM records, L/R shortcuts.
#include <stdint.h>
#include "art.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t s8; typedef int16_t s16;

// ---------- hardware ----------
#define IO16(a) (*(volatile u16 *)(0x04000000u + (a)))
#define IO8(a)  (*(volatile u8 *)(0x04000000u + (a)))
#define DISPCNT IO16(0x00)
#define VCOUNT  IO16(0x06)
#define BG0CNT  IO16(0x08)
#define BG1CNT  IO16(0x0A)
#define BG0HOFS IO16(0x10)
#define BG0VOFS IO16(0x12)
#define BLDCNT  IO16(0x50)
#define BLDY    IO16(0x54)
#define KEYS    IO16(0x130)
#define SNDL    IO16(0x80)      // NR50 (low byte) / NR51 (high byte)
#define SNDH    IO16(0x82)
#define SNDX    IO16(0x84)
#define NR10 IO8(0x60)
#define NR11 IO8(0x62)
#define NR12 IO8(0x63)
#define NR13 IO8(0x64)
#define NR14 IO8(0x65)
#define NR21 IO8(0x68)
#define NR22 IO8(0x69)
#define NR23 IO8(0x6C)
#define NR24 IO8(0x6D)
#define NR30 IO8(0x70)
#define NR31 IO8(0x72)
#define NR32 IO8(0x73)
#define NR33 IO8(0x74)
#define NR34 IO8(0x75)
#define NR41 IO8(0x78)
#define NR42 IO8(0x79)
#define NR43 IO8(0x7C)
#define NR44 IO8(0x7D)
#define NR50 IO8(0x80)
#define WAVE16 ((volatile u16 *)0x04000090)
#define TILE32  ((volatile u32 *)0x06000000)
#define BGMAP0  ((volatile u16 *)0x0600E000)
#define BGMAP1  ((volatile u16 *)0x0600E800)
#define OBJT32  ((volatile u32 *)0x06010000)
#define BGPAL   ((volatile u16 *)0x05000000)
#define OBJPAL  ((volatile u16 *)0x05000200)
#define OAMR    ((volatile u16 *)0x07000000)
#define SRAM    ((volatile u8 *)0x0E000000)
#define RGB(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))
__attribute__((used)) static const char sram_tag[] = "SRAM_V113";   // tells emulators / flash carts to give us battery save

// keys, in the same bit layout the GBC game used (plus L / R)
#define J_RIGHT 0x01
#define J_LEFT  0x02
#define J_UP    0x04
#define J_DOWN  0x08
#define J_A     0x10
#define J_B     0x20
#define J_SEL   0x40
#define J_START 0x80
#define J_R     0x100
#define J_L     0x200

#define W 32
#define H 32
#define VW 30           // visible world tiles
#define VH 18           // bottom 2 rows are the HUD
#define MAXA 48         // 24 ants per colony
#define QHP 5
#define MANA_MAX 20
#define MANA_PER_FOOD 1
#define OFFER_FOOD 2
#define OFFER_MANA 4
#define FT 4            // font tiles 4..43
#define BT 44           // terrain tiles 44..49, mana bar 50 (full) 51 (empty)
#define BAR_F (BT + 6)
#define BAR_E (BT + 7)

typedef struct { u8 x, y, team, alive, carry, sol; } Ant;
static u8 hgt[H][W], food[H][W], ph[H][W];
static Ant ant[MAXA];
static u8 nestx[2], nesty[2], stock[2], qhp[2], ncnt[2], hatched[2];
static u8 cx, cy, mana, over, camx, camy, sandbox, cheat, ff, appr, diff, slice;
static u16 gt, etk;                      // gt = game ticks (7.5 per second), etk = ticks since the last election
static s16 scx, scy;
static u32 rs = 2463534242u;
static const char *msg; static u8 msgt;
static const s8 DX[4] = {1, -1, 0, 0};
static const s8 DY[4] = {0, 0, 1, -1};
static const u16 CHEAT[10] = {J_LEFT, J_LEFT, J_RIGHT, J_RIGHT, J_UP, J_DOWN, J_UP, J_DOWN, J_B, J_A};
static const u8 RCOST[3] = {4, 3, 2};            // food per red ant: easy, normal, hard
static const u8 TRK[3] = {15, 31, 63};           // mana trickle mask (every 16 / 32 / 64 ticks)
static const char *const DNAME[3] = {"EASY", "NORM", "HARD"};
static u8 tut, tev, tutor, lcx, lcy;             // tutorial: lesson (0 = off), events done, from-title flag, last cursor
static u8 wins[3]; static u16 best[3];           // records (SRAM), best = seconds, 0 = none

static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs >> 8; }

// ---------- input / timing ----------
static u16 joy(void) {
  u16 k = (u16)~KEYS, r = 0;
  if (k & 1) r |= J_A;     if (k & 2) r |= J_B;    if (k & 4) r |= J_SEL;   if (k & 8) r |= J_START;
  if (k & 16) r |= J_RIGHT; if (k & 32) r |= J_LEFT; if (k & 64) r |= J_UP;  if (k & 128) r |= J_DOWN;
  if (k & 256) r |= J_R;   if (k & 512) r |= J_L;
  return r;
}
static void music_update(void);
static void vsync(void) { while (VCOUNT >= 160); while (VCOUNT < 160); music_update(); }

// ---------- sprites (shadow OAM, copied in VBlank) ----------
static u16 oam[128 * 4];
static void spr(u8 s, int x, int y, u8 tile, u8 pal, u8 pri, u8 flip) {
  oam[s * 4] = (u16)(y & 0xFF); oam[s * 4 + 1] = (u16)((x & 0x1FF) | (flip ? 0x1000 : 0));
  oam[s * 4 + 2] = (u16)(tile | (pri << 10) | (pal << 12));
}
static void spr_hide(u8 s) { oam[s * 4] = 0x200; }
static void hide_all(void) { u8 i; for (i = 0; i < 128; i++) spr_hide(i); }
static void oam_flush(void) { u16 i; for (i = 0; i < 128 * 4; i++) OAMR[i] = oam[i]; }

// ---------- tiles ----------
static void put_tile(volatile u32 *d, const u8 *s, const u8 *rm) {   // GBC 2bpp planar -> GBA 4bpp
  u8 y, x, v, lo, hi; u32 row;
  for (y = 0; y < 8; y++) {
    lo = s[y * 2]; hi = s[y * 2 + 1]; row = 0;
    for (x = 0; x < 8; x++) {
      v = (u8)(((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1));
      if (rm) v = rm[v];
      row |= (u32)v << (4 * x);
    }
    d[y] = row;
  }
}
static void fill_tile(volatile u32 *d, u32 v) { u8 y; for (y = 0; y < 8; y++) d[y] = v; }
// 3x5 font, one octal digit per row. order: space, 0-9, A-Z, ':', '!', '/'
static const u16 FONT[40] = {
  0,
  075557,026227,071747,071717,055711,074717,074757,071122,075757,075717,
  025755,065656,034443,065556,074647,074644,034553,055755,072227,011152,
  055655,044447,057755,065555,025552,065644,025563,065655,034216,072222,
  055557,055552,055775,055255,055222,071247,
  002020,022202,011244};
static void mk_glyph(volatile u32 *d, u16 g) {       // background colour 1, ink colour 3
  u8 y, x, lo; u32 row;
  for (y = 0; y < 8; y++) {
    lo = (y >= 1 && y <= 5) ? (u8)(((g >> (3 * (5 - y))) & 7) << 3) : 0;
    row = 0;
    for (x = 0; x < 8; x++) row |= (u32)(((lo >> (7 - x)) & 1) ? 3 : 1) << (4 * x);
    d[y] = row;
  }
}
static u8 gi(char c) {
  if (c >= '0' && c <= '9') return 1 + (c - '0');
  if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
  if (c == ':') return 37;
  if (c == '!') return 38;
  if (c == '/') return 39;
  return 0;
}
// BG1 palette banks: 0 terrain, 1 white text on black, 2 gold, 3 night sky, 4 dark gold
static void pc(u8 x, u8 y, char c, u8 p) { BGMAP1[y * 32 + x] = (u16)((FT + gi(c)) | (p << 12)); }
static void ps(u8 x, u8 y, const char *s, u8 p) { while (*s) pc(x++, y, *s++, p); }
static void pn(u8 x, u8 y, u8 n, u8 p) { if (n > 99) n = 99; pc(x, y, n > 9 ? '0' + n / 10 : ' ', p); pc(x + 1, y, '0' + n % 10, p); }
static void pz(u8 x, u8 y, u8 n, u8 p) { if (n > 99) n = 99; pc(x, y, '0' + n / 10, p); pc(x + 1, y, '0' + n % 10, p); }
static void ov_fill(u16 t, u8 pal, u8 y0, u8 y1) { u8 x, y; for (y = y0; y <= y1; y++) for (x = 0; x < 32; x++) BGMAP1[y * 32 + x] = (u16)(t | (pal << 12)); }
static void ov_clear(void) { ov_fill(0, 0, 0, 31); }          // fully transparent overlay

static void say(const char *m) { msg = m; msgt = 12; }

// ---------- HUD (BG1 rows 18-19) ----------
// row 18: MP nn [bar] F nn  T m:ss        row 19: Q a/b  A nn  R nn  P nn  FF  D NORM   (a hint replaces row 19)
static void hud(void) {
  u8 i, x; u32 s = (u32)gt * 2 / 15;
  ov_fill(1, 1, 18, 19);
  ps(0, 18, "MP", 2); pn(3, 18, mana, 1);
  for (i = 0; i < 10; i++) BGMAP1[18 * 32 + 6 + i] = (u16)((mana > 2 * i ? BAR_F : BAR_E) | (1 << 12));
  ps(18, 18, "F", 2); pn(19, 18, stock[0], 1);
  ps(23, 18, "T", 2); pn(24, 18, (u8)(s / 60 > 99 ? 99 : s / 60), 1); pc(26, 18, ':', 1); pz(27, 18, (u8)(s % 60), 1);
  if (msgt) {
    x = 0; for (const char *m = msg; *m && x < 30; m++) pc(x++, 19, *m, 1);
    msgt--; return;
  }
  ps(0, 19, "Q", 2); pc(1, 19, '0' + qhp[0], 1); pc(2, 19, '/', 1); pc(3, 19, '0' + qhp[1], 1);
  ps(6, 19, "A", 2); pn(7, 19, ncnt[0], 1); ps(10, 19, "R", 2); pn(11, 19, ncnt[1], 1);
  ps(14, 19, "P", 2); pn(15, 19, appr, 1);
  if (ff) ps(19, 19, "FF", 2);
  ps(23, 19, "D", 2); ps(25, 19, DNAME[diff], 1);
}

// ---------- help card ----------
static const char HELP[] =
  "#CONTROLS\n"
  "A B    RAISE / LOWER LAND 1MP\n"
  "L      FLOOD 3X3 (8 MP)\n"
  "SEL A  EMBEZZLE 2 FOOD > 4 MP\n"
  "R      FAST FORWARD\n"
  "START  PAUSE / RESUME\n"
  "\n"
  "#GOAL\n"
  "KILL THE RED QUEEN BEFORE\n"
  "THEY KILL YOURS\n"
  "\n"
  "#HOW\n"
  "ANTS CANT CLIMB CLIFFS OR\n"
  "SWIM: BUILD BRIDGES, CUT\n"
  "PATHS, FLOOD THE ENEMY\n"
  "MANA: TRICKLE, FOOD HOME,\n"
  "ELECTION AID (P 50 UP)\n"
  "P UNDER 25 = COUP\n";
static void help_show(void) {
  const char *s = HELP; u8 y = 1, x, g;
  hide_all(); vsync(); oam_flush();
  ov_fill(1, 1, 0, 19);
  while (*s) {
    g = 0; if (*s == '#') { g = 1; s++; }
    x = 0; while (*s && *s != '\n') pc(x++, y, *s++, g ? 2 : 1);
    if (*s) s++;
    y++;
  }
  ps(0, 19, "START: BACK", 2);
  while (joy()) vsync();
  while (!(joy() & J_START)) vsync();
  while (joy()) vsync();
  ov_clear();
}

// ---------- sound (GBA legacy PSG channels, same registers as the GB) ----------
#define N_C4 1548
#define N_G4 1714
#define N_C5 1797
#define N_E5 1849
#define N_G5 1881
#define N_C6 1923
static const u16 WIN_TUNE[6]  = {N_C5, N_E5, N_G5, N_C6, N_G5, N_C6};
static const u16 LOSE_TUNE[4] = {N_G5, N_E5, N_C5, N_C4};
static const u16 *tune_p; static u8 jl, ji, jt;
static u8 h1, h2, hn;
static void ch1(u8 sweep, u16 f, u8 env) { h1 = 14; NR10 = sweep; NR11 = 0x80; NR12 = env; NR13 = (u8)f; NR14 = (u8)(0x80 | (f >> 8)); }
static void ch2(u16 f, u8 env) { h2 = 12; NR21 = 0x80; NR22 = env; NR23 = (u8)f; NR24 = (u8)(0x80 | (f >> 8)); }
static void noise(u8 env, u8 poly) { hn = 16; NR41 = 0; NR42 = env; NR43 = poly; NR44 = 0x80; }
static void sfx_raise(void) { ch1(0x15, N_C5, 0xA1); }
static void sfx_lower(void) { ch1(0x1D, N_G4, 0xA1); }
static void sfx_deny(void)  { ch1(0x00, N_C4, 0x81); }
static void sfx_mana(void)  { ch1(0x16, N_G5, 0x91); }
static void sfx_flood(void) { noise(0xA3, 0x55); }
static void sfx_food(void)  { ch2(N_E5, 0x71); }
static void sfx_spawn(void) { ch2(N_C6, 0x81); }
static void sfx_fight(void) { noise(0x81, 0x33); }
static void sfx_hit(void)   { noise(0xC2, 0x44); }
static void sfx_qdead(void) { noise(0xF7, 0x77); }
static void jingle(const u16 *n, u8 len) { tune_p = n; ji = 0; jt = 0; jl = len; }

static const u16 NOTE[64] = {
  44, 157, 263, 363, 457, 547, 631, 711, 786, 856, 923, 986, 1046, 1102, 1155, 1205,
  1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517, 1547, 1575, 1602, 1627, 1650, 1673, 1694, 1714,
  1732, 1750, 1767, 1783, 1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915,
  1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982, 1985, 1989, 1992, 1995};
static const u8 LEAD[64] = {
  40,0,45,0,43,40,36,0,   41,0,45,0,48,45,41,0,   40,43,40,36,38,40,43,0,   38,0,43,0,47,0,45,43,
  40,0,45,0,48,47,45,40,  41,45,48,0,45,41,40,41, 38,43,47,50,47,43,38,0,    40,44,47,0,47,44,40,0};
static const u8 BROOT[8] = {9, 5, 12, 7, 9, 5, 7, 4};
static const u8 BPAT[8]  = {0, 0, 12, 0, 7, 0, 12, 7};
static const u8 ARPN[8][3] = {{21,24,28},{17,21,24},{24,28,31},{19,23,26},{21,24,28},{17,21,24},{19,23,26},{16,20,23}};
static const u8 ARPO[4]  = {0, 1, 2, 1};
static const u8 DPOLY[8] = {0x75,0x21,0x43,0x21,0x75,0x21,0x43,0x21};
static const u8 DENV[8]  = {0xA1,0x41,0x81,0x41,0xA1,0x41,0x81,0x41};
static const u8 WAVE[16] = {0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
static u8 mus_on, mt, ms;
// GBA wave RAM: with NR30 bit 5 = 0 bank 0 plays and writes go to bank 1; then select bank 1 (0x20) + DAC on (0x80)
static void music_start(void) {
  u8 i;
  NR30 = 0x00;
  for (i = 0; i < 8; i++) WAVE16[i] = (u16)(WAVE[2 * i] | (WAVE[2 * i + 1] << 8));
  NR30 = 0xA0;
  mt = 7; ms = 0; mus_on = 1;
}
static void music_stop(void) { mus_on = 0; NR12 = 0; NR22 = 0; NR42 = 0; NR30 = 0; }
static void music_update(void) {                 // once per frame, called from vsync()
  u8 b, e, n; u16 f;
  if (h1) h1--; if (h2) h2--; if (hn) hn--;
  if (ji < jl) { if (jt == 0) { ch2(tune_p[ji++], 0xB2); jt = 10; } else jt--; }
  if (!mus_on || ++mt < 8) return;
  mt = 0; b = ms >> 4;
  if (!h1) { f = NOTE[ARPN[b][ARPO[ms & 3]]]; NR10 = 0; NR11 = 0x40; NR12 = 0x42; NR13 = (u8)f; NR14 = (u8)(0x80 | (f >> 8)); }
  if (!(ms & 1)) {
    e = (ms & 15) >> 1;
    f = NOTE[BROOT[b] + BPAT[e] + 12];
    NR30 = 0xA0; NR31 = 0xC8; NR32 = 0x40; NR33 = (u8)f; NR34 = (u8)(0xC0 | (f >> 8));
    n = LEAD[(b << 3) + e];
    if (n && !h2) { f = NOTE[n]; NR21 = 0x80; NR22 = 0x83; NR23 = (u8)f; NR24 = (u8)(0x80 | (f >> 8)); }
    if (!hn) { NR41 = 0; NR42 = DENV[e]; NR43 = DPOLY[e]; NR44 = 0x80; }
  }
  ms = (ms + 1) & 127;
}

// ---------- fade (hardware brightness-down, plus master volume) ----------
static void fade(u8 from, u8 to) {               // 0 = full colour, 16 = black
  u8 l = from, k, v;
  for (;;) {
    BLDY = l; v = (u8)(7 - (l * 7) / 16); NR50 = (u8)((v << 4) | v);
    for (k = 0; k < 2; k++) vsync();
    if (l == to) break;
    if (l < to) l++; else l--;
  }
}

// ---------- records (SRAM) ----------
static void save_write(void) {
  u8 i, s = 0;
  SRAM[0] = 'E'; SRAM[1] = 'A'; SRAM[2] = 'N'; SRAM[3] = 'T';
  for (i = 0; i < 3; i++) { SRAM[4 + i] = wins[i]; SRAM[7 + 2 * i] = (u8)(best[i] & 255); SRAM[8 + 2 * i] = (u8)(best[i] >> 8); }
  for (i = 4; i < 13; i++) s = (u8)(s + SRAM[i]);
  SRAM[13] = s;
}
static void save_load(void) {
  u8 i, s = 0;
  for (i = 4; i < 13; i++) s = (u8)(s + SRAM[i]);
  if (SRAM[0] == 'E' && SRAM[1] == 'A' && SRAM[2] == 'N' && SRAM[3] == 'T' && SRAM[13] == s) {
    for (i = 0; i < 3; i++) { wins[i] = SRAM[4 + i]; best[i] = (u16)(SRAM[7 + 2 * i] | (SRAM[8 + 2 * i] << 8)); }
  } else { for (i = 0; i < 3; i++) { wins[i] = 0; best[i] = 0; } save_write(); }
}

// ---------- world ----------
static u8 is_nest(u8 x, u8 y) { return (x == nestx[0] && y == nesty[0]) || (x == nestx[1] && y == nesty[1]); }
static void draw_cell(u8 x, u8 y) {
  u8 t = hgt[y][x];
  if (is_nest(x, y)) t = 4; else if (food[y][x] && t) t = 5;
  BGMAP0[y * 32 + x] = (u16)(BT + t);
}
static u8 can_go(u8 x, u8 y, s8 dx, s8 dy) {
  s8 a = (s8)x + dx, b = (s8)y + dy, d;
  if (a < 0 || b < 0 || a >= W || b >= H) return 0;
  if (hgt[b][a] == 0) return 0;
  d = (s8)hgt[b][a] - (s8)hgt[y][x];
  return d >= -1 && d <= 1;
}
static u8 spawn(u8 team) {
  u8 i, n = 0;
  for (i = 0; i < MAXA; i++) if (ant[i].alive && ant[i].team == team) n++;
  if (n >= MAXA / 2) return 0;
  for (i = 0; i < MAXA; i++) if (!ant[i].alive) {
    ant[i].x = nestx[team]; ant[i].y = nesty[team]; ant[i].team = team; ant[i].alive = 1; ant[i].carry = 0;
    ant[i].sol = ((hatched[team]++ & 3) == 0);
    return 1;
  }
  return 0;
}
static u8 dist(u8 a, u8 b) { return a > b ? a - b : b - a; }
static void count(u8 *c) { u8 i; c[0] = c[1] = 0; for (i = 0; i < MAXA; i++) if (ant[i].alive) c[ant[i].team]++; }

static void apr(s8 d) { s16 v = (s16)appr + d; appr = v < 0 ? 0 : v > 99 ? 99 : (u8)v; }
static void die(Ant *a) { a->alive = 0; if (a->team == 0) apr(-2); }
static void election(void) {
  if (appr >= 50) { mana = (mana + 8 > MANA_MAX) ? MANA_MAX : mana + 8; sfx_mana(); say("ELECTION WON! AID"); }
  else if (appr >= 25) { sfx_deny(); say("ELECTION: NO BONUS"); }
  else { stock[0] >>= 1; mana = 0; appr = 40; sfx_qdead(); say("COUP! COFFERS LOOTED"); }
}
static void step_ant(Ant *a) {
  u8 d, t = a->team, e = t ^ 1, found = 0, bd = 0, tx, ty, de;
  s16 sc, best_sc = -32000;
  u8 soldier = !a->carry && qhp[e] && ncnt[t] >= 8 && a->sol;
  if (!a->carry && food[a->y][a->x]) { food[a->y][a->x] = 0; a->carry = 1; draw_cell(a->x, a->y); }
  if (a->carry) { u8 p = ph[a->y][a->x]; ph[a->y][a->x] = p > 215 ? 255 : p + 40; }
  for (d = 0; d < 4; d++) {
    if (!can_go(a->x, a->y, DX[d], DY[d])) continue;
    tx = a->x + DX[d]; ty = a->y + DY[d];
    if (a->carry) sc = -10 * (s16)(dist(tx, nestx[t]) + dist(ty, nesty[t])) + (rnd() & 15);
    else if (soldier) { de = dist(tx, nestx[e]) + dist(ty, nesty[e]); sc = -14 * (s16)de + (rnd() & 31); }
    else sc = ph[ty][tx] + (food[ty][tx] ? 120 : 0) + (rnd() & 31);
    if (sc > best_sc) { best_sc = sc; bd = d; found = 1; }
  }
  if (found) { a->x += DX[bd]; a->y += DY[bd]; }
  if (a->carry && a->x == nestx[t] && a->y == nesty[t]) {
    a->carry = 0; if (stock[t] < 99) stock[t]++;
    if (t == 0) { mana = (mana + MANA_PER_FOOD > MANA_MAX) ? MANA_MAX : mana + MANA_PER_FOOD; apr(1); sfx_food(); }
  }
  if (!a->carry && qhp[e] && a->x == nestx[e] && a->y == nesty[e]) {
    if (a->sol) { if (rnd() & 1) { qhp[e]--; if (e == 0) apr(-3); if (qhp[e]) sfx_hit(); else sfx_qdead(); } }
    else if (stock[e]) { stock[e]--; a->carry = 1; sfx_fight(); }
  }
}
static void tick_slice(void) {                    // 1/8 of the ants per call; every 8th call runs the colony-level work
  u8 i, j, x, y, cst;
  for (i = slice; i < MAXA; i += 8) {
    if (!ant[i].alive) continue;
    if (hgt[ant[i].y][ant[i].x] == 0) { die(&ant[i]); continue; }
    step_ant(&ant[i]);
  }
  for (i = slice; i < MAXA; i += 8)
    if (ant[i].alive) for (j = i + 1; j < MAXA; j++)
      if (ant[j].alive && ant[i].team != ant[j].team && ant[i].x == ant[j].x && ant[i].y == ant[j].y)
        { if (rnd() & 1) die(&ant[i]); else die(&ant[j]); sfx_fight(); if (!ant[i].alive) break; }
  if ((gt & 3) == 0) for (y = slice << 2; y < (u8)((slice << 2) + 4); y++) for (x = 0; x < W; x++) if (ph[y][x]) ph[y][x]--;
  if (++slice < 8) return;
  slice = 0; gt++;
  count(ncnt);
  for (i = 0; i < 2; i++) {
    if (!qhp[i]) continue;
    cst = i ? RCOST[diff] : 3;
    if (stock[i] >= cst && spawn(i)) { stock[i] -= cst; if (i == 0) { sfx_spawn(); apr(1); } }
    else if (ncnt[i] == 0 && (gt & 31) == 0) spawn(i);
    if ((gt & 127) == 0 && qhp[i] < QHP) qhp[i]++;
  }
  if ((gt & 31) == 0) apr(!stock[0] && ncnt[0] ? -2 : -1);
  if (++etk >= 450) { etk = 0; election(); }
  if ((gt & 7) == 0) for (i = 0; i < 2; i++) {
    x = rnd() & (W - 1); y = rnd() & (H - 1);
    if (hgt[y][x] && !food[y][x] && !is_nest(x, y)) { food[y][x] = 1; draw_cell(x, y); }
  }
  if ((gt & TRK[diff]) == 0 && mana < MANA_MAX) mana++;
}
static void bump(u8 cx0, u8 cy0, u8 v, u8 r) {
  s8 x, y;
  for (y = (s8)cy0 - r; y <= (s8)cy0 + r; y++) for (x = (s8)cx0 - r; x <= (s8)cx0 + r; x++)
    if (x >= 0 && y >= 0 && x < W && y < H) hgt[y][x] = v;
}
static void smooth(void) {
  u8 pass, x, y, a, b;
  for (pass = 0; pass < 3; pass++) for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    a = hgt[y][x]; if (!a) continue;
    if (x + 1 < W) { b = hgt[y][x + 1]; if (b) { if (a > b + 1) hgt[y][x] = b + 1; else if (b > a + 1) hgt[y][x + 1] = a + 1; } }
    a = hgt[y][x];
    if (y + 1 < H) { b = hgt[y + 1][x]; if (b) { if (a > b + 1) hgt[y][x] = b + 1; else if (b > a + 1) hgt[y + 1][x] = a + 1; } }
  }
}

// ---------- camera + sprites ----------
static void follow(void) {
  u8 t;
  if (cx < camx + 3) camx = cx > 3 ? cx - 3 : 0;
  else if (cx + 3 >= camx + VW) { t = cx + 4 - VW; camx = t > W - VW ? W - VW : t; }
  if (cy < camy + 3) camy = cy > 3 ? cy - 3 : 0;
  else if (cy + 3 >= camy + VH) { t = cy + 4 - VH; camy = t > H - VH ? H - VH : t; }
}
static void scroll_step(void) {
  s16 tx = (s16)camx * 8, ty = (s16)camy * 8;
  if (scx < tx) { scx += 4; if (scx > tx) scx = tx; } else if (scx > tx) { scx -= 4; if (scx < tx) scx = tx; }
  if (scy < ty) { scy += 4; if (scy > ty) scy = ty; } else if (scy > ty) { scy -= 4; if (scy < ty) scy = ty; }
}
static void place(u8 s, u8 tile, u8 pal, u8 x, u8 y) {
  int sx = (int)x * 8 - scx, sy = (int)y * 8 - scy;
  if (sx > -8 && sx < 240 && sy > -8 && sy < 144) spr(s, sx, sy, tile, pal, 1, 0); else spr_hide(s);
}
static void draw_sprites(void) {
  u8 i;
  for (i = 0; i < MAXA; i++) { if (ant[i].alive) place(i, 0, ant[i].team, ant[i].x, ant[i].y); else spr_hide(i); }
  for (i = 0; i < 2; i++) { if (qhp[i]) place(MAXA + i, 2, i, nestx[i], nesty[i]); else spr_hide(MAXA + i); }
  place(MAXA + 2, 1, 2, cx, cy);
}

// ---------- new game ----------
static void newgame(void) {
  u8 i, k, x, y;
  DISPCNT = 0x80; BLDY = 16;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { hgt[y][x] = 1; food[y][x] = 0; ph[y][x] = 0; }
  for (k = 0; k < 28; k++) { x = rnd() & (W - 1); y = rnd() & (H - 1); bump(x, y, 2, 1); hgt[y][x] = 3; }
  for (k = 0; k < 9; k++) {
    x = rnd() & (W - 1); y = rnd() & (H - 1);
    hgt[y][x] = 0;
    if (x + 1 < W) hgt[y][x + 1] = 0;
    if (y + 1 < H) hgt[y + 1][x] = 0;
    if (x + 1 < W && y + 1 < H) hgt[y + 1][x + 1] = 0;
  }
  nestx[0] = 4; nesty[0] = 26; nestx[1] = 27; nesty[1] = 5;
  bump(nestx[0], nesty[0], 1, 1); bump(nestx[1], nesty[1], 1, 1);
  smooth();
  for (k = 0; k < 36; k++) { x = rnd() & (W - 1); y = rnd() & (H - 1); if (hgt[y][x] && !is_nest(x, y)) food[y][x] = 1; }
  for (i = 0; i < MAXA; i++) ant[i].alive = 0;
  stock[0] = stock[1] = 0; qhp[0] = qhp[1] = QHP; hatched[0] = hatched[1] = 0;
  for (k = 0; k < 3; k++) { spawn(0); spawn(1); }
  count(ncnt);
  ff = 0; appr = 50; etk = 0; slice = 0; cheat = 0; cx = nestx[0]; cy = nesty[0] - 2; mana = 10; gt = 0; over = 0; msgt = 0;
  camx = 0; camy = H - VH; scx = 0; scy = (s16)camy * 8;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) draw_cell(x, y);
  ov_clear(); hud();
  say(sandbox ? "SANDBOX: NO LIMITS" : "A/B LAND  START HELP");
  BG0HOFS = (u16)scx; BG0VOFS = (u16)scy;
  hide_all(); draw_sprites(); oam_flush();
  DISPCNT = 0x1340;
  music_start();
  fade(16, 0);
}

static void raise_land(void) {
  if (mana && hgt[cy][cx] < 3 && !is_nest(cx, cy)) { hgt[cy][cx]++; mana--; draw_cell(cx, cy); sfx_raise(); tev |= 2; return; }
  sfx_deny(); say(is_nest(cx, cy) ? "NEST CANT BE EDITED" : hgt[cy][cx] >= 3 ? "ALREADY HIGHEST" : "NEED MANA");
}
static void lower_land(void) {
  if (mana && hgt[cy][cx] > 0 && !is_nest(cx, cy)) { hgt[cy][cx]--; mana--; draw_cell(cx, cy); sfx_lower(); tev |= 4; return; }
  sfx_deny(); say(is_nest(cx, cy) ? "NEST CANT BE EDITED" : hgt[cy][cx] == 0 ? "ALREADY WATER" : "NEED MANA");
}
static void offering(void) {
  if (stock[0] < OFFER_FOOD) { sfx_deny(); say("NEED 2 FOOD"); return; }
  if (mana >= MANA_MAX) { sfx_deny(); say("MANA IS FULL"); return; }
  stock[0] -= OFFER_FOOD; mana = (mana + OFFER_MANA > MANA_MAX) ? MANA_MAX : mana + OFFER_MANA;
  apr(-5); sfx_mana(); say("EMBEZZLED! P DOWN");
}
static void flood(void) {
  s8 x, y;
  if (mana < 8) { sfx_deny(); say("FLOOD NEEDS 8 MANA"); return; }
  mana -= 8; sfx_flood(); tev |= 8;
  for (y = (s8)cy - 1; y <= (s8)cy + 1; y++) for (x = (s8)cx - 1; x <= (s8)cx + 1; x++)
    if (x >= 0 && y >= 0 && x < W && y < H && hgt[y][x] && !is_nest(x, y)) { hgt[y][x]--; draw_cell(x, y); }
}

// ---------- title ----------
static void logo_line(const char *s, u8 x0, u8 y0) {   // blocky 3D lettering from the font: dark shadow layer, then gold face
  u8 i, gx, gy, k; u16 g;
  for (k = 2; k--; )
    for (i = 0; s[i]; i++) {
      g = FONT[gi(s[i])];
      for (gy = 0; gy < 5; gy++) for (gx = 0; gx < 3; gx++)
        if ((g >> (3 * (4 - gy))) & (4 >> gx)) BGMAP1[(y0 + gy + k) * 32 + x0 + i * 4 + gx + k] = (u16)(2 | ((k ? 4 : 2) << 12));
    }
}
static void title_dyn(void) {
  u8 x; u16 b = best[diff];
  ov_fill(1, 3, 15, 15); ov_fill(1, 3, 2, 2);
  ps(10, 15, "<      >", 3 + 0); ps(12, 15, DNAME[diff], 2);
  if (wins[diff]) { ps(5, 2, "WINS", 2); pn(10, 2, wins[diff], 3); ps(14, 2, "BEST", 2); pn(19, 2, (u8)(b / 60 > 99 ? 99 : b / 60), 3); pc(21, 2, ':', 3); pz(22, 2, (u8)(b % 60), 3); }
  else ps(8, 2, "NO WINS YET", 3);
  for (x = 0; x < 30; x++) if (BGMAP1[15 * 32 + x] == (u16)((FT + gi('<')) | (3 << 12))) BGMAP1[15 * 32 + x] = 1 | (3 << 12);
}
static void title_scene(void) {
  u8 x, k;
  ov_fill(1, 3, 0, 19);
  for (k = 0; k < 30; k++) BGMAP1[((rnd() % 13) + 1) * 32 + rnd() % 30] = (u16)(3 | (3 << 12));
  for (x = 0; x < 32; x++) { BGMAP1[18 * 32 + x] = (u16)(BT + 2); BGMAP1[19 * 32 + x] = (u16)(BT + 1); }
  ps(7, 1, "THE RULER OF YOU", 2); ps(7, 0, "A: LEARN TO PLAY", 2);
  logo_line("EMPIRE", 3, 3); logo_line("ANTS", 7, 9);
  title_dyn();
}
static u8 wx[5];
static void title(void) {
  u8 i, fr = 0, on = 255, bl; u16 k, p, prev = 0;
  DISPCNT = 0x80; BLDY = 16;
  ov_clear(); title_scene();
  wx[0] = 14; wx[1] = 62; wx[2] = 112; wx[3] = 200; wx[4] = 150;
  hide_all(); oam_flush();
  DISPCNT = 0x1340;
  music_start();
  fade(16, 0);
  for (;;) {
    vsync(); oam_flush();
    fr++; rs ^= fr + (u32)VCOUNT * 2654435761u; if (!rs) rs = 1;
    k = joy(); p = k & ~prev; prev = k;
    if (p & (J_L | J_RIGHT)) { diff = diff == 2 ? 0 : diff + 1; title_dyn(); }
    if (p & (J_R | J_LEFT))  { diff = diff == 0 ? 2 : diff - 1; title_dyn(); }
    if (p & J_A) { tutor = 1; sandbox = 0; break; }          // A alone = guided tutorial
    if (p & J_START) { tutor = 0; sandbox = (k & J_SEL) ? 1 : 0; break; }
    bl = (fr & 63) < 44;
    if (bl != on) { on = bl; ps(9, 16, on ? "PRESS START" : "           ", 2); }
    for (i = 0; i < 5; i++) {                      // 3 black ants march right, 2 red ants left, along the grass
      if (!(fr & 1)) { if (i < 3) { if (++wx[i] >= 248) wx[i] = 0; } else wx[i] = wx[i] ? wx[i] - 1 : 247; }
      spr(i, (int)wx[i] - 8, 136, (u8)(3 + (((fr >> 3) + i) & 1)), i < 3 ? 0 : 1, 0, 0);
    }
  }
  fade(0, 16);
  music_stop();
  hide_all(); oam_flush();
  ov_clear();
  while (joy()) vsync();
}

// ---------- ANT EYE: first-person 3D view from a black ant (SELECT+START) ----------
// BG1 rows 0-15 (30 x 16 tiles = 240x128 px) are redrawn from a ray march over the heightmap, one ray per tile column.
// Pixel classes are written straight as 4bpp colour indices of palette bank 5: 1 sky, 2/3 checker land, 4 hill, 5 water, 6 red, 7 black.
#define VX 30
#define VR 16
#define EV 64                                         // first view tile (480 tiles)
#define VN 24                                         // ray steps, half a cell each
static const s8 SIN16[16] = {0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59, -64, -59, -45, -24};
static const u8 BBH[VN + 1] = {0, 40, 32, 21, 16, 12, 10, 9, 8, 7, 6, 5, 5, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2};
static const char *const COMPASS[8] = {"E ", "SE", "S ", "SW", "W ", "NW", "N ", "NE"};
static s8 OFF[7][VN + 1];
static u8 occ[H][W], cls[VR * 8], vang;
static u32 ebuf[3][VR * 8];                           // rendered columns waiting for VBlank
static void view_init(void) {
  u8 d, n; s16 v;
  for (d = 0; d < 7; d++) for (n = 1; n <= VN; n++) {
    v = (s16)((((s16)d - 3) * 4 - 3) * 21) / n;
    OFF[d][n] = v > 80 ? 80 : v < -80 ? -80 : (s8)v;
  }
}
static void view_col(u8 c, u8 vx, u8 vy, u8 eh) {
  s16 t = (s16)(((s16)c * 2 - 29) * 2 / 3), px = (s16)vx * 256 + 128, py = (s16)vy * 256 + 128, yt, a, e, r, rx, ry;
  u8 n, x, y, h, o, col, lim = VR * 8, lb;
  rx = (s16)((s16)SIN16[(vang + 4) & 15] * 32 - (s16)SIN16[vang] * t);
  ry = (s16)((s16)SIN16[vang] * 32 + (s16)SIN16[(vang + 4) & 15] * t);
  rx >>= 4; ry >>= 4;
  for (n = 0; n < VR * 8; n++) cls[n] = 1;
  for (n = 1; n <= VN && lim; n++) {
    px += rx; py += ry;
    if (px < 0 || py < 0 || px >= W * 256 || py >= H * 256) break;
    x = (u8)(px >> 8); y = (u8)(py >> 8);
    h = hgt[y][x];
    yt = 64 - OFF[h + 3 - eh][n];
    lb = lim;
    if (yt < lb) {
      a = yt < 0 ? 0 : yt;
      col = h == 0 ? 5 : h == 3 ? 4 : ((x + y) & 1) ? 2 : 3;
      for (r = a; r < lb; r++) cls[r] = col;
      lim = (u8)a;
    }
    o = occ[y][x];
    if (o && !(x == vx && y == vy) && (u8)(px - 64) < 128 && (u8)(py - 64) < 128) {
      a = (BBH[n] * 4 + 2) / 3; if (o > 2) a <<= 1;
      a = yt - a;
      e = yt < lb ? yt : lb;
      if (a < 0) a = 0;
      col = (o & 1) ? 7 : 6;
      for (r = a; r < e; r++) cls[r] = col;
      if (a < lim) lim = (u8)a;
    }
  }
}
static void occ_set(u8 on) {
  u8 i;
  for (i = 0; i < MAXA; i++) if (ant[i].alive) occ[ant[i].y][ant[i].x] = on ? ant[i].team + 1 : 0;
  for (i = 0; i < 2; i++) if (qhp[i]) occ[nesty[i]][nestx[i]] = on ? 3 + i : 0;
}
static void eye_status(u8 vx, u8 vy) {
  ov_fill(1, 1, 18, 19);
  ps(0, 18, "X", 2); pn(1, 18, vx, 1); ps(5, 18, "Y", 2); pn(6, 18, vy, 1);
  ps(10, 18, "H", 2); pc(11, 18, '0' + hgt[vy][vx], 1); ps(14, 18, "FACING", 2); ps(21, 18, COMPASS[vang >> 1], 1);
  ps(0, 19, "A NEXT B GO HERE START BACK", 1);
}
static void eye_step(u8 *vx, u8 *vy, s8 dir) {
  s8 fx = SIN16[(vang + 4) & 15], fy = SIN16[vang], dx = 0, dy = 0;
  if ((fx < 0 ? -fx : fx) >= (fy < 0 ? -fy : fy)) dx = fx < 0 ? -1 : 1; else dy = fy < 0 ? -1 : 1;
  dx *= dir; dy *= dir;
  if (can_go(*vx, *vy, dx, dy)) { *vx += dx; *vy += dy; }
}
static u8 vok(u8 i) { return i < MAXA ? (ant[i].alive && ant[i].team == 0) : qhp[0] > 0; }
static void eye(void) {                              // time stands still while you look through a black ant's eyes
  u8 i, x, y, k2, vi = MAXA, best = 255, d, rep = 0, go = 1, vx = 0, vy = 0, vcol = VX, dirs, last = 0, np = 0, pc0 = 0;
  u16 k, p, prev;
  tev |= 16;
  for (i = 0; i < MAXA; i++) if (vok(i)) { d = (u8)(dist(ant[i].x, cx) + dist(ant[i].y, cy)); if (d < best) { best = d; vi = i; } }
  hide_all(); vsync(); oam_flush();
  BGPAL[81] = RGB(10,18,28); BGPAL[82] = RGB(11,26,9); BGPAL[83] = RGB(4,16,6); BGPAL[84] = RGB(21,14,6);
  BGPAL[85] = RGB(4,10,31);  BGPAL[86] = RGB(31,6,4);  BGPAL[87] = RGB(2,2,2);
  for (x = 0; x < VX; x++) for (y = 0; y < VR; y++) BGMAP1[y * 32 + x] = (u16)((EV + x * VR + y) | (5 << 12));
  for (y = 16; y < 20; y++) for (x = 0; x < 32; x++) BGMAP1[y * 32 + x] = 1 | (1 << 12);
  occ_set(1);
  while (joy()) vsync();
  prev = 0;
  for (;;) {
    if (go) {
      if (go == 1) { if (vi < MAXA) { vx = ant[vi].x; vy = ant[vi].y; } else { vx = nestx[0]; vy = nesty[0]; } }
      go = 0; vcol = 0; eye_status(vx, vy);
    }
    vsync();
    for (k2 = 0; k2 < np; k2++) {                    // copy last frame's columns into VRAM (we are in VBlank)
      volatile u32 *dst = TILE32 + (EV + (pc0 + k2) * VR) * 8;
      for (x = 0; x < VR * 8; x++) dst[x] = ebuf[k2][x] * 0x11111111u;
    }
    np = 0;
    if (vcol < VX) {                                 // 3 columns per frame: the view sweeps in, input stays live
      pc0 = vcol;
      for (k2 = 0; k2 < 3 && vcol < VX; k2++, vcol++) {
        view_col(vcol, vx, vy, hgt[vy][vx]);
        for (x = 0; x < VR * 8; x++) ebuf[k2][x] = cls[x];
        np++;
      }
    }
    k = joy(); p = k & ~prev; prev = k;
    if (p & J_START) break;
    if (p & J_B) { cx = vx; cy = vy; break; }
    if (p & J_A) { i = 0; do { vi = (vi == MAXA) ? 0 : vi + 1; } while (!vok(vi) && ++i <= MAXA); sfx_food(); go = 1; }
    dirs = (u8)(k & 15);
    if (dirs != last) { rep = 0; last = dirs; }
    if (dirs) {
      if (rep == 0 || (rep >= 8 && !(rep & 3))) {
        if (dirs & J_RIGHT) { vang = (vang + 1) & 15; go = 2; }
        else if (dirs & J_LEFT) { vang = (vang + 15) & 15; go = 2; }
        else { x = vx; y = vy; eye_step(&vx, &vy, (dirs & J_UP) ? 1 : -1); if (vx != x || vy != y) go = 2; }
      }
      if (rep < 250) rep++;
    }
  }
  occ_set(0);
  ov_clear(); hud();
  while (joy()) vsync();
}

// ---------- TUTORIAL: lessons (title screen: press A) ----------
// Each lesson is a full-screen card (START next, B skip); lessons with a task then show a live hint on the HUD until you do it.
#define TN 10
static const u8 TEV[TN] = {0, 1, 2, 4, 0, 8, 16, 0, 0, 0};          // event bit that completes each lesson (0 = read only)
static const char *const THINT[TN] = {0, "TRY: MOVE THE CURSOR", "TRY: PRESS A TO RAISE", "TRY: PRESS B TO LOWER", 0,
  "PRESS L TO FLOOD", "SEL+START: ANT EYE", 0, 0, 0};
static const char *const TCARD[TN] = {
  "WELCOME RULER!\n"
  "YOU RULE THE BLACK ANTS OF\n"
  "EMPIRE ANTS. YOU CANT GIVE\n"
  "THEM ORDERS: INSTEAD YOU\n"
  "SHAPE THE LAND AND THEY\n"
  "WALK AROUND IT\n"
  "\n"
  "GOAL: KILL THE RED QUEEN\n"
  "BEFORE THEY KILL YOURS\n",
  "1/8 THE CURSOR\n"
  "THE YELLOW FRAME IS YOUR\n"
  "CURSOR: LAND TOOLS WORK ON\n"
  "THE TILE UNDER IT\n"
  "\n"
  "D PAD MOVES IT, HOLD TO\n"
  "REPEAT. THE MAP SCROLLS\n"
  "NEAR THE EDGE\n"
  "\n"
  "NOW TRY IT!\n",
  "2/8 RAISE LAND\n"
  "LAND HAS 4 HEIGHTS:\n"
  "0 WATER  1 SAND\n"
  "2 GRASS  3 HILL\n"
  "\n"
  "A RAISES THE TILE UNDER THE\n"
  "CURSOR, COST 1 MANA. ANTS\n"
  "CANT CLIMB MORE THAN 1 STEP:\n"
  "BUILD RAMPS AND STAIRS!\n"
  "NESTS CANT BE EDITED\n",
  "3/8 LOWER LAND\n"
  "B LOWERS THE TILE, COST 1 MANA\n"
  "\n"
  "LEVEL 0 IS WATER: ANTS CANT\n"
  "WALK ON IT AND DROWN IF\n"
  "FLOODED. DIG MOATS TO STOP\n"
  "RED ANTS, RAISE LAND TO\n"
  "BRIDGE GAPS\n",
  "4/8 MANA\n"
  "MP IS YOUR MANA: EVERY EDIT\n"
  "COSTS MP\n"
  "\n"
  "MP COMES FROM:\n"
  " SLOW TRICKLE\n"
  " FOOD CARRIED HOME\n"
  " ELECTION AID\n"
  "\n"
  "SEL+A EMBEZZLES 2 FOOD INTO\n"
  "4 MP BUT P DROPS BY 5\n",
  "5/8 FLOOD\n"
  "PRESS L (OR TAP SELECT):\n"
  "LOWERS A 3X3 AREA BY ONE\n"
  "LEVEL, COST 8 MP\n"
  "\n"
  "ANTS ON TILES THAT HIT LEVEL\n"
  "0 DROWN: GREAT AGAINST RED\n"
  "ARMIES BUT CAREFUL WITH\n"
  "YOUR OWN! MP REFILLED\n",
  "6/8 ANT EYE\n"
  "SEE THE WORLD LIKE A BLACK\n"
  "ANT: HOLD SELECT AND TAP\n"
  "START\n"
  "L R  TURN     U D  WALK\n"
  "A    NEXT ANT\n"
  "B    JUMP CURSOR HERE\n"
  "START  BACK\n"
  "RED POSTS: ENEMIES\n"
  "TALL POSTS: QUEENS\n",
  "7/8 THE COLONY\n"
  "BLACK ANTS FIND FOOD AND\n"
  "CARRY IT HOME. 3 FOOD HATCHES\n"
  "A NEW ANT\n"
  "EVERY 4TH IS A SOLDIER:\n"
  "WITH 8 ANTS THEY MARCH ON\n"
  "THE RED NEST\n"
  "ANTS FOLLOW TRAILS:\n"
  "MAKE EASY PATHS!\n",
  "8/8 POPULARITY\n"
  "P IS POPULARITY. FOOD AND\n"
  "NEW ANTS RAISE P: DEAD ANTS\n"
  "AND HUNGER CUT IT\n"
  "\n"
  "ELECTION EVERY MINUTE:\n"
  "P 50 UP: 8 MP AID\n"
  "P UNDER 25: COUP! COFFERS\n"
  "LOOTED\n",
  "READY TO RULE!\n"
  "KILL THE RED QUEEN TO WIN:\n"
  "LOSE YOURS AND ITS OVER\n"
  "\n"
  "START: PAUSE HELP\n"
  "R: FAST FORWARD\n"
  "\n"
  "LONG LIVE THE QUEEN!\n"
  "GOOD LUCK!\n",
};
static void tut_card(u8 i) {
  const char *s = TCARD[i]; u8 y = 1, x;
  hide_all(); vsync(); oam_flush();
  ov_fill(1, 1, 0, 19);
  while (*s) {
    x = 2; while (*s && *s != '\n') pc(x++, y, *s++, y == 1 ? 2 : 1);
    if (*s) s++;
    y += (y == 1) ? 2 : 1;                           // title row 1, body from row 3
  }
  ps(2, 18, "START: NEXT   B: SKIP", 2);
  while (joy()) vsync();
  for (;;) {
    vsync();
    if (joy() & J_START) break;
    if (joy() & J_B) { tut = 0; break; }
  }
  while (joy()) vsync();
  ov_clear(); hud();
}
static void tut_enter(void) {                        // show lessons until one needs the player to do something
  while (tut) {
    if (tut > TN) { tut = 0; say("GOOD LUCK RULER!"); sfx_mana(); break; }
    tut_card(tut - 1);
    if (!tut) { say("TUTORIAL SKIPPED"); break; }
    tev = 0; lcx = cx; lcy = cy;
    if (mana < 10) mana = 10;
    if (tut == 6) mana = MANA_MAX;
    if (TEV[tut - 1]) break;
    tut++;
  }
}

// ---------- play ----------
static void end_card(void) {
  u32 s = (u32)gt * 2 / 15; u8 nb = 0;
  ov_fill(1, 1, 18, 19);
  if (over == 1) {
    if (!sandbox && !cheat) {
      if (wins[diff] < 99) wins[diff]++;
      if (!best[diff] || s < best[diff]) { best[diff] = (u16)s; nb = 1; }
      save_write();
    }
    ps(0, 18, "YOU WIN!", 2); ps(10, 18, "TIME", 2);
    pn(15, 18, (u8)(s / 60 > 99 ? 99 : s / 60), 1); pc(17, 18, ':', 1); pz(18, 18, (u8)(s % 60), 1);
    if (nb) ps(21, 18, "NEW BEST!", 2);
  } else ps(0, 18, "COLONY LOST!", 2);
  ps(0, 19, "PRESS START", 1);
}
static void play(void) {
  u16 k, prev, p, last = 0; u8 dirs, rep = 0, fire, t = 4, ki = 0, selused = 0, selprev = 0, n;
  while (joy()) vsync();
  prev = 0;
  if (tutor) { tut = 1; tut_enter(); prev = joy(); }
  while (!over) {
    vsync();
    BG0HOFS = (u16)scx; BG0VOFS = (u16)scy; oam_flush();
    k = joy(); p = k & ~prev; prev = k;
    if ((p & J_START) && (k & J_SEL)) { selused = 1; eye(); prev = joy(); continue; }   // SELECT+START: ant eye
    if (p & J_START) { help_show(); hud(); prev = joy(); continue; }
    dirs = (u8)(k & 15);
    if (dirs != last) { rep = 0; last = dirs; }
    if (dirs) {
      fire = (rep == 0) || (rep >= 10 && (rep % 3) == 1);
      if (rep < 250) rep++;
      if (fire) {
        if ((dirs & J_LEFT) && cx > 0) cx--;
        if ((dirs & J_RIGHT) && cx < W - 1) cx++;
        if ((dirs & J_UP) && cy > 0) cy--;
        if ((dirs & J_DOWN) && cy < H - 1) cy++;
      }
    }
    if (p) {
      if (p == CHEAT[ki]) { if (++ki == 10) { ki = 0; cheat = 1; sfx_mana(); say("CHEAT ON! INFINITE"); } }
      else ki = (p == CHEAT[0]) ? 1 : 0;
    }
    if (k & J_SEL) {                                 // SELECT: modifier. SEL+A embezzle, SEL+B fast forward, alone (on release) flood
      if (p & J_SEL) selused = 0;
      if (p & J_A) { offering(); selused = 1; }
      if (p & J_B) { ff ^= 1; say(ff ? "FAST FORWARD ON" : "FAST FORWARD OFF"); selused = 1; }
    } else {
      if (selprev && !selused) flood();
      if (p & J_A) raise_land();
      if (p & J_B) lower_land();
    }
    selprev = (u8)(k & J_SEL);
    if (p & J_L) flood();                            // GBA shoulder shortcuts
    if (p & J_R) { ff ^= 1; say(ff ? "FAST FORWARD ON" : "FAST FORWARD OFF"); }
    if (tut && (cx != lcx || cy != lcy)) { tev |= 1; lcx = cx; lcy = cy; }
    if (tut && (tev & TEV[tut - 1])) { sfx_mana(); tut++; tut_enter(); prev = joy(); }
    if (tut) { qhp[0] = qhp[1] = QHP; if (appr < 50) appr = 50; }          // no game over mid-lesson
    if (cheat) { mana = MANA_MAX; stock[0] = 99; appr = 99; }
    if (sandbox) { mana = MANA_MAX; qhp[0] = qhp[1] = QHP; appr = 99; }
    follow(); scroll_step();
    for (n = ff ? 4 : 1; n; n--) tick_slice();
    if (++t >= 8) {
      t = 0; count(ncnt);
      if (tut && !msgt && THINT[tut - 1]) { msg = THINT[tut - 1]; msgt = 1; }   // keep the lesson goal on the HUD
      hud();
    }
    if (!qhp[1]) over = 1; else if (!qhp[0]) over = 2;
    draw_sprites();
  }
  music_stop();
  end_card();
  if (over == 1) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  vsync(); oam_flush();
  while (1) { vsync(); if (joy() & J_START) break; }
  while (joy()) vsync();
  fade(0, 16);
}

int main(void) {
  u8 i, x, y; static const u8 BARRM[4] = {1, 2, 2, 3};
  DISPCNT = 0x80;
  SNDX = 0x80; SNDH = 0x0002; SNDL = 0xFF77;          // PSG on, 100% DMG volume, all channels both sides
  BLDCNT = 0xFF; BLDY = 16;
  // palettes
  BGPAL[0] = RGB(8,16,28); BGPAL[1] = RGB(28,24,14); BGPAL[2] = RGB(8,20,6); BGPAL[3] = RGB(6,4,3);
  BGPAL[17] = RGB(0,0,0); BGPAL[18] = RGB(20,20,20); BGPAL[19] = RGB(31,31,31);
  BGPAL[33] = RGB(0,0,0); BGPAL[34] = RGB(22,12,3);  BGPAL[35] = RGB(31,27,8);
  BGPAL[49] = RGB(1,1,5); BGPAL[50] = RGB(11,11,22); BGPAL[51] = RGB(31,29,22);
  BGPAL[67] = RGB(14,8,2);
  OBJPAL[1] = RGB(2,2,2);  OBJPAL[2] = RGB(14,14,14); OBJPAL[3] = RGB(31,31,31);
  OBJPAL[17] = RGB(26,3,3); OBJPAL[18] = RGB(31,12,8); OBJPAL[19] = RGB(31,31,31);
  OBJPAL[33] = RGB(31,31,0); OBJPAL[34] = RGB(31,20,0); OBJPAL[35] = RGB(31,31,31);
  // tiles: 0 blank, 1 solid colour 1, 2 solid colour 3, 3 star, 4.. font, 44.. terrain + mana bar
  fill_tile(TILE32, 0); fill_tile(TILE32 + 8, 0x11111111u); fill_tile(TILE32 + 16, 0x33333333u);
  fill_tile(TILE32 + 24, 0x11111111u); TILE32[24 + 3] = 0x11131111u;
  for (i = 0; i < 40; i++) mk_glyph(TILE32 + (FT + i) * 8, FONT[i]);
  for (i = 0; i < 8; i++) put_tile(TILE32 + (BT + i) * 8, BGT + i * 16, i >= 6 ? BARRM : 0);
  for (i = 0; i < 5; i++) put_tile(OBJT32 + i * 8, SPT + i * 16, 0);
  for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) { BGMAP0[y * 32 + x] = BT; BGMAP1[y * 32 + x] = 0; }
  BGPAL[81] = RGB(10,18,28); view_init();
  BG0CNT = 2 | (28 << 8);                             // priority 2, charblock 0, screenblock 28
  BG1CNT = 0 | (29 << 8);                             // priority 0
  hide_all(); oam_flush();
  save_load();
  diff = 1;
  for (;;) { title(); newgame(); play(); }
}
