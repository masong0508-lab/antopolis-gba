// EMPIRE-ANTS for Game Boy Advance - port of the GBC game. Raw hardware registers, no libraries.
// Mode 0: BG0 = 32x32 world (scrolls), BG1 = HUD / overlay text, OBJ = ants, queens, cursor.
// All graphics come from art.h (the GBC 2bpp tiles, expanded to 4bpp at boot) and the 3x5 font.
// New on GBA: 30x18 tile view, 24 ants per colony, difficulty levels, game timer, SRAM records, L/R shortcuts.
#include <stdint.h>
#include "art.h"
#include "logo.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;

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
#define NR51 IO8(0x81)          // stereo routing: bits 0-3 right, 4-7 left (ch1..ch4), same layout as the GB
#define WAVE_ON 0xC0            // NR30: DAC on + play wave bank 1 (bit 6); writes always go to the bank that is NOT playing
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
// roguelike perks (ported from the GBC build): after an election that is not a coup, take one of two random perks with A or B
static u8 perk, pend, pa, pb;                    // perk = owned bit mask, pend = ticks left to choose, pa/pb = the two offered
static u8 fcost = 8, hc0 = 3, hc1 = 3, tm = 31, dm = 31, qmax0 = 5;   // flood cost, food per ant (you / red), mana trickle mask, popularity-drain mask, your queen HP cap
static char pbuf[20];
static const char PN[] = "FLOOD 4FASTEGGMANA UPCALM   QUEENUPSLOWRED";
static u8 tut, tev, tutor, lcx, lcy;             // tutorial: lesson (0 = off), events done, from-title flag, last cursor
static u8 wins[3]; static u16 best[3], hiscore[3];           // records (SRAM), best = seconds, 0 = none

static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs >> 8; }

// ---------- input / timing ----------
static u16 joy(void) {
  u16 k = (u16)~KEYS, r = 0;
  if (k & 1) r |= J_A;     if (k & 2) r |= J_B;    if (k & 4) r |= J_SEL;   if (k & 8) r |= J_START;
  if (k & 16) r |= J_RIGHT; if (k & 32) r |= J_LEFT; if (k & 64) r |= J_UP;  if (k & 128) r |= J_DOWN;
  if (k & 256) r |= J_R;   if (k & 512) r |= J_L;
  return r;
}
static void music_update(void), tm_update(void);
static void vsync(void) { while (VCOUNT >= 160); while (VCOUNT < 160); music_update(); tm_update(); }

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
  "ANTS CANT CLIMB OR SWIM\n"
  "MANA: TRICKLE, FOOD HOME,\n"
  "ELECTION AID (P 50 UP)\n"
  "P UNDER 25 = COUP\n"
  "NO COUP: PERK A OR B\n"
  "FLOODS AND QUAKES STRIKE\n";
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
  ps(0, 19, "START OR B: BACK", 2);
  while (joy()) vsync();
  while (!(joy() & (J_START | J_B))) vsync();
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
static u8 mus_on, mt, ms, nomus;                 // nomus = music switched off from the pause menu
// GBA wave RAM: with NR30 = 0 bank 0 plays and writes go to bank 1; then select bank 1 (bit 6) + DAC on (bit 7) = 0xC0
static void music_start(void) {
  u8 i;
  NR30 = 0x00;
  for (i = 0; i < 8; i++) WAVE16[i] = (u16)(WAVE[2 * i] | (WAVE[2 * i + 1] << 8));
  NR30 = WAVE_ON;
  mt = 7; ms = 0; mus_on = !nomus;
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
    NR30 = WAVE_ON; NR31 = 0xC8; NR32 = 0x40; NR33 = (u8)f; NR34 = (u8)(0xC0 | (f >> 8));
    n = LEAD[(b << 3) + e];
    if (n && !h2) { f = NOTE[n]; NR21 = 0x80; NR22 = 0x83; NR23 = (u8)f; NR24 = (u8)(0x80 | (f >> 8)); }
    if (!hn) { NR41 = 0; NR42 = DENV[e]; NR43 = DPOLY[e]; NR44 = 0x80; }
  }
  ms = (ms + 1) & 127;
}

// ---------- title theme (ported from the GBC build): E phrygian-dominant, ~81 BPM, 16 bars (8 + 8), in stereo ----------
// ch1 = echo of the lead (hard left), ch2 = lead with vibrato (hard right), ch3 = plucked 3+3+2 drone bass (centre),
// ch4 = tiny footsteps that ping-pong between the speakers. The title owns all four channels (no sfx play there).
static const u8 TLEAD[128] = {       // one entry per eighth note: 0 = rest, 1 = hold, else semitones above C2 (like LEAD)
   0, 0, 0, 0,  0, 0,35,36,
  40, 1, 1,38, 36, 1,35,33,
  32, 1,33, 1, 35,36,35,33,
  32, 1, 1, 1,  0, 0,35,36,
  41, 1,40,38, 40, 1,36,38,
  36, 1, 1,35, 33, 1,32,33,
  44, 1,41,40, 38,36,35, 1,
  40, 1, 1, 1,  1, 1, 0, 0,
  32,37,39,40,41,39, 1, 1, 1,40,39, 1,41, 1, 1,38,
  34,38, 1, 1,32,34,37, 1,34,35, 1, 1, 0, 0, 0, 0,
  40,37,38, 1, 1, 1,36, 1, 1, 1, 1,35,37,35, 1, 0,
  33,37, 1,40, 1, 1,37,40, 1, 1,41,39,41, 1, 0, 0};
static const u8 TROOT[8] = {16, 16, 17, 16, 21, 17, 23, 16};         // bass root per bar: E E F E A F B E
static const u8 TBASS[8] = {0, 255, 255, 0, 255, 255, 7, 255};       // per eighth: root, root, fifth (255 = none)
static const char *const TSTEP[2] = {"X.x.x.xxX.x.x.x.", "X.x.xx.xX.x.xxx."};   // footsteps per 16th, alternating bars
static const s8 TVIB[8] = {0, 1, 2, 1, 0, -1, -2, -1};
static const u8 TWAVE[16] = {0x8C,0xFF,0xED,0xCD,0xDD,0xCD,0xEF,0xFC,0x83,0x00,0x12,0x32,0x22,0x32,0x10,0x03};   // hollow, reedy
static u8 tmu, tmf, tme, tvt, tvp, tpan;           // on, frame in eighth, eighth 0-127, lead age, vibrato phase, noise side
static u16 tlf;                                    // lead base frequency (vibrato wobbles around it)
static void tm_start(void) {
  u8 i;
  NR30 = 0x00;                                     // bank 0 plays: load the waveform into bank 1, then switch to it
  for (i = 0; i < 8; i++) WAVE16[i] = (u16)(TWAVE[2 * i] | (TWAVE[2 * i + 1] << 8));
  NR30 = WAVE_ON;
  NR51 = 0xD6;                                     // ch1 left, ch2 right, ch3 both, ch4 left (moves per step)
  tmf = 21; tme = 127; tvt = 255; tvp = 0; tpan = 0; tmu = !nomus;      // the first update plays step 0
}
static void tm_stop(void) {
  tmu = 0;
  NR12 = 0; NR22 = 0; NR42 = 0; NR30 = 0;
  NR51 = 0xFF;                                     // back to centred sound for the game
}
static void tm_update(void) {                      // once per frame from vsync(): 22 frames per eighth note
  u8 b, e, n, s; u16 f; s16 a;
  if (!tmu) return;
  if (++tmf >= 22) { tmf = 0; tme = (tme + 1) & 127; }
  b = tme >> 3; e = tme & 7;
  if (tmf == 0) {
    if (TBASS[e] != 255) {                         // bass: short plucks on the wave channel
      f = NOTE[TROOT[b & 7] + TBASS[e]];
      NR30 = WAVE_ON; NR31 = 0x60; NR32 = 0x40; NR33 = (u8)f; NR34 = (u8)(0xC0 | (f >> 8));
    }
    n = TLEAD[tme];
    if (n > 1) {                                   // lead: 25% pulse, slow decay
      tlf = NOTE[n]; tvt = 0; tvp = 0;
      NR21 = 0x40; NR22 = 0xA5; NR23 = (u8)tlf; NR24 = (u8)(0x80 | (tlf >> 8));
    }
    n = TLEAD[(tme - 1) & 127];
    if (n > 1) {                                   // echo: last eighth's note again, thin and quiet, on the other side
      f = NOTE[n];
      NR10 = 0; NR11 = 0x00; NR12 = 0x55; NR13 = (u8)f; NR14 = (u8)(0x80 | (f >> 8));
    }
  }
  if (tmf == 0 || tmf == 11) {                     // footsteps on the 16th grid
    s = (u8)((e << 1) | (tmf ? 1 : 0));
    n = (u8)TSTEP[b & 1][s];
    if (n != '.') {
      tpan ^= 1;
      NR51 = (u8)(0x56 | (tpan ? 0x80 : 0x08));
      NR41 = 0; NR42 = (n == 'X') ? 0x51 : 0x31; NR43 = tpan ? 0x29 : 0x39; NR44 = 0x80;
    }
  }
  if (tvt != 255) {                                // vibrato on the lead once the note has settled
    if (tvt < 100) tvt++;
    tvp++;
    if (tvt >= 8) {
      a = (s16)((2048 - tlf) >> 6); if (a < 1) a = 1;
      s = (u8)TVIB[(tvp >> 1) & 7];                // -2..2 (a * v / 2 without mul/div)
      n = (s == 2 || s == 254) ? (u8)a : (s == 1 || s == 255) ? (u8)(a >> 1) : 0;
      f = (s >= 128) ? (u16)(tlf - n) : (u16)(tlf + n);
      NR23 = (u8)f; NR24 = (u8)(f >> 8);           // no trigger bit: pitch only
    }
  }
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
  for (i = 0; i < 3; i++) {
    SRAM[4 + i] = wins[i]; SRAM[7 + 2 * i] = (u8)(best[i] & 255); SRAM[8 + 2 * i] = (u8)(best[i] >> 8);
    SRAM[13 + 2 * i] = (u8)(hiscore[i] & 255); SRAM[14 + 2 * i] = (u8)(hiscore[i] >> 8);
  }
  for (i = 4; i < 19; i++) s = (u8)(s + SRAM[i]);
  SRAM[19] = s;
}
static void save_load(void) {
  u8 i, s = 0;
  for (i = 4; i < 19; i++) s = (u8)(s + SRAM[i]);
  if (SRAM[0] == 'E' && SRAM[1] == 'A' && SRAM[2] == 'N' && SRAM[3] == 'T' && SRAM[19] == s) {
    for (i = 0; i < 3; i++) {
      wins[i] = SRAM[4 + i]; best[i] = (u16)(SRAM[7 + 2 * i] | (SRAM[8 + 2 * i] << 8));
      hiscore[i] = (u16)(SRAM[13 + 2 * i] | (SRAM[14 + 2 * i] << 8));
    }
  } else { for (i = 0; i < 3; i++) { wins[i] = 0; best[i] = 0; hiscore[i] = 0; } save_write(); }
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
static void set_rules(void) {
  fcost = 8; hc0 = 3; hc1 = RCOST[diff]; tm = TRK[diff]; dm = 31; qmax0 = QHP;
  if (perk & 1) fcost = 4;                       // FLOOD 4
  if (perk & 2) hc0 = 2;                         // FASTEGG
  if (perk & 4) tm >>= 1;                        // MANA UP
  if (perk & 8) dm = (u8)((dm << 1) | 1);        // CALM: popularity sinks slower, disasters rarer
  if (perk & 16) qmax0 = QHP + 1;                // QUEENUP
  if (perk & 32) hc1++;                          // SLOWRED
}
static void offer(void) {                        // two different perks you do not own yet
  u8 i;
  if (perk == 63) return;
  do pa = (u8)(rnd() & 7); while (pa > 5 || ((perk >> pa) & 1));
  pb = pa;
  do pb = pb > 4 ? 0 : pb + 1; while ((perk >> pb) & 1);
  pbuf[0] = 'A'; pbuf[1] = ':'; pbuf[9] = ' '; pbuf[10] = 'B'; pbuf[11] = ':';
  for (i = 0; i < 7; i++) { pbuf[2 + i] = PN[pa * 7 + i]; pbuf[12 + i] = PN[pb * 7 + i]; }
  pbuf[19] = 0; pend = 80;                       // about 10 s to choose
}
static void pick(u8 i) {
  perk |= (u8)(1 << i); pend = 0; set_rules();
  if (i == 4 && qhp[0]) qhp[0]++;
  sfx_mana(); say("PERK TAKEN!");
}
static void hit(u8 px, u8 py, u8 q) {            // 3x3 patch: every tile sinks one level; q = earthquake (each tile up or down)
  s8 x, y;
  for (y = (s8)py - 1; y <= (s8)py + 1; y++) for (x = (s8)px - 1; x <= (s8)px + 1; x++)
    if (x >= 0 && y >= 0 && x < W && y < H && !is_nest(x, y)) {
      if (q && (rnd() & 1)) { if (hgt[y][x] < 3) hgt[y][x]++; } else if (hgt[y][x]) hgt[y][x]--;
      draw_cell(x, y);
    }
}
static u8 dmask_dis(void) { u8 b = diff == 0 ? 127 : diff == 1 ? 63 : 31; return (perk & 8) ? (u8)((b << 1) | 1) : b; }
static void disaster(void) {                     // hits both colonies alike, anywhere on the map
  u8 q = (u8)(rnd() & 1);
  hit((u8)(rnd() & (W - 1)), (u8)(rnd() & (H - 1)), q);
  sfx_flood(); say(q ? "EARTHQUAKE!" : "FLASH FLOOD!");
}
static void election(void) {
  u8 ok = appr >= 25;                            // anything but a coup earns a perk offer
  if (appr >= 50) { mana = (mana + 8 > MANA_MAX) ? MANA_MAX : mana + 8; sfx_mana(); say("ELECTION WON! AID"); }
  else if (appr >= 25) { sfx_deny(); say("ELECTION: NO BONUS"); }
  else { stock[0] >>= 1; mana = 0; appr = 40; sfx_qdead(); say("COUP! COFFERS LOOTED"); }
  if (ok && !tut && !sandbox) offer();
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
    cst = i ? hc1 : hc0;
    if (stock[i] >= cst && spawn(i)) { stock[i] -= cst; if (i == 0) { sfx_spawn(); apr(1); } }
    else if (ncnt[i] == 0 && (gt & 31) == 0) spawn(i);
    if ((gt & 127) == 0 && qhp[i] < (i ? QHP : qmax0)) qhp[i]++;
  }
  if ((gt & dm) == 0) apr(!stock[0] && ncnt[0] ? -2 : -1);
  if (pend) pend--;
  if (!tut && !sandbox && gt > 90 && (gt & dmask_dis()) == 0 && !(rnd() & 3)) disaster();   // rarer on EASY / with CALM
  if (++etk >= 450) { etk = 0; election(); }
  if ((gt & 7) == 0) for (i = 0; i < 2; i++) {
    x = rnd() & (W - 1); y = rnd() & (H - 1);
    if (hgt[y][x] && !food[y][x] && !is_nest(x, y)) { food[y][x] = 1; draw_cell(x, y); }
  }
  if ((gt & tm) == 0 && mana < MANA_MAX) mana++;
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
  perk = 0; pend = 0; set_rules(); ff = 0; appr = 50; etk = 0; slice = 0; cheat = 0; cx = nestx[0]; cy = nesty[0] - 2; mana = 10; gt = 0; over = 0; msgt = 0;
  camx = 0; camy = H - VH; scx = 0; scy = (s16)camy * 8;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) draw_cell(x, y);
  ov_clear(); hud();
  say(sandbox ? "SANDBOX: NO LIMITS" : "A/B LAND  START MENU");
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
  if (mana < fcost) { sfx_deny(); say(fcost == 4 ? "FLOOD NEEDS 4 MANA" : "FLOOD NEEDS 8 MANA"); return; }
  mana -= fcost; sfx_flood(); tev |= 8;
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
  tm_start();                                      // the title has its own stereo theme
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
  tm_stop();
  hide_all(); oam_flush();
  ov_clear();
  while (joy()) vsync();
}

// ---------- ANT EYE: first-person view from a black ant (SELECT+START) ----------
// Hybrid renderer (video mode 1):
//  * BG2 (affine) is a real Mode-7 floor: the 32x32 world as 8bpp tiles. HBlank DMA0 rewrites PA/PC + the reference point for every
//    scanline (perspective, any angle, pixel-smooth, nearly free for the CPU); HBlank DMA1 fades the far floor into the sky (BLDALPHA).
//  * BG1 (rows 0-15 = 30 x 16 tiles = 240x128 px) keeps the old one-ray-per-tile-column marcher, but only for what a flat plane
//    cannot show: terrain above / below eye level, ants and nests. Eye-level cells are written as colour 0 (transparent).
//  * While the camera moves (turn = ease, step = glide) the overlay is hidden and the floor does the work; the overlay is redrawn
//    for the final pose in the background and appears when the camera has settled.
// Overlay colour classes (4bpp, palette bank 5): 0 clear, 2/3 checker land, 4 hill, 5 water, 6 red, 7 black. BG2 uses the same
// palette entries (80 + class) plus 96-98 for texture detail.
#define VX 30
#define VR 16
#define EV 64                                         // first overlay tile (480 tiles)
#define VN 24                                         // ray steps, half a cell each
#define HZ 64                                         // horizon scanline
#define FY0 65                                        // first Mode-7 scanline (D ~ 21 cells); above it: sky
#define FY1 128                                       // first scanline of the status rows
#define TURN 8                                        // angle units: 256 = full turn, so 8 = 11.25 degrees
#define SKYC (-0x4000000)                             // reference point far outside the 256x256 map = transparent
#define DMAR(n, o) (*(volatile u32 *)(0x040000B0u + (n) * 12u + (o)))
#define DMA_ON  0x80000000u
#define DMA_HBL 0x20000000u
#define DMA_REP 0x02000000u
#define DMA_W32 0x04000000u
#define DMA_RLD 0x00600000u                           // dest increment + reload each HBlank
#define DMA_FIX 0x00400000u
#ifdef __arm__
#define IWRAM __attribute__((section(".data.iwram"), long_call, noinline))   // hot code runs from IWRAM (ROM is ~6x slower)
#else
#define IWRAM
#endif
typedef struct { u32 ab, cd; s32 x, y; } M7;          // PA|PB<<16, PC|PD<<16, BG2X, BG2Y = registers 0x04000020..2F
static const s16 SINT[256] = {                        // sin(a), 256 = 1.0, a in 1/256 turns, 0 = east, clockwise (y points down)
  0,6,13,19,25,31,38,44,50,56,62,68,74,80,86,92,
  98,104,109,115,121,126,132,137,142,147,152,157,162,167,172,177,
  181,185,190,194,198,202,206,209,213,216,220,223,226,229,231,234,
  237,239,241,243,245,247,248,250,251,252,253,254,255,255,256,256,
  256,256,256,255,255,254,253,252,251,250,248,247,245,243,241,239,
  237,234,231,229,226,223,220,216,213,209,206,202,198,194,190,185,
  181,177,172,167,162,157,152,147,142,137,132,126,121,115,109,104,
  98,92,86,80,74,68,62,56,50,44,38,31,25,19,13,6,
  0,-6,-13,-19,-25,-31,-38,-44,-50,-56,-62,-68,-74,-80,-86,-92,
  -98,-104,-109,-115,-121,-126,-132,-137,-142,-147,-152,-157,-162,-167,-172,-177,
  -181,-185,-190,-194,-198,-202,-206,-209,-213,-216,-220,-223,-226,-229,-231,-234,
  -237,-239,-241,-243,-245,-247,-248,-250,-251,-252,-253,-254,-255,-255,-256,-256,
  -256,-256,-256,-255,-255,-254,-253,-252,-251,-250,-248,-247,-245,-243,-241,-239,
  -237,-234,-231,-229,-226,-223,-220,-216,-213,-209,-206,-202,-198,-194,-190,-185,
  -181,-177,-172,-167,-162,-157,-152,-147,-142,-137,-132,-126,-121,-115,-109,-104,
  -98,-92,-86,-80,-74,-68,-62,-56,-50,-44,-38,-31,-25,-19,-13,-6,
};
static const u8 BBH[VN + 1] = {0, 40, 32, 21, 16, 12, 10, 9, 8, 7, 6, 5, 5, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2};
static const char *const COMPASS[8] = {"EAST", "SOUTH EAST", "SOUTH", "SOUTH WEST", "WEST", "NORTH WEST", "NORTH", "NORTH EAST"};
static s8 OFF[7][VN + 1];
static u8 occ[H][W], cls[VR * 8], vang;               // vang = facing, 0..255
static u16 KK[160], KQ[160], blt[160];                // per scanline: ground distance (cells * 2048), K * 4/3, BLDALPHA fog
static M7 m7t[2][161];                                // double-buffered HBlank DMA tables
static void view_init(void) {
  u8 d, n; u16 y; s32 k;
  for (d = 0; d < 7; d++) for (n = 1; n <= VN; n++) {
    s16 v = (s16)((((s16)d - 3) * 4 - 3) * 21) / n;
    OFF[d][n] = v > 80 ? 80 : v < -80 ? -80 : (s8)v;
  }
  // flat ground seen from 0.164 cells up with a 192 px focal length: distance D = 31.5 / (y + 0.5 - 64) cells; K = D * 2048
  for (y = 0; y < 160; y++) {
    KK[y] = KQ[y] = 0; blt[y] = 16;
    if (y >= FY0 && y < FY1) {
      k = 129024 / (2 * (s32)y - 127);
      KK[y] = (u16)k; KQ[y] = (u16)(k * 4 / 3);
      k = 16 - k / 3300; if (k < 3) k = 3;
      blt[y] = (u16)(k | ((16 - k) << 8));
    }
  }
}
IWRAM static void view_col(u8 c, u8 vx, u8 vy, u8 eh, u8 ang) {   // one ray -> cls[0..127]
  s16 t = (s16)(((s16)c * 2 - 29) * 2 / 3), px = (s16)vx * 256 + 128, py = (s16)vy * 256 + 128, yt, a, e, r, rx, ry, p0x, p0y;
  s16 cs = SINT[(ang + 64) & 255], sn = SINT[ang];
  s32 f, g;
  u8 n, x, y, h, o, col, lim = VR * 8, lb, pcx = vx, pcy = vy, pf = 1;
  rx = (s16)(((s16)cs * 32 - (s16)sn * t) >> 6);
  ry = (s16)(((s16)sn * 32 + (s16)cs * t) >> 6);
  for (n = 0; n < VR * 8; n++) cls[n] = 0;
  for (n = 1; n <= VN && lim; n++) {
    p0x = px; p0y = py;
    px += rx; py += ry;
    if (px < 0 || py < 0 || px >= W * 256 || py >= H * 256) break;
    x = (u8)(px >> 8); y = (u8)(py >> 8);
    h = hgt[y][x];
    yt = 64 - OFF[h + 3 - eh][n];
    lb = lim;
    if (h != eh && pf && (x != pcx || y != pcy)) {    // level ground -> raised / sunken cell: its front edge sits where the ray crossed the border,
      f = 0;                                          // so the Mode-7 floor in front of it stays visible (steps are only half a cell apart)
      if (x != pcx) f = (((s32)(rx > 0 ? x : x + 1) * 256 - p0x) * 16) / rx;
      if (y != pcy) { g = (((s32)(ry > 0 ? y : y + 1) * 256 - p0y) * 16) / ry; if (g > f) f = g; }
      f = (f > 16 ? 16 : f) + (n - 1) * 16; if (f < 1) f = 1;
      f = 64 + 1008 / f;                              // eye-level ground row at that distance
      if (f < lb) lb = (u8)f;
    }
    pcx = x; pcy = y; pf = (h == eh);
    if (yt < lb) {
      a = yt < 0 ? 0 : yt;
      col = h == eh ? 0 : h == 0 ? 5 : h == 3 ? 4 : ((x + y) & 1) ? 2 : 3;   // eye level: leave it to the Mode-7 floor
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
IWRAM static void eye_put(u8 c) {                     // cls[] -> the 16 tiles of overlay column c (every pixel row is one colour)
  volatile u32 *d = TILE32 + (EV + c * VR) * 8; u8 i;
  for (i = 0; i < VR * 8; i++) d[i] = cls[i] * 0x11111111u;
}
IWRAM static void ov_show(u8 on) {
  u8 x, y;
  for (x = 0; x < VX; x++) for (y = 0; y < VR; y++) BGMAP1[y * 32 + x] = on ? (u16)((EV + x * VR + y) | (5 << 12)) : 0;
}
// The ground as the Mode-7 hardware sees it. Screen pixel (x, y) shows the point P = cam + D*fwd + D*(u/192)*right, u = x + 0.5 - 120,
// with D = K(y) cells ahead. In 8.8 texels (1 cell = 8 texels): PA,PC = K/192 * right, X0,Y0 = cam + K*fwd - 120 * (PA,PC).
IWRAM static void m7_build(M7 *t, u8 ang, s16 fx, s16 fy) {
  s32 cs = SINT[(ang + 64) & 255], sn = SINT[ang], bx = (s32)fx * 8, by = (s32)fy * 8, K, q, pa, pc;
  u8 y;
  for (y = FY0; y < FY1; y++) {
    K = KK[y]; q = KQ[y];
    pa = (-q * sn) >> 16; pc = (q * cs) >> 16;
    t[y].ab = (u16)pa;                                // PB = 0
    t[y].cd = (u16)pc;                                // PD = 0
    t[y].x = bx + ((K * cs) >> 8) - pa * 120 + (pa >> 1);
    t[y].y = by + ((K * sn) >> 8) - pc * 120 + (pc >> 1);
  }
}
static void m7_sky(M7 *t) { u8 y; for (y = 0; y < 161; y++) { t[y].ab = t[y].cd = 0; t[y].x = t[y].y = SKYC; } }
static void eye_arm(const M7 *t) {                    // call right after vsync(): line 0 by hand, lines 1.. by HBlank DMA
  DMAR(0, 8) = 0; DMAR(1, 8) = 0;
  *(volatile u32 *)0x04000020 = t[0].ab; *(volatile u32 *)0x04000024 = t[0].cd;
  *(volatile u32 *)0x04000028 = (u32)t[0].x; *(volatile u32 *)0x0400002C = (u32)t[0].y;
  IO16(0x52) = blt[0];
  DMAR(0, 0) = (u32)(t + 1); DMAR(0, 4) = 0x04000020u; DMAR(0, 8) = DMA_ON | DMA_HBL | DMA_REP | DMA_W32 | DMA_RLD | 4;
  DMAR(1, 0) = (u32)(blt + 1); DMAR(1, 4) = 0x04000052u; DMAR(1, 8) = DMA_ON | DMA_HBL | DMA_REP | DMA_FIX | 1;
}
static void floor_tiles(volatile u32 *d) {            // 8bpp 8x8 tiles: 0 clear, 1/2 grass (odd / even cell), 3 hill, 4 water
  static const u8 BASE[4] = {82, 83, 84, 85}, SPEC[4] = {83, 82, 96, 97};
  u8 t, p, k, px, py, hh; u32 w;
  for (p = 0; p < 16; p++) d[p] = 0;
  for (t = 1; t <= 4; t++) for (p = 0; p < 16; p++) {
    w = 0;
    for (k = 0; k < 4; k++) {
      px = (u8)((p & 1) * 4 + k); py = (u8)(p >> 1);
      hh = (u8)((px * 37 + py * 91 + t * 53) & 15);
      w |= (u32)((t == 4 ? ((py & 3) == 1 && px >= 2 && px <= 5) : hh == 0) ? SPEC[t - 1] : BASE[t - 1]) << (k * 8);
    }
    d[t * 16 + p] = w;
  }
}
static void floor_map(volatile u16 *m) {              // 32x32 one-byte entries, read as halfwords
  u8 x, y, k, h; u16 e;
  for (y = 0; y < H; y++) for (x = 0; x < W; x += 2) {
    e = 0;
    for (k = 0; k < 2; k++) { h = hgt[y][x + k]; e |= (u16)(h == 0 ? 4 : h == 3 ? 3 : ((x + k + y) & 1) ? 1 : 2) << (k * 8); }
    m[y * 16 + x / 2] = e;
  }
}
static void occ_set(u8 on) {
  u8 i;
  for (i = 0; i < MAXA; i++) if (ant[i].alive) occ[ant[i].y][ant[i].x] = on ? ant[i].team + 1 : 0;
  for (i = 0; i < 2; i++) if (qhp[i]) occ[nesty[i]][nestx[i]] = on ? 3 + i : 0;
}
static u8 vok(u8 i) { return i < MAXA ? (ant[i].alive && ant[i].team == 0) : qhp[0] > 0; }
static void eye_dir(s8 *dx, s8 *dy) {                // the grid direction the ant faces (the axis closest to its heading)
  s16 fx = SINT[(vang + 64) & 255], fy = SINT[vang]; *dx = *dy = 0;
  if ((fx < 0 ? -fx : fx) >= (fy < 0 ? -fy : fy)) *dx = fx < 0 ? -1 : 1; else *dy = fy < 0 ? -1 : 1;
}
static const char *eye_ahead(u8 vx, u8 vy) {         // what a step forward would run into, in at most 6 letters
  s8 dx, dy, a, b, d; u8 o;
  eye_dir(&dx, &dy);
  a = (s8)vx + dx; b = (s8)vy + dy;
  if (a < 0 || b < 0 || a >= W || b >= H) return "EDGE";
  o = occ[b][a];
  if (o) return o == 1 ? "FRIEND" : o == 2 ? "ENEMY" : o == 3 ? "HOME" : "R NEST";
  if (hgt[b][a] == 0) return "WATER";
  d = (s8)hgt[b][a] - (s8)hgt[vy][vx];
  return (d < -1 || d > 1) ? "CLIFF" : "CLEAR";
}
// Status panel (rows 16-19, 30 columns). Gold = labels, white = values:
//   FROM ANT 3/12            HEIGHT 1        which ant you ride (or YOUR NEST), and the ground level under it
//   FACING SOUTH WEST   AHEAD CLEAR          compass heading, and what one step forward would meet
//   PAD: WALK/TURN  A: NEXT ANT              the controls, always on screen
//   B: CURSOR HERE  START: LEAVE
static void eye_status(u8 vi, u8 vx, u8 vy) {
  u8 i, n = 0, r = 0;
  ov_fill(1, 1, 16, 19);
  if (vi < MAXA) {
    for (i = 0; i < MAXA; i++) if (vok(i)) { n++; if (i == vi) r = n; }
    ps(0, 16, "FROM", 2); ps(5, 16, "ANT", 1); pn(9, 16, r, 1); pc(11, 16, '/', 1); pn(12, 16, n, 1);
  } else { ps(0, 16, "FROM", 2); ps(5, 16, "YOUR NEST", 1); }
  ps(20, 16, "HEIGHT", 2); pc(27, 16, (char)('0' + hgt[vy][vx]), 1);
  ps(0, 17, "FACING", 2); ps(7, 17, COMPASS[((vang + 16) >> 5) & 7], 1);
  ps(18, 17, "AHEAD", 2); ps(24, 17, eye_ahead(vx, vy), 1);
  ps(0, 18, "PAD: WALK/TURN  A: NEXT ANT", 1);
  ps(0, 19, "B: CURSOR HERE  START: LEAVE", 1);
}
static void eye_step(u8 *vx, u8 *vy, s8 dir) {
  s8 dx, dy;
  eye_dir(&dx, &dy);
  dx *= dir; dy *= dir;
  if (can_go(*vx, *vy, dx, dy)) { *vx += dx; *vy += dy; }
}
static s16 ease(s16 v, s16 t, s16 s) { s16 d = t - v; return d > s ? v + s : d < -s ? v - s : t; }
static void eye(void) {                              // time stands still while you look through a black ant's eyes
  u8 i, x, y, vi = MAXA, best = 255, d, rep = 0, vx = 0, vy = 0, vcol = 0, dirs, last = 0, on = 0, cur = 0, act = 0, jump, chg, vc;
  s16 fa = vang, fx, fy, tx, ty, da;
  u16 k, p, prev;
  tev |= 16;
  for (i = 0; i < MAXA; i++) if (vok(i)) { d = (u8)(dist(ant[i].x, cx) + dist(ant[i].y, cy)); if (d < best) { best = d; vi = i; } }
  if (vi < MAXA) { vx = ant[vi].x; vy = ant[vi].y; } else { vx = nestx[0]; vy = nesty[0]; }
  fx = (s16)(vx * 256 + 128); fy = (s16)(vy * 256 + 128);
  hide_all(); vsync(); oam_flush();
  BGPAL[82] = RGB(11,26,9); BGPAL[83] = RGB(4,16,6); BGPAL[84] = RGB(21,14,6); BGPAL[85] = RGB(4,10,31);
  BGPAL[86] = RGB(31,6,4);  BGPAL[87] = RGB(2,2,2);  BGPAL[96] = RGB(15,9,3);  BGPAL[97] = RGB(12,19,31);
  for (y = 16; y < 20; y++) for (x = 0; x < 32; x++) BGMAP1[y * 32 + x] = 1 | (1 << 12);
  occ_set(1);
  floor_tiles(TILE32 + 0x2000);                       // charblock 2 (0x06008000)
  floor_map((volatile u16 *)0x0600D800);              // screenblock 27
  m7_sky(m7t[0]); m7_sky(m7t[1]);
  m7_build(m7t[0], vang, fx, fy);
  ov_show(0); eye_status(vi, vx, vy);
  while (joy()) vsync();
  vsync(); eye_arm(m7t[0]);
  IO16(0x0C) = 0x5B0B;                                // BG2: priority 3, charblock 2, screenblock 27, 256x256 affine
  BLDCNT = 0x2044;                                    // alpha blend: BG2 over the backdrop (= sky colour)
  DISPCNT = 0x1641;                                   // mode 1: BG1 + BG2 + OBJ
  prev = 0;
  for (;;) {
    vsync();
    eye_arm(m7t[cur]);                                // the table built last frame drives this frame's HBlank DMA
    k = joy(); p = k & ~prev; prev = k;
    if (p & J_START) break;
    if (p & J_B) { cx = vx; cy = vy; break; }
    jump = chg = 0;
    if (p & J_A) {
      i = 0; do { vi = (vi == MAXA) ? 0 : vi + 1; } while (!vok(vi) && ++i <= MAXA); sfx_food();
      if (vi < MAXA) { vx = ant[vi].x; vy = ant[vi].y; } else { vx = nestx[0]; vy = nesty[0]; }
      jump = chg = 1;
    }
    dirs = (u8)((k & 15) | ((k & J_L) ? J_LEFT : 0) | ((k & J_R) ? J_RIGHT : 0));   // L / R turn too (as the tutorial says)
    if (dirs != last) { rep = 0; last = dirs; }
    if (dirs) {
      if (dirs & (J_LEFT | J_RIGHT)) {                // turning: every 2 frames once held, matches the 4 units / frame ease
        if (rep == 0 || (rep >= 8 && !(rep & 1))) { vang = (u8)(vang + ((dirs & J_RIGHT) ? TURN : 256 - TURN)); chg = 1; }
      } else if (rep == 0 || (rep >= 8 && !(rep & 3))) {   // walking: one cell per 4 frames = the glide time
        x = vx; y = vy; eye_step(&vx, &vy, (dirs & J_UP) ? 1 : -1); if (vx != x || vy != y) chg = 1;
      }
      if (rep < 250) rep++;
    }
    tx = (s16)(vx * 256 + 128); ty = (s16)(vy * 256 + 128);
    if (jump) { fx = tx; fy = ty; }                   // jumping to another ant: no glide
    if (chg) { vcol = 0; act = 0; eye_status(vi, vx, vy); } else if (act < 250) act++;
    da = (s16)(((vang - fa + 128) & 255) - 128);      // ease the floor toward the wanted pose
    fa = (s16)((fa + (da > 4 ? 4 : da < -4 ? -4 : da)) & 255);
    fx = ease(fx, tx, 64); fy = ease(fy, ty, 64);
    i = (u8)(vcol >= VX && fa == vang && fx == tx && fy == ty && act >= 5);   // camera settled and overlay complete
    if (i != on) { ov_show(i); on = i; }
    cur ^= 1;
    m7_build(m7t[cur], (u8)fa, fx, fy);               // next frame's table (the other buffer is being read by DMA)
    if (vcol < VX) {                                  // redraw the overlay for the final pose, as many columns as the frame allows
      d = hgt[vy][vx];
      do { view_col(vcol, vx, vy, d, vang); eye_put(vcol); vcol++; vc = (u8)VCOUNT; } while (vcol < VX && !(vc >= 100 && vc < 160));
    }
  }
  vsync();
  DMAR(0, 8) = 0; DMAR(1, 8) = 0;
  occ_set(0);
  DISPCNT = 0x1340; BLDCNT = 0xFF; IO16(0x0C) = 0;
  ov_clear(); hud();
  while (joy()) vsync();
}

// ---------- TUTORIAL: lessons (title screen: press A) ----------
// Each lesson is a full-screen card (START next, B skip); lessons with a task then show a live hint on the HUD until you do it.
#define TN 10
static const u8 TEV[TN] = {0, 1, 2, 4, 0, 8, 16, 0, 0, 0};          // event bit that completes each lesson (0 = read only)
static const char *const THINT[TN] = {0, "TRY: MOVE THE CURSOR", "TRY: PRESS A TO RAISE", "TRY: PRESS B TO LOWER", 0,
  "PRESS L TO FLOOD", "SEL+START: ANT EYE", 0, 0, 0};
static const char *const TTITLE[TN] = {"WELCOME RULER!", "THE CURSOR", "RAISE LAND", "LOWER LAND", "MANA", "FLOOD", "ANT EYE", "THE COLONY", "POPULARITY", "READY TO RULE!"};
// Lesson text: 7 lines of up to 28 letters (the font has A-Z 0-9 : ! / only). *asterisks* switch gold highlighting on and off.
static const char *const TBODY[TN] = {
  "YOU RULE THE *BLACK ANTS* OF\n"
  "EMPIRE ANTS BUT YOU CANT\n"
  "GIVE ORDERS: *SHAPE THE LAND*\n"
  "AND THEY WALK AROUND IT\n"
  "\n"
  "*GOAL:* KILL THE RED QUEEN\n"
  "BEFORE THEY KILL YOURS",
  "THE *YELLOW FRAME* IS YOUR\n"
  "CURSOR: LAND TOOLS WORK ON\n"
  "THE TILE UNDER IT\n"
  "\n"
  "*D PAD* MOVES IT: HOLD TO\n"
  "REPEAT: MAP SCROLLS AT EDGE\n"
  "*NOW TRY IT!*",
  "*A* RAISES THE CURSOR TILE\n"
  "BY ONE LEVEL: COST *1 MANA*\n"
  "\n"
  "ANTS CANT CLIMB MORE THAN\n"
  "*1 STEP*: BUILD RAMPS AND\n"
  "STAIRS! NESTS CANT BE EDITED",
  "*B* LOWERS THE TILE BY ONE\n"
  "LEVEL: COST *1 MANA*\n"
  "\n"
  "LEVEL 0 IS *WATER*: ANTS CANT\n"
  "WALK OR LIVE THERE: DIG\n"
  "MOATS TO STOP RED ANTS AND\n"
  "RAISE LAND TO BRIDGE GAPS",
  "*MP* IS YOUR MANA: EVERY EDIT\n"
  "COSTS MP: EARN MORE FROM:\n"
  "*A SLOW TRICKLE*\n"
  "*FOOD CARRIED HOME*\n"
  "*ELECTION AID*\n"
  "\n"
  "*SEL A* EMBEZZLES: P DROPS 5",
  "*L* LOWERS A 3X3 AREA BY ONE\n"
  "LEVEL: COST *8 MANA*\n"
  "\n"
  "ANTS ON TILES THAT HIT LEVEL\n"
  "0 *DROWN*: GREAT AGAINST RED\n"
  "ARMIES BUT CAREFUL WITH\n"
  "YOUR OWN!",
  "HOLD *SEL* AND TAP *START*\n"
  "TO SEE LIKE A BLACK ANT\n"
  "*L R* TURN   *UP DOWN* WALK\n"
  "*A* NEXT ANT  *B* CURSOR HERE\n"
  "*START* LEAVE\n"
  "*RED POSTS* ARE ENEMIES\n"
  "*TALL POSTS* ARE QUEENS",
  "*BLACK ANTS* FIND FOOD AND\n"
  "CARRY IT HOME: *3 FOOD*\n"
  "HATCHES A NEW ANT\n"
  "EVERY 4TH ANT IS A *SOLDIER*:\n"
  "WITH 8 ANTS THEY MARCH ON\n"
  "THE RED NEST\n"
  "ANTS FOLLOW *TRAILS*!",
  "*P* IS POPULARITY: FOOD AND\n"
  "NEW ANTS RAISE IT: DEAD ANTS\n"
  "AND HUNGER CUT IT\n"
  "\n"
  "ELECTION EVERY MINUTE:\n"
  "*P 50 UP*: 8 MP AID\n"
  "*P UNDER 25*: COUP! MP LOST",
  "KILL THE *RED QUEEN* TO WIN:\n"
  "LOSE YOURS AND ITS OVER\n"
  "\n"
  "*START* PAUSE MENU\n"
  "*R* FAST FORWARD\n"
  "\n"
  "*LONG LIVE THE QUEEN!*",
};

// ----- card tiles (built once at boot, above the ANT EYE overlay tiles) and the picture canvas -----
#define BIGF 544                                       // 2x gold title font, 2 tiles per glyph (top, bottom): 544..623
#define BAND 624                                       // the picture: BW x BH tiles: 624..763
#define ORN  764                                       // 764 rule, 765/766 centre diamond, 767/768 pip on/off, 769 arrow
#define BW 28
#define BH 5
enum { C_SKY = 1, C_CREAM, C_GOLD, C_WATER, C_SAND, C_GRASS, C_HILL, C_RED, C_DARK, C_GRAY, C_STEEL, C_PINK, C_WHITE, C_LEAF, C_NAVY };
static void tdraw(u16 t, const char *s) {             // 8x8 tile from 64 hex digits, row by row, leftmost pixel first
  u8 y, x; u32 w; char c;
  for (y = 0; y < 8; y++) {
    for (w = 0, x = 0; x < 8; x++) { c = s[y * 8 + x]; w |= (u32)(c > '9' ? c - 'a' + 10 : c - '0') << (4 * x); }
    TILE32[t * 8 + y] = w;
  }
}
static void card_init(void) {
  static const u16 PIC[16] = {0, RGB(4,7,18), RGB(31,29,22), RGB(31,27,8), RGB(8,16,28), RGB(28,24,14), RGB(8,20,6), RGB(14,9,5),
    RGB(26,3,3), RGB(2,2,2), RGB(14,14,14), RGB(11,11,22), RGB(31,12,8), RGB(31,31,31), RGB(4,14,5), RGB(1,1,5)};
  u8 g, x, y, i, p[16][8], px, py;
  u32 w;
  BGPAL[145] = RGB(1,1,5); BGPAL[146] = RGB(22,12,3); BGPAL[147] = RGB(31,27,8);       // bank 9: gold on night (titles, highlights, rules)
  BGPAL[161] = RGB(1,1,5); BGPAL[162] = RGB(8,8,16);  BGPAL[163] = RGB(15,15,26);      // bank 10: dim steel (small labels)
  for (i = 0; i < 16; i++) BGPAL[112 + i] = PIC[i];                                    // bank 7: the picture palette (see C_*)
  for (g = 0; g < 40; g++) {                                                           // big font: the 3x5 glyph doubled, shadow then face
    for (y = 0; y < 16; y++) for (x = 0; x < 8; x++) p[y][x] = 1;
    for (i = 0; i < 2; i++)
      for (y = 0; y < 5; y++) for (x = 0; x < 3; x++)
        if ((FONT[g] >> (3 * (4 - y))) & (4 >> x)) {
          px = (u8)(x * 2 + (i ? 0 : 1)); py = (u8)(y * 2 + 2 + (i ? 0 : 1));
          p[py][px] = p[py][px + 1] = p[py + 1][px] = p[py + 1][px + 1] = i ? 3 : 2;
        }
    for (y = 0; y < 16; y++) {
      for (w = 0, x = 0; x < 8; x++) w |= (u32)p[y][x] << (4 * x);
      TILE32[(BIGF + g * 2 + (y >> 3)) * 8 + (y & 7)] = w;
    }
  }
  tdraw(ORN,     "11111111" "11111111" "11111111" "22222222" "22222222" "11111111" "11111111" "11111111");
  tdraw(ORN + 1, "11111113" "11111133" "11111333" "22223333" "22223333" "11111333" "11111133" "11111113");
  tdraw(ORN + 2, "31111111" "33111111" "33311111" "33332222" "33332222" "33311111" "33111111" "31111111");
  tdraw(ORN + 3, "11111111" "11111111" "11333311" "11333311" "11333311" "11333311" "11111111" "11111111");
  tdraw(ORN + 4, "11111111" "11111111" "11222211" "11222211" "11222211" "11222211" "11111111" "11111111");
  tdraw(ORN + 5, "11111111" "13111111" "13311111" "13331111" "13333111" "13331111" "13311111" "13111111");
}

// ----- the picture band. Each card draws its static background ONCE into bgbuf (EWRAM). Every frame tut_anim copies it to sbuf,
// paints the moving parts on top, and band_put() DMAs the result into the band tiles during VBlank: smooth, no tearing -----
#ifdef __arm__
#define EWRAM __attribute__((section(".ewram")))
#else
#define EWRAM
#endif
EWRAM static u32 bgbuf[BW * BH * 8], sbuf[BW * BH * 8];
static u32 *cur = bgbuf;
static int bclip = BW * 8;
static int iabs(int a) { return a < 0 ? -a : a; }
static int tease(int t, int len) {                    // smoothstep: 0..1000 over len frames
  int u; if (t <= 0) return 0; if (t >= len) return 1000;
  u = t * 1000 / len; return (u * u / 1000) * (3000 - 2 * u) / 1000;
}
IWRAM static void bpx(int x, int y, u8 c) {
  u32 *d; u8 s;
  if ((unsigned)x >= (unsigned)bclip || (unsigned)y >= BH * 8) return;
  d = cur + ((y >> 3) * BW + (x >> 3)) * 8 + (y & 7); s = (u8)((x & 7) * 4);
  *d = (*d & ~(0xFu << s)) | ((u32)c << s);
}
IWRAM static void brect(int x, int y, int w, int h, u8 c) {
  int i, j, xe = x + w, ye = y + h; u32 wc = (u32)c * 0x11111111u;
  if (x < 0) x = 0; if (y < 0) y = 0; if (xe > bclip) xe = bclip; if (ye > BH * 8) ye = BH * 8;
  for (j = y; j < ye; j++)
    for (i = x; i < xe; ) {
      if (!(i & 7) && i + 8 <= xe) { cur[((j >> 3) * BW + (i >> 3)) * 8 + (j & 7)] = wc; i += 8; }
      else { bpx(i, j, c); i++; }
    }
}
static u8 bget(int x, int y) {
  if ((unsigned)x >= BW * 8 || (unsigned)y >= BH * 8) return 0;
  return (u8)((bgbuf[((y >> 3) * BW + (x >> 3)) * 8 + (y & 7)] >> ((x & 7) * 4)) & 15);
}
static void band_put(void) {                          // sbuf -> band tiles (call in VBlank)
  DMAR(3, 0) = (u32)sbuf; DMAR(3, 4) = (u32)(TILE32 + BAND * 8); DMAR(3, 8) = DMA_ON | DMA_W32 | (BW * BH * 8);
}
static void bbox(int x, int y, int w, int h, u8 c) { brect(x, y, w, 1, c); brect(x, y + h - 1, w, 1, c); brect(x, y, 1, h, c); brect(x + w - 1, y, 1, h, c); }
static void bdisc(int cx, int cy, int r, u8 c) { int i, j; for (j = -r; j <= r; j++) for (i = -r; i <= r; i++) if (i * i + j * j <= r * r + r) bpx(cx + i, cy + j, c); }
static void bline(int x0, int y0, int x1, int y1, u8 c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, e2;
  for (;;) {
    bpx(x0, y0, c); if (x0 == x1 && y0 == y1) break;
    e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
static void btri(int x, int y, int w, int h, u8 c) {
  int k, hw; for (k = 0; k < h; k++) { hw = h > 1 ? k * w / (2 * (h - 1)) : 0; brect(x + w / 2 - hw, y + k, 2 * hw + 1, 1, c); }
}
static void bdown(int x, int y, int w, int h, u8 c) {
  int k, hw; for (k = 0; k < h; k++) { hw = h > 1 ? (h - 1 - k) * w / (2 * (h - 1)) : 0; brect(x + w / 2 - hw, y + k, 2 * hw + 1, 1, c); }
}
static void btext(int x, int y, const char *s, u8 sc, u8 c) {
  u16 g; int gx, gy;
  for (; *s; s++, x += 4 * sc) {
    g = FONT[gi(*s)];
    for (gy = 0; gy < 5; gy++) for (gx = 0; gx < 3; gx++) if ((g >> (3 * (4 - gy))) & (4 >> gx)) brect(x + gx * sc, y + gy * sc, sc, sc, c);
  }
}
static int blen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void bctext(int cx, int y, const char *s, u8 sc, u8 c) { btext(cx - (blen(s) * 4 - 1) * sc / 2, y, s, sc, c); }
static void bplus(int x, int y, u8 c) { brect(x - 1, y, 3, 1, c); brect(x, y - 1, 1, 3, c); }
static void bsparkle(int x, int y, u8 c) { brect(x - 2, y, 5, 1, c); brect(x, y - 2, 1, 5, c); }
static void bbtn(int cx, int cy, char ch, int pr) {
  char t[2]; t[0] = ch; t[1] = 0; if (pr) cy++;
  bdisc(cx, cy, 9, C_STEEL); bdisc(cx, cy, 8, pr ? C_WHITE : C_GOLD); btext(cx - 2, cy - 4, t, 2, C_NAVY);
}
static void bpill(int cx, int y, const char *s, u8 fill) {
  int w = blen(s) * 4 + 5, x = cx - w / 2;
  brect(x + 4, y, w - 8, 9, fill); bdisc(x + 4, y + 4, 4, fill); bdisc(x + w - 5, y + 4, 4, fill);
  btext(x + 3, y + 2, s, 1, C_NAVY);
}
static void barrow(int x, int y, u8 d, u8 c) {
  int k;
  if (d == 0) { brect(x - 5, y - 1, 7, 3, c); for (k = 0; k < 4; k++) brect(x + 1 + k, y - 3 + k, 1, 7 - 2 * k, c); }
  else if (d == 1) { brect(x - 1, y - 5, 3, 7, c); for (k = 0; k < 4; k++) brect(x - 3 + k, y + 1 + k, 7 - 2 * k, 1, c); }
  else { brect(x - 1, y - 1, 3, 7, c); for (k = 0; k < 4; k++) brect(x - 3 + k, y - 1 - k, 7 - 2 * k, 1, c); }
}
static void bground(int y0, u8 c) {
  int x, y; brect(0, y0, BW * 8, BH * 8 - y0, c);
  if (c == C_GRASS) for (y = y0 + 2; y < BH * 8; y += 4) for (x = (y * 3) % 7; x < BW * 8; x += 7) bpx(x, y, C_LEAF);
}
static void bant(int x, int y, int d, u8 body, u8 leg, u8 crown, int ph) {
  int k, fx;
  for (k = -1; k <= 1; k++) { fx = k * 5 + ((((ph + k) & 3) < 2) ? -1 : 1) * 2; bline(x, y + 2, x + fx, y + 9, leg); }
  bline(x + 8 * d, y - 2, x + 12 * d, y - 7 + ((ph >> 1) & 1) * 2, leg); bline(x + 8 * d, y - 2, x + 11 * d, y - 3 + (ph & 1), leg);
  bdisc(x - 9 * d, y, 5, body); bdisc(x, y, 3, body); bdisc(x + 6 * d, y - 1, 3, body);
  if (crown) { brect(x - 4, y - 9, 9, 3, C_GOLD); bpx(x - 4, y - 11, C_GOLD); bpx(x, y - 12, C_GOLD); bpx(x + 4, y - 11, C_GOLD); bpx(x, y - 9, C_RED); }
}
static void bwave(int x, int y, int s) { bpx(x + s, y + 2, C_WHITE); bpx(x + s + 1, y + 1, C_WHITE); bpx(x + s + 2, y, C_WHITE); bpx(x + s + 3, y + 1, C_WHITE); bpx(x + s + 4, y + 2, C_WHITE); }
static void bmana(int n, int fl, int all) {
  int k; for (k = 0; k < 10; k++) brect(36 + k * 17, 5, 15, 11, k >= n ? C_STEEL : (k == fl || all) ? C_WHITE : C_GOLD);
}
static void ant_spr(u8 s, int x, int y, u16 fr, u8 pal) { spr(s, 8 + x, 32 + y, (u8)(3 + (((fr >> 3) + s) & 1)), pal, 0, 0); }

static const char MINI[4][19] = {"222211112222333322", "222110011122233322", "232210001222223222", "233221000112222222"};
static void mpos(int p, int *x, int *y) {
  if (p < 8) { *x = 4 + p; *y = 0; } else if (p < 11) { *x = 11; *y = p - 7; } else if (p < 18) { *x = 11 - (p - 10); *y = 3; } else { *x = 4; *y = 3 - (p - 17); }
}
static void tut_art(u8 i) {
  static const char *const LV[4] = {"0 WATER", "1 SAND", "2 GRASS", "3 HILL"};
  int k, x, y; u8 c; char ch;
  cur = bgbuf; bclip = BW * 8;
  for (k = 0; k < BW * BH * 8; k++) bgbuf[k] = 0x11111111u;
  switch (i) {
  case 0:
    bdisc(112, 9, 5, C_GOLD); btri(88, 18, 48, 12, C_HILL); btri(60, 22, 30, 8, C_HILL); bground(30, C_GRASS);
    break;
  case 1:
    for (y = 0; y < 4; y++) for (x = 0; x < 18; x++) {
      ch = MINI[y][x]; c = ch == '0' ? C_WATER : ch == '1' ? C_SAND : ch == '2' ? C_GRASS : C_HILL;
      brect(64 + x * 8, 4 + y * 8, 8, 8, c);
    }
    bbox(63, 3, 146, 34, C_STEEL);
    brect(24, 4, 8, 8, C_GRAY); brect(16, 12, 8, 8, C_GRAY); brect(24, 12, 8, 8, C_STEEL); brect(32, 12, 8, 8, C_GRAY); brect(24, 20, 8, 8, C_GRAY);
    btri(26, 6, 5, 3, C_WHITE); bdown(26, 22, 5, 3, C_WHITE);
    for (k = 0; k < 3; k++) { bpx(20 - k, 14 + k, C_WHITE); bpx(20 - k, 18 - k, C_WHITE); bpx(35 + k, 14 + k, C_WHITE); bpx(35 + k, 18 - k, C_WHITE); }
    bctext(28, 31, "D PAD", 1, C_CREAM);
    break;
  case 2:
    for (k = 0; k < 4; k++) {
      c = (u8)(C_WATER + k); x = 8 + k * 32; y = 31 - 6 * (k + 1);
      brect(x, y, 30, 6 * (k + 1), c); brect(x, y, 30, 1, C_WHITE);
      bctext(x + 15, 33, LV[k], 1, C_CREAM);
    }
    btext(192, 20, "1 LEVEL", 1, C_CREAM); btext(192, 27, "1 MP", 1, C_GOLD);
    break;
  case 3:
    bground(26, C_GRASS);
    break;
  case 4:
    btext(8, 5, "MP", 2, C_GOLD);
    bdisc(70, 26, 4, C_WATER); btri(67, 17, 7, 6, C_WATER); bctext(70, 33, "TRICKLE", 1, C_CREAM);
    bdisc(112, 25, 5, C_LEAF); bdisc(111, 24, 3, C_GRASS); brect(111, 18, 2, 3, C_HILL); bctext(112, 33, "FOOD", 1, C_CREAM);
    brect(154, 25, 15, 8, C_GRAY); brect(157, 19, 9, 7, C_CREAM); brect(157, 25, 9, 1, C_NAVY); bctext(162, 33, "ELECTION", 1, C_CREAM);
    break;
  case 5:
    bctext(24, 33, "8 MP", 1, C_GOLD);
    for (y = 0; y < 3; y++) for (x = 0; x < 3; x++) { brect(58 + x * 12, 2 + y * 12, 11, 11, C_GRASS); brect(138 + x * 12, 2 + y * 12, 11, 11, C_WATER); }
    break;
  case 6:
    bclip = 140; bground(13, C_GRASS); bclip = BW * 8;
    brect(140, 0, 1, 40, C_STEEL); brect(179, 17, 3, 1, C_GOLD); brect(180, 16, 1, 3, C_GOLD);
    break;
  case 7:
    bground(28, C_GRASS);
    for (k = 0; k < 3; k++) { bdisc(18 + k * 6, 25 - (k & 1) * 2, 3, C_LEAF); bdisc(18 + k * 6, 24 - (k & 1) * 2, 1, C_GRASS); }
    btri(158, 10, 52, 18, C_HILL); bdisc(184, 27, 5, C_NAVY); brect(179, 27, 11, 2, C_NAVY);
    break;
  case 8:
    brect(16, 13, 192, 1, C_WHITE); brect(16, 24, 192, 1, C_WHITE);
    bctext(16, 29, "0", 1, C_CREAM); bctext(64, 29, "25", 1, C_CREAM); bctext(112, 29, "50", 1, C_CREAM); bctext(208, 29, "100", 1, C_CREAM);
    brect(64, 24, 1, 4, C_CREAM); brect(112, 24, 1, 4, C_CREAM);
    break;
  default:
    brect(90, 22, 44, 9, C_GOLD); brect(90, 29, 44, 2, C_HILL);
    btri(90, 9, 11, 13, C_GOLD); btri(106, 4, 11, 18, C_GOLD); btri(123, 9, 11, 13, C_GOLD);
    bdisc(95, 8, 1, C_WHITE); bdisc(112, 3, 1, C_WHITE); bdisc(129, 8, 1, C_WHITE);
    bground(35, C_GRASS);
    break;
  }
  bbox(0, 0, BW * 8, BH * 8, C_STEEL);
}
static void tut_anim(u8 i, u16 fr) {
  int f = fr, k, x, y, t, u; u8 c;
  cur = sbuf; bclip = BW * 8;
  for (k = 0; k < BW * BH * 8; k++) sbuf[k] = bgbuf[k];
  switch (i) {
  case 0: {
    static const u8 SX[10] = {10, 34, 58, 84, 140, 166, 190, 212, 70, 150}, SY[10] = {5, 12, 3, 9, 4, 11, 6, 3, 2, 8};
    static const s8 RX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, RY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    for (k = 0; k < 10; k++) {
      t = ((f / 9) + k * 3) & 7;
      if (t == 0) bsparkle(SX[k], SY[k], C_WHITE); else if (t < 4) bpx(SX[k], SY[k], C_CREAM); else bpx(SX[k], SY[k], C_STEEL);
    }
    for (k = 0; k < 8; k++) for (u = 7; u <= 8 + ((((f / 6) + k) & 1) * 3); u++) bpx(112 + RX[k] * u, 9 + RY[k] * u, C_GOLD);
    t = f % 300;
    if (t < 18) for (u = 0; u < 5; u++) bpx(200 - t * 7 + u * 2, 1 + t - u, u == 0 ? C_WHITE : u < 3 ? C_CREAM : C_STEEL);
    bant(26, 24 - ((f / 24) & 1), 1, C_DARK, C_GRAY, 1, f / 8);
    bant(198, 24 - (((f + 12) / 24) & 1), -1, C_RED, C_PINK, 1, f / 8 + 2);
    if (f % 50 < 6) bsparkle(26, 9, C_WHITE);
    if ((f + 25) % 50 < 6) bsparkle(198, 9, C_WHITE);
    for (k = 0; k < 3; k++) {
      ant_spr((u8)k, 44 + (f / 2 + k * 21) % 63, 27, fr, 0);
      ant_spr((u8)(3 + k), 170 - (f / 2 + k * 21) % 63, 27, fr, 1);
    }
    break; }
  case 1: {
    int p0 = (f / 7) % 20, px, py;
    mpos((p0 + 18) % 20, &x, &y); bbox(64 + x * 8, 4 + y * 8, 8, 8, C_GRAY);
    mpos((p0 + 19) % 20, &x, &y); bbox(64 + x * 8, 4 + y * 8, 8, 8, C_CREAM);
    if (f % 7 < 4) {
      if (p0 >= 1 && p0 <= 7) { brect(32, 12, 8, 8, C_GOLD); for (k = 0; k < 3; k++) { bpx(35 + k, 14 + k, C_NAVY); bpx(35 + k, 18 - k, C_NAVY); } }
      else if (p0 >= 8 && p0 <= 10) { brect(24, 20, 8, 8, C_GOLD); bdown(26, 22, 5, 3, C_NAVY); }
      else if (p0 >= 11 && p0 <= 17) { brect(16, 12, 8, 8, C_GOLD); for (k = 0; k < 3; k++) { bpx(20 - k, 14 + k, C_NAVY); bpx(20 - k, 18 - k, C_NAVY); } }
      else { brect(24, 4, 8, 8, C_GOLD); btri(26, 6, 5, 3, C_NAVY); }
    }
    mpos(p0, &px, &py);
    spr(0, 8 + 64 + px * 8, 32 + 4 + py * 8 - (f % 7 < 2), 1, 2, 0, 0);
    break; }
  case 2: {
    int s = (f / 40) % 5, ux, ay, pr;
    u = f % 40; pr = (s < 4 && u < 8);
    for (k = 0; k < 3; k++) bpx(8 + ((f / 3 + k * 9) % 28), 26 + (k & 1) * 2, C_WHITE);
    if (s < 4 && u < 10) brect(8 + s * 32, 31 - 6 * (s + 1) - (u < 6 ? 2 : 0), 30, 2, (u & 2) ? C_CREAM : C_WHITE);
    bbtn(174, 24, 'A', pr);
    barrow(174, 8 - ((f / 8) & 1), 2, C_GOLD);
    if (s < 4 && u < 24) { bplus(148, 24 - u, C_GOLD); btext(151, 22 - u, "1", 1, C_GOLD); }
    if (s < 4) {
      if (s == 0 || u >= 12) { ux = 8 + s * 32 + 11; ay = 31 - 6 * (s + 1) - 7; }
      else { ux = 8 + (s - 1) * 32 + 11 + 32 * u / 12; ay = 31 - 6 * s - 7 - 6 * u / 12 - u * (12 - u) / 6; }
      ant_spr(0, ux, ay, fr, 0);
    } else {
      ant_spr(0, 8 + 3 * 32 + 8, 31 - 24 - 7 - (((f / 6) & 1) * 2), fr, 0);
      brect(126, 1, 1, 6, C_CREAM); brect(127, 1, 5 - ((f / 6) & 1), 3, C_RED);
      if ((f / 5) & 1) bsparkle(100, 4, C_WHITE); else bsparkle(140, 6, C_GOLD);
    }
    break; }
  case 3: {
    int T = f % 280, ax, dd, wy, x0 = 86, w = 44, mv = 1;
    bant(180, 18, -1, C_DARK, C_GRAY, 1, f / 8);
    if (T >= 64 && T < 240) {
      dd = (T - 64) * 14 / 12; if (dd > 14) dd = 14;
      if (T >= 220) { u = T - 220; x0 = 86 + u * 22 / 20; w = 44 - u * 44 / 20; }
      if (w > 0) {
        brect(x0, 26, w, dd, C_HILL);
        if (T >= 76) {
          wy = 40 - (T - 76) * 12 / 16; if (wy < 28) wy = 28;
          brect(x0, wy, w, 40 - wy, C_WATER);
          if (w > 6 && wy <= 30) for (k = 0; k < 4; k++) { bwave(x0 + (k * 11 + f / 3) % (w - 5), 30, 0); bwave(x0 + (k * 11 + 5 + f / 2) % (w - 5), 35, 0); }
        }
      }
      if (T < 78) for (k = 0; k < 6; k++) bpx(86 + (T * 7 + k * 13) % 44, 25 - (T - 64) / 2 - k % 3, C_SAND);
    }
    bbtn(108, 11, 'B', T >= 60 && T < 76);
    if (T >= 62 && T < 92) { brect(128, 18 - (T - 62) / 3, 3, 1, C_GOLD); btext(132, 16 - (T - 62) / 3, "1", 1, C_GOLD); }
    if (T < 60) ax = 4 + T; else if (T < 92) { ax = 64; mv = 0; } else if (T < 104) ax = 64 + (T - 92); else if (T < 140) { ax = 76; mv = 0; }
    else if (T < 210) ax = 76 - (T - 140) * 80 / 70; else ax = -100;
    if (ax > -8) ant_spr(0, ax, 18, mv ? fr : 0, 1); else spr_hide(0);
    if ((T >= 70 && T < 92) || (T >= 104 && T < 140)) { brect(ax - 1, 4, 9, 10, C_WHITE); bpx(ax + 3, 14, C_WHITE); btext(ax + 3, 6, "!", 1, C_RED); }
    if (T >= 140 && T < 200 && ((f / 5) & 1)) bsparkle(180, 3, C_GOLD);
    break; }
  case 4: {
    static const u8 SXP[3] = {70, 112, 162}, SYP[3] = {16, 18, 19};
    static const u8 CORE[3] = {C_WHITE, C_GRASS, C_CREAM}, TRAIL[3] = {C_STEEL, C_LEAF, C_STEEL};
    int w = (f / 30) % 10, uq = f % 30, n, fl = -1, tx, ty, sx, sy, j, q, px, py;
    if (w < 8) n = 2 + w; else if (w == 8) n = 10; else n = 10 - uq * 8 / 30;
    if (w >= 1 && w <= 7 && uq < 8) fl = n - 1;
    bmana(n, fl, w == 8 && ((uq / 4) & 1));
    if (w < 8) {
      sx = SXP[w % 3]; sy = SYP[w % 3]; tx = 36 + n * 17 + 7; ty = 11;
      for (j = 2; j >= 0; j--) {
        q = uq - j * 3; if (q < 0) continue;
        px = sx + (tx - sx) * q / 30; py = sy + (ty - sy) * q / 30 - q * (30 - q) * 12 / 225;
        if (j) bpx(px, py, TRAIL[w % 3]); else bdisc(px, py, 1, CORE[w % 3]);
      }
      if (uq < 8) bsparkle(sx, sy - 5, C_WHITE);
    }
    if (w == 8 && (uq & 4)) { bsparkle(60, 3, C_WHITE); bsparkle(190, 18, C_WHITE); }
    bpx(69, 22 + (f / 6) % 4, C_WHITE);
    break; }
  case 5: {
    int T = f % 210, cx, cy, d, tf, ax, ay;
    bbtn(24, 20, 'L', T >= 28 && T < 40);
    barrow(112 + ((f / 6) & 1) * 2, 20, 0, C_GOLD);
    for (cy = 0; cy < 3; cy++) for (cx = 0; cx < 3; cx++) {
      d = iabs(cx - 1) + iabs(cy - 1); tf = 40 + d * 9; x = 58 + cx * 12; y = 2 + cy * 12;
      if (T >= tf && T < 190) {
        u = T - tf; brect(x, y, 11, 11, u < 5 ? C_WHITE : C_WATER);
        if (u >= 5) { bwave(x + 1, y + 3, ((f / 8 + cx + cy) % 3) * 2); bwave(x + 1, y + 8, ((f / 8 + cx + cy + 1) % 3) * 2); }
      }
      bwave(139 + cx * 12, 5 + cy * 12, ((f / 8 + cx * 2 + cy) % 3) * 2); bwave(139 + cx * 12, 10 + cy * 12, ((f / 8 + cx + cy * 2) % 3) * 2);
    }
    for (k = 0; k < 2; k++) {
      cx = k ? 2 : 0; cy = k ? 2 : 1; d = iabs(cx - 1) + iabs(cy - 1); tf = 40 + d * 9;
      ax = 58 + cx * 12 + 1 + ((f / 10 + k) & 1); ay = 2 + cy * 12 + 2;
      if (T < tf) ant_spr((u8)k, ax, ay, fr, 0);
      else {
        spr_hide((u8)k); u = T - tf;
        if (u < 28) for (t = 0; t < 3; t++) bdisc(58 + cx * 12 + 3 + t * 3, 2 + cy * 12 + 8 - u / 2 - t * 3, t == 1, C_WHITE);
      }
    }
    for (k = 0; k < 4; k++) { u = (f / 2 + k * 8) % 24; bpx(184 + k * 6, 30 - u, C_WHITE); if (u > 1) bpx(184 + k * 6, 31 - u, C_STEEL); }
    break; }
  case 6: {
    static const s8 PLX[4] = {-16, 12, 3, -5}; static const u8 PLO[4] = {0, 75, 150, 225};
    int sw, x0, e, z, s, gy, h, w, ph = f % 240;
    sw = (ph < 120 ? ph : 240 - ph) - 60; x0 = 70 - sw / 3;
    bclip = 140;
    for (k = 0; k < 3; k++) { x = (f / 6 + k * 70) % 200 - 30 - sw / 2; bdisc(x, 4 + (k & 1) * 3, 3, C_STEEL); bdisc(x + 5, 5 + (k & 1) * 3, 2, C_STEEL); }
    for (k = 0; k < 9; k++) bline(x0, 13, -60 + k * 35 - sw, 40, C_LEAF);
    for (k = 0; k < 6; k++) { z = k * 20 + 20 - (f % 20); y = 13 + 560 / z; if (y < 40) brect(0, y, 140, 1, C_LEAF); }
    for (k = 0; k < 4; k++) {
      e = (f + PLO[k]) % 300; z = 140 - e * 4 / 10; s = 400 / z;
      gy = 13 + 7 * s / 5; h = s / 2 + 1; w = s / 8 + 1; x = x0 + PLX[k] * s / 10;
      if (k == 3) { h *= 2; brect(x - w / 2, gy - h, w, h, C_PINK); brect(x - w / 2 - 1, gy - h, w + 2, h / 5 + 1, C_GOLD); }
      else brect(x - w / 2, gy - h, w, h, C_RED);
    }
    bclip = BW * 8;
    ph = f % 180;
    bpill(180, 7, "SEL", ph < 110 ? C_WHITE : C_GOLD);
    bpill(180, 25, "START", (ph >= 50 && ph < 66) ? C_WHITE : C_GOLD);
    if (ph >= 66 && ph < 80) bbox(1, 1, 138, 38, C_GOLD);
    break; }
  case 7: {
    static char tb[] = "FOOD 0/3";
    int j = (f + 10) / 40, r = j % 3, n = j == 0 ? 0 : (r == 0 ? 3 : r), s = (f + 10) % 120, ax;
    for (k = 0; k < 4; k++) {
      u = (f + 40 * (k + 1)) % 160;
      if (u < 70) { ax = 20 + u * 11 / 5; ant_spr((u8)k, ax, 23, fr, 0); brect(ax + 2, 20, 3, 2, C_GRASS); bpx(ax + 3, 19, C_LEAF); }
      else if (u < 80) { spr_hide((u8)k); if (u < 76) bsparkle(184, 20, C_GOLD); }
      else if (u < 150) ant_spr((u8)k, 174 - (u - 80) * 11 / 5, 23, fr, 0);
      else ant_spr((u8)k, 20, 23, 0, 0);
    }
    tb[5] = (char)('0' + n); bctext(112, 7, tb, 1, C_CREAM);
    bctext(112, 14, "NEW ANT", 1, (j >= 3 && s < 70) ? C_GOLD : C_STEEL);
    spr_hide(4);
    if (j >= 3) {
      if (s < 26) {
        x = 182 + ((s > 8 && ((s / 3) & 1)) ? 1 : 0); brect(x, 22, 5, 7, C_WHITE); brect(x + 1, 21, 3, 1, C_WHITE); brect(x + 1, 29, 3, 1, C_WHITE);
        if (s > 16) { bpx(183, 24, C_DARK); bpx(184, 25, C_DARK); bpx(185, 24, C_DARK); }
      } else if (s < 95) {
        ax = 176 - (s - 26) * 11 / 5; ant_spr(4, ax, 23, fr, 2);
        bpx(ax + 9 + f % 5, 22 + f % 3, C_GOLD); bpx(ax + 12 + f % 4, 25 + f % 2, C_WHITE);
      }
    }
    break; }
  case 8: {
    int T = f % 420, v, xm, coup, aid;
    if (T < 60) v = 62; else if (T < 120) v = 62 - 44 * tease(T - 60, 60) / 1000; else if (T < 180) v = 18;
    else if (T < 260) v = 18 + 23 * tease(T - 180, 80) / 1000; else if (T < 300) v = 41;
    else if (T < 360) v = 41 + 37 * tease(T - 300, 60) / 1000; else v = 78 - 16 * tease(T - 360, 60) / 1000;
    xm = 16 + v * 192 / 100; coup = v < 25; aid = v >= 50;
    brect(16, 14, 48, 10, (coup && ((f / 5) & 1)) ? C_PINK : C_RED); brect(64, 14, 48, 10, C_GRAY); brect(112, 14, 96, 10, C_GRASS);
    if (aid) brect(112 + (f * 2) % 92, 14, 4, 10, C_CREAM);
    bctext(40 + (coup ? ((f / 2) & 1) * 2 - 1 : 0), 17, coup ? "COUP!" : "COUP", 1, C_WHITE);
    bctext(88, 17, "NOTHING", 1, C_CREAM); bctext(160, 17, "8 MP AID", 1, C_WHITE);
    if (aid) for (k = 0; k < 3; k++) { u = (f / 2 + k * 5) % 14; bplus(130 + k * 30, 12 - u, C_GRASS); }
    if (coup) for (k = 0; k < 3; k++) btext(30 + k * 18 + (((f + k) / 3) & 1), 5 + (((f + k * 2) / 4) & 1), "!", 1, C_RED);
    bdisc(xm, 5, 4, C_GOLD); btext(xm - 1, 3, "P", 1, C_NAVY); bdown(xm - 3, 9, 7, 4, C_GOLD);
    break; }
  default: {
    static const u8 GX[8] = {60, 46, 76, 150, 170, 184, 40, 192}, GY[8] = {10, 28, 15, 10, 26, 12, 12, 26};
    static const u8 CC[6] = {C_RED, C_GOLD, C_GRASS, C_PINK, C_WHITE, C_CREAM};
    for (k = 0; k < 28; k++) {
      x = (k * 37 + k * k * 11) % 224 + (((f / 6) + k) & 3) - 1; y = (f * (1 + k % 3)) / 3 % 46 + (k * 7) % 46; y = y % 46 - 4;
      brect(x, y, 2, (k & 1) ? 3 : 2, CC[k % 6]);
    }
    t = f % 90;
    if (t < 56) for (y = 4; y < 31; y++) { x = 84 + t + (31 - y) * 3 / 5; for (u = 0; u < 3; u++) if (bget(x + u, y) == C_GOLD) bpx(x + u, y, C_WHITE); }
    c = (u8)((f / 12) & 1);
    bdisc(95, 26, 2, c ? C_PINK : C_RED); bdisc(112, 26, 2, c ? C_WHITE : C_WATER); bdisc(129, 26, 2, c ? C_RED : C_PINK);
    for (k = 0; k < 8; k++) { t = (f + k * 13) % 48; if (t < 6) bsparkle(GX[k], GY[k], C_WHITE); else if (t < 12) bplus(GX[k], GY[k], C_GOLD); }
    for (k = 0; k < 2; k++) { t = (f + k * 20) % 40; ant_spr((u8)k, k ? 160 : 58, 27 - (t < 20 ? t * (20 - t) / 16 : 0), fr, 0); }
    break; }
  }
}
static u8 tview;                                       // 1 while the pause menu is browsing the lesson cards (labels change)
static const char *tps; static u8 tpx, tpy, tpg;       // lesson text types itself in
static void tut_type(u8 n) {
  while (n && *tps) {
    if (*tps == '\n') { tpy++; tpx = 1; } else if (*tps == '*') tpg ^= 1; else { pc(tpx++, tpy, *tps, tpg ? 9 : 3); n--; }
    tps++;
  }
}
static void tut_draw(u8 i) {
  u8 x, y, g, n, len = 0;
  hide_all();
  ov_fill(1, 3, 0, 19);
  if (i == 0) ps(1, 0, "A QUICK TOUR", 10);
  else if (i == TN - 1) ps(1, 0, "ALL DONE", 10);
  else { ps(1, 0, "LESSON", 10); pc(8, 0, (char)('0' + i), 10); pc(9, 0, '/', 10); pc(10, 0, '8', 10); }
  for (n = 0; n < TN; n++) BGMAP1[19 + n] = (u16)((ORN + (n <= i ? 3 : 4)) | (9 << 12));
  while (TTITLE[i][len]) len++;
  for (n = 0; n < len; n++) {
    g = gi(TTITLE[i][n]); x = (u8)((30 - len) / 2 + n);
    BGMAP1[32 + x] = (u16)((BIGF + g * 2) | (9 << 12)); BGMAP1[64 + x] = (u16)((BIGF + g * 2 + 1) | (9 << 12));
  }
  for (x = 1; x < 29; x++) { BGMAP1[3 * 32 + x] = (u16)(ORN | (9 << 12)); BGMAP1[17 * 32 + x] = (u16)(ORN | (9 << 12)); }
  BGMAP1[3 * 32 + 14] = BGMAP1[17 * 32 + 14] = (u16)((ORN + 1) | (9 << 12));
  BGMAP1[3 * 32 + 15] = BGMAP1[17 * 32 + 15] = (u16)((ORN + 2) | (9 << 12));
  tut_art(i); tut_anim(i, 0); band_put();
  for (y = 0; y < BH; y++) for (x = 0; x < BW; x++) BGMAP1[(4 + y) * 32 + 1 + x] = (u16)((BAND + y * BW + x) | (7 << 12));
  tps = TBODY[i]; tpx = 1; tpy = 10; tpg = 0;
  ps(1, 18, "START", 9); ps(7, 18, i == TN - 1 ? (tview ? "BACK" : "PLAY") : "NEXT", 3);
  if (i < TN - 1 || tview) { ps(14, 18, "B", 9); ps(16, 18, tview ? "MENU" : "SKIP", 3); }
}
static void tut_card(u8 i) {
  u16 fr = 0; u8 tl;
  hide_all(); vsync(); oam_flush();
  tut_draw(i);
  while (joy()) vsync();
  for (;;) {
    vsync(); fr++;
    band_put(); oam_flush();
    tut_anim(i, fr);
    tut_type(5);
    tl = (u8)((fr & 63) < 32 ? (fr & 31) : 31 - (fr & 31));
    BGPAL[147] = RGB(31, 25 + tl / 5, 6 + tl / 4);
    BGMAP1[19 + i] = (u16)(((fr & 16) ? ORN + 4 : ORN + 3) | (9 << 12));
    BGMAP1[18 * 32 + 28] = (u16)(((fr & 32) ? 0 : ORN + 5) | (9 << 12));
    if (joy() & J_START) break;
    if ((i < TN - 1 || tview) && (joy() & J_B)) { tut = 0; break; }
  }
  BGPAL[147] = RGB(31, 27, 8);
  while (joy()) vsync();
  hide_all(); vsync(); oam_flush();
  ov_clear(); hud();
}
// ---------- PAUSE MENU (START): live colony status + a menu (UP DOWN, A; START or B resumes) ----------
#define PM_N 6
static const char *const PITEM[PM_N] = {"RESUME", "CONTROLS", "HOW TO PLAY", "MUSIC", "FAST FORWARD", "QUIT TO TITLE"};
static const char *const PHINT[PM_N] = {"BACK TO THE GAME", "BUTTONS AND RULES", "REPLAY THE LESSON CARDS", "MUSIC ON OR OFF", "SPEED UP THE GAME", "GIVE UP AND LEAVE"};
static void pm_rule(u8 y) {
  u8 x; for (x = 1; x < 29; x++) BGMAP1[y * 32 + x] = (u16)(ORN | (9 << 12));
  BGMAP1[y * 32 + 14] = (u16)((ORN + 1) | (9 << 12)); BGMAP1[y * 32 + 15] = (u16)((ORN + 2) | (9 << 12));
}
static void pm_time(u8 x, u8 y, u32 sec) { pn(x, y, (u8)(sec / 60), 3); pc(x + 2, y, ':', 3); pz(x + 3, y, (u8)(sec % 60), 3); }
static void pm_draw(void) {                            // everything except the menu rows
  u8 i, n, x, c = 0, g;
  ov_fill(1, 3, 0, 19);
  ps(1, 0, "THE COLONY WAITS", 10); ps(sandbox ? 22 : 24, 0, sandbox ? "SANDBOX" : DNAME[diff], 10);
  for (n = 0; n < 6; n++) {
    g = gi("PAUSED"[n]); x = (u8)(12 + n);
    BGMAP1[32 + x] = (u16)((BIGF + g * 2) | (9 << 12)); BGMAP1[64 + x] = (u16)((BIGF + g * 2 + 1) | (9 << 12));
  }
  pm_rule(3); pm_rule(10); pm_rule(17);
  ps(1, 4, "MANA", 9);     pn(6, 4, mana, 3);          ps(16, 4, "FOOD", 9);   pn(23, 4, stock[0], 3);
  ps(1, 5, "ANTS", 9);     pn(6, 5, ncnt[0], 3);       ps(16, 5, "FOES", 9);   pn(23, 5, ncnt[1], 3);
  ps(1, 6, "POPULAR", 9);  pn(9, 6, appr, 3);          ps(16, 6, "QUEENS", 9);
  pc(23, 6, (char)('0' + qhp[0]), 3); pc(24, 6, '/', 3); pc(25, 6, (char)('0' + qhp[1]), 3);
  ps(1, 7, "ELECTION", 9); pm_time(10, 7, (u32)(450 - etk) * 2 / 15);   ps(16, 7, "TIME", 9); pm_time(23, 7, (u32)gt * 2 / 15);
  ps(1, 8, "PERKS", 9);
  for (i = 0; i < 6; i++) if ((perk >> i) & 1) {
    for (n = 0; n < 7; n++) pc((u8)(7 + (c % 3) * 8 + n), (u8)(8 + c / 3), PN[i * 7 + n], 3);
    c++;
  }
  if (!c) ps(7, 8, "NONE YET", 10);
  ps(1, 19, "UP DOWN  A OK  START PLAY", 10);
}
static void pm_menu(u8 sel, u8 conf) {
  u8 i, x, y;
  for (i = 0; i < PM_N; i++) {
    y = (u8)(11 + i);
    for (x = 1; x < 29; x++) BGMAP1[y * 32 + x] = (u16)(1 | (3 << 12));
    if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
    ps(3, y, (conf && i == sel) ? "SURE? A YES   B NO" : PITEM[i], i == sel ? 9 : 3);
    if (i == 3) ps(23, y, nomus ? "OFF" : "ON", 9);
    if (i == 4) ps(23, y, ff ? "ON" : "OFF", 9);
  }
  for (x = 1; x < 29; x++) BGMAP1[18 * 32 + x] = (u16)(1 | (3 << 12));
  ps(1, 18, PHINT[sel], 3);
}
static void pause_menu(void) {
  u8 sel = 0, conf = 0, run = 1, act, i; u16 k, p, prev = 0;
  hide_all(); vsync(); oam_flush();
  while (joy()) vsync();
  pm_draw(); pm_menu(sel, conf);
  while (run) {
    vsync(); k = joy(); p = k & ~prev; prev = k;
    if (!p) continue;
    if (conf) {                                        // quit confirmation: A = yes, anything else cancels
      if (p & J_A) { over = 3; tut = 0; break; }
      conf = 0; pm_menu(sel, conf); continue;
    }
    if (p & (J_START | J_B)) break;
    if (p & J_UP)   { sel = sel ? sel - 1 : PM_N - 1; sfx_food(); }
    if (p & J_DOWN) { sel = sel == PM_N - 1 ? 0 : sel + 1; sfx_food(); }
    act = (u8)((p & J_A) || ((p & (J_LEFT | J_RIGHT)) && (sel == 3 || sel == 4)));
    if (act) {
      switch (sel) {
      case 0: run = 0; break;
      case 1: help_show(); pm_draw(); prev = joy(); break;
      case 2: {
        u8 sv = tut; tview = 1; tut = 1;               // browse the lesson cards without changing the game
        for (i = 0; i < TN && tut; i++) tut_card(i);
        tut = sv; tview = 0; pm_draw(); prev = joy(); break;
      }
      case 3: nomus ^= 1; if (nomus) music_stop(); else music_start(); sfx_mana(); break;
      case 4: ff ^= 1; sfx_mana(); break;
      default: conf = 1; sfx_deny(); break;
      }
    }
    pm_menu(sel, conf);
  }
  while (joy()) vsync();
  ov_clear();
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
static u16 calc_score(u32 secs) {                 // 300/600/900 by level + speed bonus + 5 per ant + popularity + 10 per queen HP
  u16 sc = (u16)(300 * (diff + 1));
  if (secs < 600) sc = (u16)(sc + 600 - secs);
  return (u16)(sc + ncnt[0] * 5 + appr + qhp[0] * 10);
}
static void pd(u8 x, u8 y, u16 v, u8 p) { u8 i; for (i = 4; i--; v /= 10) pc(x + i, y, (char)('0' + v % 10), p); }
static void end_card(void) {
  u32 s = (u32)gt * 2 / 15; u8 nb = 0, nh = 0; u16 sc = 0;
  ov_fill(1, 1, 18, 19);
  if (over == 1) {
    if (!sandbox && !cheat) {
      if (wins[diff] < 99) wins[diff]++;
      if (!best[diff] || s < best[diff]) { best[diff] = (u16)s; nb = 1; }
      if (sc = calc_score(s), sc > hiscore[diff]) { hiscore[diff] = sc; nh = 1; }
      save_write();
    }
    if (!sc) sc = calc_score(s);
    ps(0, 18, "YOU WIN!", 2); ps(10, 18, "TIME", 2);
    pn(15, 18, (u8)(s / 60 > 99 ? 99 : s / 60), 1); pc(17, 18, ':', 1); pz(18, 18, (u8)(s % 60), 1);
    if (nb) ps(21, 18, "NEW BEST!", 2);
  } else ps(0, 18, "COLONY LOST!", 2);
  if (over == 1) { ps(13, 19, "SCORE", 2); pd(19, 19, sc, 1); if (nh) ps(24, 19, "HIGH!", 2); }
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
    if (p & J_START) { pause_menu(); hud(); prev = joy(); continue; }
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
      if (pend && !msgt && (p & (J_A | J_B))) pick((p & J_A) ? pa : pb);   // offer on the HUD: A / B take a perk
      else { if (p & J_A) raise_land(); if (p & J_B) lower_land(); }
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
      else if (pend && !msgt) { msg = pbuf; msgt = 1; }                          // keep the perk offer on the HUD
      hud();
    }
    if (!qhp[1]) over = 1; else if (!qhp[0]) over = 2;
    draw_sprites();
  }
  music_stop();
  if (over == 3) { fade(0, 16); return; }              // quit from the pause menu: straight back to the title
  end_card();
  if (over == 1) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  vsync(); oam_flush();
  while (1) { vsync(); if (joy() & J_START) break; }
  while (joy()) vsync();
  fade(0, 16);
}

static void gfx_init(void) {                          // palettes, tiles, maps, layers (everything main() needs before the first screen)
  u8 i, x, y; static const u8 BARRM[4] = {1, 2, 2, 3};
  for (i = 0; i < 255; i++) { BGPAL[i] = 0; OBJPAL[i] = 0; }
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
  card_init();
  BG0CNT = 2 | (28 << 8);                             // priority 2, charblock 0, screenblock 28
  BG1CNT = 0 | (29 << 8);                             // priority 0
  hide_all(); oam_flush();
}
int main(void) {
  DISPCNT = 0x80;
  logo_play();                                        // Danny Steel boot logo (START skips)
  IO16(0x0C) = IO16(0x0E) = 0; IO16(0x14) = IO16(0x16) = 0; IO16(0x48) = IO16(0x4A) = 0;   // undo the logo's BG2/3, BG1 scroll, windows
  gfx_init();
  save_load();
  diff = 1;
  for (;;) { title(); newgame(); play(); }
}
