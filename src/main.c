// EMPIRE-ANTS for Game Boy Advance - port of the GBC game. Raw hardware registers, no libraries.
// Mode 0: BG0 = 32x32 world (scrolls), BG1 = HUD / overlay text, OBJ = ants, queens, cursor.
// All graphics come from art.h (the GBC 2bpp tiles, expanded to 4bpp at boot) and the 3x5 font.
// New on GBA: 30x18 tile view, 24 ants per colony, difficulty levels, game timer, SRAM records, L/R shortcuts.
#include <stdint.h>
#include "art.h"
#include "logo.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#include "font8.h"
#include "art2.h"
#include "maps.h"
#ifdef __arm__
#define EWRAM __attribute__((section(".ewram")))
#else
#define EWRAM
#endif

// ---------- hardware ----------
#define IO16(a) (*(volatile u16 *)(0x04000000u + (a)))
#define IO8(a)  (*(volatile u8 *)(0x04000000u + (a)))
#define DISPCNT IO16(0x00)
#define VCOUNT  IO16(0x06)
#define BG0CNT  IO16(0x08)
#define BG1CNT  IO16(0x0A)
#define BG2CNT  IO16(0x0C)
#define BG3CNT_ IO16(0x0E)
#define BG0HOFS IO16(0x10)
#define BG0VOFS IO16(0x12)
#define BG2HOFS IO16(0x18)
#define BG2VOFS IO16(0x1A)
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
#define BGMAP3  ((volatile u16 *)0x0600F800)   // termite UI layer (soil, frames, tunnels), screenblock 31
#define BGMAP2  ((volatile u16 *)0x0600F000)   // decor layer (food, nest mounds, trails), screenblock 30
#define TILE2   ((volatile u32 *)0x06008000)   // charblock 2: terrain + decor tiles (the ANT EYE only uses its first 10 tiles)
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

#define MAXW 128        // row stride of the world arrays (a power of two); the world is at most 96 x 96, chosen on the title screen
#define MAXH 96
#define W mw
#define H mh
#define VW 30           // visible world tiles
#define VH 17           // bottom 3 rows are the HUD
#define MAXA 48         // 24 ants per colony
#define QHP 5
#define MANA_MAX 20
#define MANA_PER_FOOD 1
#define OFFER_FOOD 2
#define OFFER_MANA 4
#define FT 4            // font tiles 4..61 (58 glyphs, see font8.h)
#define NGLYPH 58
#define BT 770          // old 2bpp terrain tiles 770..775 (title ground), AP bar 776 (full) 777 (empty)
#define BAR_F (BT + 6)
#define BAR_E (BT + 7)

typedef struct { u8 x, y, team, alive, carry, sol, ox, oy, age, fl; } Ant;   // ox/oy/age: last cell + slices since the step (smooth walking), fl = faces left
static u8 mw = 32, mh = 32;                                       // world size in cells (32..96 each)
EWRAM static u8 hgt[MAXH][MAXW], food[MAXH][MAXW], ph[MAXH][MAXW];
EWRAM static u16 dbuf[3][MAXH][MAXW];    // three distance maps: one per team in use + one being worked on in the background
static u16 (*dnp[2])[MAXW] = {dbuf[0], dbuf[1]};   // dnp[t][y][x] = walking distance to team t's nest (NOPATH = cannot get there)
#define NOPATH 0xFFFF
static u8 pdirty = 1, mapt; static u16 nfood;                                // pdirty: land changed, redo the paths; mapt = map type
static u8 pfx[16], pfy[16], npatch;                                 // food patches (berry bushes regrow food around them)
static u8 shk, qfl[2], alt, alx, aly, jmp, hrow;                  // screen shake, queen hit flash, alert timer + place, SELECT jump step, decor refresh row
// maps: 4 styles at any size, then the hand-made maps (maps.h, always 32x32), then INFINITE (a new world from a 320-bit seed)
#define NSTYLE 4
#define MAPN (NSTYLE + NHAND + 2)
#define MINF (NSTYLE + NHAND)
#define MMY  (NSTYLE + NHAND + 1)                  // MY MAP: made in the in-game editor, saved on the cartridge
EWRAM static char cmap[MAXH * MAXH];              // MY MAP cells (w s g h f Y R), row by row
static u8 has_my;
static HandMap myhm = {"MY MAP", "MADE BY YOU", 32, 32, cmap};
static const char *const MNAME[NSTYLE] = {"MEADOW", "RIVER", "LAKES", "CANYON"};
static const char *const MDESC[NSTYLE] = {"GENTLE HILLS AND PONDS", "FIND THE FORDS", "WALK AROUND THE WATER", "CLIFFS AND RAMPS"};
static const char *mname(u8 i) { return i < NSTYLE ? MNAME[i] : i < MINF ? HMAPS[i - NSTYLE].name : i == MINF ? "INFINITE" : "MY MAP"; }
static const char *mdesc(u8 i) { return i < NSTYLE ? MDESC[i] : i < MINF ? HMAPS[i - NSTYLE].desc : i == MINF ? "A NEW WORLD EVERY GAME" : has_my ? "MADE BY YOU" : "NOT MADE YET: \1 TO MAKE IT"; }
static const HandMap *hmap(u8 i) { return i == MMY ? (has_my ? &myhm : 0) : (i >= NSTYLE && i < MINF) ? &HMAPS[i - NSTYLE] : 0; }
static u8 tw = 48, th = 48, newgame_gen_only;                                       // chosen world size (the hand-made maps are always 32x32)
static Ant ant[MAXA];
static u8 nestx[2], nesty[2], stock[2], qhp[2], ncnt[2], hatched[2];
static u8 cx, cy, mana, over, camx, camy, sandbox, cheat, spd = 4, sf, appr, diff, slice;
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
static const char PN[] = "FLOOD 4FASTEGGAP UP  CALM   QUEENUPSLOWRED";
static const char *const PD1[6] = {"FLOODS COST 4 AP", "NEW ANTS COST", "AP REFILLS", "POPULARITY DROPS", "YOUR QUEEN GETS", "RED ANTS COST"};
static const char *const PD2[6] = {"INSTEAD OF 8", "ONLY 2 FOOD", "TWICE AS FAST", "SLOWER: LESS DISASTERS", "ONE MORE HEART", "ONE MORE FOOD"};
static u8 tut, tev, tutor, lcx, lcy;             // tutorial: lesson (0 = off), events done, from-title flag, last cursor
static u8 wins[3]; static u16 best[3], hiscore[3];           // records (SRAM), best = seconds, 0 = none

// INFINITE maps: a 320-bit xorshift generator (10 x 32-bit words). Its state is stirred every title frame with the frame count,
// the scanline counter and the buttons, so every game starts from one of 2^320 (about 2 x 10^96) possible seeds.
static u32 mst[10] = {123456789u, 362436069u, 521288629u, 88675123u, 5783321u, 2463534242u, 3141592653u, 2718281828u, 1618033988u, 1414213562u};
static u8 mpi, genm;
static void pool_mix(u32 v) { u32 *p = &mst[mpi]; *p ^= v + 0x9E3779B9u + (*p << 6) + (*p >> 2); mpi = (u8)(mpi == 9 ? 0 : mpi + 1); }
static u32 mrnd(void) {
  u32 t = mst[mpi], v; u8 j = (u8)(mpi == 0 ? 9 : mpi - 1);
  t ^= t << 11; v = mst[j]; v = (v ^ (v >> 19)) ^ (t ^ (t >> 8)); mst[mpi] = v; mpi = (u8)(mpi == 9 ? 0 : mpi + 1);
  return v;
}
// ---------- OPTIONS (title: R, or the pause menu). Saved on the cartridge. ----------
enum { O_PRESET, O_ANTS, O_AP, O_REFILL, O_REDN, O_REDATK, O_MYATK, O_HEARTS, O_DIS, O_ELECT, O_PERKS, O_FOOD, O_REGROW, O_TIME,
       O_GSPD, O_CURSOR, O_HINTS, O_ALERTS, O_TRAILS, O_SHAKE, O_POPS, O_PANEL, O_ANTCOL, O_MUSIC, O_SFX, O_EYESPD, O_SKY, NOPT };
typedef struct { const char *name, *vals; u8 def; const char *h1, *h2; } Opt;
static const Opt OPTS[NOPT] = {
  {"PRESET",        "CUSTOM|KIDS|CLASSIC|HARD", 2, "SETS THE GAME RULES BELOW", "KIDS: GENTLE  HARD: TOUGH"},
  {"START ANTS",    "3|5|8|12",                 0, "ANTS EACH NEST HAS WHEN", "THE GAME BEGINS"},
  {"START AP",      "5|10|15|20",               1, "ANT POWER YOU START WITH", "EACH LAND CHANGE COSTS 1"},
  {"AP REFILL",     "SLOW|NORMAL|FAST",         1, "HOW FAST AP COMES BACK", "ALL BY ITSELF"},
  {"RED ANTS",      "FEWER|NORMAL|MORE",        1, "HOW EASILY THE RED NEST", "HATCHES NEW ANTS"},
  {"RED ATTACKS",   "CALM|NORMAL|ANGRY",        1, "RED SOLDIERS MARCH WHEN", "RED HAS 12  8  OR 5 ANTS"},
  {"MY ARMY GOES",  "EARLY|NORMAL|LATE",        1, "YOUR SOLDIERS MARCH WHEN", "YOU HAVE 5  8  OR 12 ANTS"},
  {"QUEEN HEARTS",  "3|4|5|6",                  2, "HOW MANY HITS EACH QUEEN", "CAN TAKE"},
  {"DISASTERS",     "OFF|RARE|NORMAL|OFTEN",    2, "FLOODS AND EARTHQUAKES", "THAT HIT BOTH SIDES"},
  {"ELECTIONS",     "ON|OFF",                   0, "A POPULARITY VOTE EVERY", "MINUTE  OFF: NO COUPS"},
  {"PERKS",         "ON|OFF",                   0, "PICK A BONUS AFTER A", "GOOD ELECTION"},
  {"FOOD",          "LITTLE|NORMAL|LOTS",       1, "HOW MUCH FOOD GROWS ON", "THE BERRY PATCHES"},
  {"FOOD REGROWS",  "SLOW|NORMAL|FAST",         1, "HOW FAST EATEN FOOD", "COMES BACK"},
  {"TIME LIMIT",    "OFF|5 MIN|10 MIN|20 MIN",  0, "WHEN TIME IS UP THE SIDE", "WITH MORE ANTS WINS"},
  {"START SPEED",   "SLOW|NORMAL|FAST",         1, "HOW FAST THE GAME RUNS", "\4 STILL SWITCHES FAST"},
  {"CURSOR SPEED",  "SLOW|NORMAL|FAST",         1, "HOW FAST THE CURSOR MOVES", "WHEN YOU HOLD THE D PAD"},
  {"BUTTON HINTS",  "ON|OFF",                   0, "THE BOTTOM LINE THAT SAYS", "WHAT \1 AND \2 WILL DO"},
  {"ALERTS",        "ON|OFF",                   0, "WARNINGS WHEN YOUR QUEEN", "IS ATTACKED"},
  {"ANT TRAILS",    "SHOW|HIDE",                0, "THE DOTS YOUR ANTS LEAVE", "ON THE GROUND"},
  {"SCREEN SHAKE",  "ON|OFF",                   0, "SHAKE ON FLOODS AND", "EARTHQUAKES"},
  {"POP UPS",       "ON|OFF",                   0, "LITTLE +1  SPLASH AND", "HEART PICTURES"},
  {"MENU STYLE",    "SOIL|CLAY|MOSS|NIGHT|AMBER|CRYSTAL", 0, "THE DIRT BEHIND MENUS", "AND UNDER THE MAP"},
  {"YOUR ANTS",     "BLACK|BLUE|GREEN|PURPLE|GOLD|WHITE|PINK|SHADOW", 0, "THE COLOR OF YOUR ANTS", "AND YOUR QUEEN"},
  {"MUSIC",         "ON|OFF",                   0, "THE SONGS", ""},
  {"SOUNDS",        "ON|OFF",                   0, "BEEPS  CRUNCHES AND", "SPLASHES"},
  {"ANT EYE SPEED", "SLOW|NORMAL|FAST",         1, "WALKING SPEED IN THE", "ANT EYE VIEW"},
  {"ANT EYE SKY",   "DAY|SUNSET|NIGHT",         0, "THE SKY IN THE ANT EYE", "VIEW"},
};
static u8 opt[NOPT];
static u8 cht[16];                                   // cheats switched on (EXTRAS > CHEATS)
static void ach_get(u8 i);
static u8 opt_locked(u8 i, u8 v);
static u8 qlost, rdrown, ewins;                   // classic achievements: queen hurt, red ants drowned, elections won
#define O(i) (opt[i])
static u8 qbase = 5, nosfx;                       // queen hearts (from the options), sounds off
void *memcpy(void *d, const void *s, unsigned n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
void *memset(void *d, int c, unsigned n) { u8 *a = d; while (n--) *a++ = (u8)c; return d; }
static u32 rnd(void) { if (genm) return mrnd() >> 8; rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs >> 8; }

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
// 3x5 font (big title letters, picture text), one octal digit per row. order: space, 0-9, A-Z, : ! / . - + ? < > then icons # @ % & ^ ~ $
static const u16 FONT[NGLYPH] = {
  0,
  075557,026227,071747,071717,055711,074717,074757,071122,075757,075717,
  025755,065656,034443,065556,074647,074644,034553,055755,072227,011152,
  055655,044447,057755,065555,025552,065644,025563,065655,034216,072222,
  055557,055552,055775,055255,055222,071247,
  002020,022202,011244,
  000002,000700,002720,071202,012421,042124,   // . - + ? < >
  057720,025752,013760,013641,075057,025312,055770,   // # @ % & ^ ~ $  (HUD icons, rough 3x5 versions)
  027760,025252,027555,075757,012425};                 // [ ] { } =  (dirt, amber, house, book, sword)
static void mk_glyph(volatile u32 *d, const u8 *g) {  // 8x8 text glyph (font8.h): ink colour 3, soft shadow colour 1, see-through
  u8 y, x, c; u32 row;
  for (y = 0; y < 8; y++) {
    row = 0;
    for (x = 0; x < 8; x++) {
      c = ((g[y] >> (7 - x)) & 1) ? 3 : (x && y && ((g[y - 1] >> (8 - x)) & 1)) ? 1 : 0;
      row |= (u32)c << (4 * x);
    }
    d[y] = row;
  }
}
static u8 gi(char c) {
  if (c >= '0' && c <= '9') return 1 + (c - '0');
  if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
  if (c == ':') return 37;
  if (c == '!') return 38;
  if (c == '/') return 39;
  { static const char EX[] = ".-+?<>#@%&^~$[]{}="; u8 i; for (i = 0; EX[i]; i++) if (c == EX[i]) return (u8)(40 + i); }
  return 0;
}
// BG1 palette banks: 0 terrain, 1 white text on black, 2 gold, 3 night sky, 4 dark gold
#define CHIP 300                                 // button pictures: \1 A, \2 B, \3 L, \4 R, \5 START, \6 SELECT (palette bank 4)
static void pc(u8 x, u8 y, char c, u8 p) {
  if ((u8)c >= 1 && (u8)c <= 6) { BGMAP1[y * 32 + x] = (u16)((CHIP + c - 1) | (4 << 12)); return; }
  BGMAP1[y * 32 + x] = (u16)((FT + gi(c)) | (p << 12));
}
static void ps(u8 x, u8 y, const char *s, u8 p) { while (*s) pc(x++, y, *s++, p); }
static void pn(u8 x, u8 y, u8 n, u8 p) { if (n > 99) n = 99; pc(x, y, n > 9 ? '0' + n / 10 : ' ', p); pc(x + 1, y, '0' + n % 10, p); }
static void pz(u8 x, u8 y, u8 n, u8 p) { if (n > 99) n = 99; pc(x, y, '0' + n / 10, p); pc(x + 1, y, '0' + n % 10, p); }
static void ov_fill(u16 t, u8 pal, u8 y0, u8 y1) { u8 x, y; for (y = y0; y <= y1; y++) for (x = 0; x < 32; x++) BGMAP1[y * 32 + x] = (u16)(t | (pal << 12)); }
// ---------- termite UI layer (BG3): soil chambers behind every menu, the dirt under the map for the HUD, the ant-farm title ----------
#define UIB 800                                    // UI tiles 800..831 (see UIT in art2.h)
static u16 uisoil(u8 x, u8 y) { return (u16)(UIB + ((x * 7 + y * 13 + (x * y)) & 3)); }
static void ui_full(void) {                        // a dug-out chamber: soil with a bark edge all round
  u8 x, y; u16 t;
  for (y = 0; y < 20; y++) for (x = 0; x < 32; x++) {
    t = uisoil(x, y);
    if (x < 30) {
      if (y == 0) t = x == 0 ? UIB + 4 : x == 29 ? UIB + 6 : UIB + 5;
      else if (y == 19) t = x == 0 ? UIB + 9 : x == 29 ? UIB + 11 : UIB + 10;
      else if (x == 0) t = UIB + 7; else if (x == 29) t = UIB + 8;
    }
    BGMAP3[y * 32 + x] = t;
  }
}
static void ui_game(void) {                        // the map above, the dirt (HUD) below with grass hanging into it
  u8 x, y;
  for (y = 0; y < 20; y++) for (x = 0; x < 32; x++)
    BGMAP3[y * 32 + x] = y < 17 ? 0 : y == 17 ? (u16)(UIB + 12 + ((x * 5) & 1)) : uisoil(x, y);
}
static u8 tun_at(u8 x, u8 y) {                     // the title's ant farm: which cells are tunnels
  if (y >= 18) return 1;                           // the long tunnel along the bottom (the ants walk here)
  if ((x == 1 || x == 28) && y >= 1) return 1;     // two shafts down the sides
  return 0;
}
static void ui_title(void) {
  u8 x, y, m;
  for (y = 0; y < 20; y++) for (x = 0; x < 32; x++) {
    if (x < 30 && tun_at(x, y)) {
      m = (u8)((y > 0 && tun_at(x, y - 1)) | ((x < 29 && tun_at(x + 1, y)) << 1) | ((y < 19 && tun_at(x, y + 1)) << 2) | ((x > 0 && tun_at(x - 1, y)) << 3));
      if (y == 19) m |= 4; if (x == 0 && y >= 18) m |= 8; if (x == 29 && y >= 18) m |= 2;
      BGMAP3[y * 32 + x] = (u16)(UIB + 14 + m);
    } else BGMAP3[y * 32 + x] = ((x * 11 + y * 5) % 17 == 0) ? UIB + 30 : uisoil(x, y);
  }
}
static void ov_clear(void) { ov_fill(0, 0, 0, 31); ui_game(); }   // text layer cleared, back to the game look

static void options_screen(void), pm_rule(u8 y), big(u8 y, const char *t);
static u8 emp_unlocked(void);
static const u8 CDLY[3] = {16, 10, 6}, CRAT[3] = {5, 3, 2};   // cursor repeat: delay and rate (slow / normal / fast)
static u8 timeup;
static u16 frc;
static void party_tick(void) {                    // PARTY ANTS cheat: your ants cycle through the rainbow
  static const u16 RB[8] = {RGB(31,4,4), RGB(31,18,2), RGB(31,31,4), RGB(6,28,6), RGB(4,24,31), RGB(10,8,31), RGB(24,6,31), RGB(31,8,20)};
  u8 f;
  if (!cht[6]) return;
  f = (u8)(frc >> (cht[6] == 1 ? 4 : cht[6] == 2 ? 3 : 1));   // SLOW / FAST / STROBE
  OBJPAL[2] = RB[f & 7]; OBJPAL[3] = RB[(f + 2) & 7]; OBJPAL[4] = RB[(f + 4) & 7];
}
static void say(const char *m) { msg = m; msgt = 12; }
static u8 blen2(const char *s) { u8 n = 0; while (s[n]) n++; return n; }

// ---------- game speed (pause menu: SPEED; R / SEL+B toggles NORMAL <-> VERY FAST; FRAME ADVANCE: R steps one game tick) ----------
// 0 PAUSED (world frozen, you can still edit), 1 FRAME ADVANCE*, 2 VERY SLOW, 3 SLOW, 4 NORMAL, 5 FAST, 6 VERY FAST, 7 LIGH ANT NING*   (* = unlocked by 10 wins)
static const char *const SPNAME[8] = {"PAUSED", "FRAME ADVANCE", "VERY SLOW", "SLOW", "NORMAL", "FAST", "VERY FAST", "LIGH ANT NING"};
static const char *const SPTAG[8] = {"II", ">I", "1/4", "1/2", "", "X2", "X4", "X8"};
static const u8 SPN[8] = {0, 0, 1, 1, 1, 2, 4, 8};          // ant slices per frame (8 slices = 1 game tick = 8 frames at NORMAL)
static const u8 SPM[8] = {0, 0, 3, 1, 0, 0, 0, 0};          // run only when (frame & mask) == 0: VERY SLOW every 4th frame, SLOW every 2nd
static u8 spd_ok(void) { return (u8)((u16)wins[0] + wins[1] + wins[2] >= 10 || cht[7]); }
static void spd_next(s8 dir) {                          // step through the speeds, skipping the two that are still locked
  u8 i;
  for (i = 0; i < 8; i++) { spd = (u8)((spd + dir + 8) & 7); if (spd_ok() || (spd != 1 && spd != 7)) break; }
}
static void spd_toggle(void) { spd = spd == 4 ? (cht[7] ? 7 : 6) : 4; say(spd >= 6 ? "FAST FORWARD ON" : "NORMAL SPEED"); }

// ---------- HUD (BG1 rows 17-19) ----------
// row 17: YOU hearts  ant-count   speed   RED hearts  ant-count
// row 18: AP + bar   food   popularity   time
// row 19: a message, or what is under the cursor and what A / B would do there
// text palette banks: 1 white, 2 gold, 8 red, 13 green, 14 cyan, 15 dim grey (all on black)
static u8 psx(u8 x, u8 y, const char *s, u8 p) { while (*s && x < 30) pc(x++, y, *s++, p); return x; }
static void cur_info(void) {
  static const char *const LN[4] = {"WATER", "SAND 1", "GRASS 2", "HILL 3"};
  u8 h = hgt[cy][cx], x;
  if (cx == nestx[0] && cy == nesty[0]) { ps(0, 19, "YOUR NEST  \5 MENU  \6 JUMP", 1); return; }
  if (cx == nestx[1] && cy == nesty[1]) { ps(0, 19, "RED NEST: YOUR SOLDIERS GO HERE", 8); return; }
  x = psx(0, 19, LN[h], 2);
  if (food[cy][cx] && h) psx((u8)(x + 1), 19, "+FOOD", 13);
  if (!mana && !cheat && !sandbox) { ps(19, 19, "NO AP: WAIT", 14); return; }
  if (O(O_HINTS)) return;
  if (h == 0) ps(20, 19, "\1 FILL IN", 1);
  else if (h == 3) ps(20, 19, "\2 DIG DOWN", 1);
  else ps(19, 19, "\1 UP \2 DOWN", 1);
}
static void hud(void) {
  u8 i; u32 s = (u32)gt * 2 / 15;
  ov_fill(1, 1, 17, 19);
  ps(0, 17, "YOU", 1); for (i = 0; i < qmax0; i++) pc((u8)(4 + i), 17, '#', i < qhp[0] ? 8 : 15);
  pc(11, 17, '@', 1); pn(12, 17, ncnt[0], 1);
  if (spd != 4) ps(14, 17, SPTAG[spd], 2);
  ps(17, 17, "RED", 8); for (i = 0; i < qbase; i++) pc((u8)(21 + i), 17, '#', i < qhp[1] ? 8 : 15);
  pc(27, 17, '@', 8); pn(28, 17, ncnt[1], 8);
  pc(0, 18, '&', 14); pn(1, 18, mana, 14);
  for (i = 0; i < 10; i++) BGMAP1[18 * 32 + 3 + i] = (u16)((mana > 2 * i ? BAR_F : BAR_E) | (14 << 12));
  pc(14, 18, '%', 13); pn(15, 18, stock[0], 13);
  pc(18, 18, '^', 2); pn(19, 18, appr, 2);
  pc(22, 18, '~', 1); pn(23, 18, (u8)(s / 60 > 99 ? 99 : s / 60), 1); pc(25, 18, ':', 1); pz(26, 18, (u8)(s % 60), 1);
  if (msgt) { psx(0, 19, msg, 2); msgt--; return; }
  cur_info();
}

// ---------- help card ----------
static const char HELP[] =
  "#BUTTONS\n"
  "D PAD     MOVE THE CURSOR\n"
  "\1         RAISE LAND  1 AP\n"
  "\2         LOWER LAND  1 AP\n"
  "\3         FLOOD 3X3   8 AP\n"
  "\4         FAST FORWARD\n"
  "\6         JUMP AROUND THE MAP\n"
  "\5         PAUSE MENU\n"
  "\6 + \5     ANT EYE VIEW\n"
  "\6 + \1     TRADE 2 FOOD: 4 AP\n"
  "\n"
  "#GOAL\n"
  "KILL THE RED QUEEN BEFORE\n"
  "THE RED ANTS KILL YOURS\n"
  "\n"
  "#TIPS\n"
  "DARK EDGES ARE CLIFFS: NO WAY\n";
static void help_show(void) {
  const char *s = HELP; u8 y = 1, x, g;
  hide_all(); vsync(); oam_flush();
  ov_fill(1, 1, 0, 19); ui_full();
  while (*s) {
    g = 0; if (*s == '#') { g = 1; s++; }
    x = 0; while (*s && *s != '\n') pc(x++, y, *s++, g ? 2 : 1);
    if (*s) s++;
    y++;
  }
  ps(0, 19, "\5 OR \2 BACK", 2);
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
static void ch1(u8 sweep, u16 f, u8 env) { if (nosfx) return; h1 = 14; NR10 = sweep; NR11 = 0x80; NR12 = env; NR13 = (u8)f; NR14 = (u8)(0x80 | (f >> 8)); }
static void ch2(u16 f, u8 env) { if (nosfx) return; h2 = 12; NR21 = 0x80; NR22 = env; NR23 = (u8)f; NR24 = (u8)(0x80 | (f >> 8)); }
static void noise(u8 env, u8 poly) { if (nosfx) return; hn = 16; NR41 = 0; NR42 = env; NR43 = poly; NR44 = 0x80; }
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
static void sfx_alarm(void) { ch1(0x00, N_C6, 0xC2); }
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
// MY MAP on the cartridge: offset 64 'E','M','A','P', w, h, w*h cell letters, 16-bit sum
#define MYO 64
static void my_save(void) {
  u16 i, n = (u16)(myhm.w * myhm.h), sum = 0;
  SRAM[MYO] = 'E'; SRAM[MYO + 1] = 'M'; SRAM[MYO + 2] = 'A'; SRAM[MYO + 3] = 'P'; SRAM[MYO + 4] = myhm.w; SRAM[MYO + 5] = myhm.h;
  for (i = 0; i < n; i++) { SRAM[MYO + 6 + i] = (u8)cmap[i]; sum = (u16)(sum + (u8)cmap[i]); }
  SRAM[MYO + 6 + n] = (u8)sum; SRAM[MYO + 7 + n] = (u8)(sum >> 8); has_my = 1;
}
static void my_load(void) {
  u16 i, n, sum = 0; u8 w = SRAM[MYO + 4], h = SRAM[MYO + 5];
  has_my = 0;
  if (SRAM[MYO] != 'E' || SRAM[MYO + 1] != 'M' || SRAM[MYO + 2] != 'A' || SRAM[MYO + 3] != 'P' || w < 32 || h < 32 || w > 96 || h > 96) return;
  n = (u16)(w * h);
  for (i = 0; i < n; i++) { cmap[i] = (char)SRAM[MYO + 6 + i]; sum = (u16)(sum + (u8)cmap[i]); }
  if ((u8)sum != SRAM[MYO + 6 + n] || (u8)(sum >> 8) != SRAM[MYO + 7 + n]) return;
  myhm.w = w; myhm.h = h; has_my = 1;
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
  my_load();
}

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
// ---------- world ----------
// Terrain is drawn on BG0 with auto-tiles (charblock 2, palette bank 11): every cell picks one of 81 tiles per level from how its
// four neighbours compare. Lower by 1 = a soft step (ants can walk it), lower by 2+ = a dark cliff (ants cannot). Water gets foam
// where it meets land. BG2 (screenblock 30, palette bank 12) is the decor layer on top: food, the nest mounds and ant trails.
#define TB  16                                    // terrain tiles 16..339 (level * 81 + code)
#define DEC 340                                   // decor: +0/+1 food, +2/+3 trail, +4..+12 nest mound (3x3)
EWRAM static u16 bq[MAXW * MAXH];                 // BFS queue (cell = y << 7 | x)
EWRAM static union { s16 f[MAXH][MAXW]; u8 b[240 * 160]; } gen_scratch;   // map generator scratch / battle picture
#define fld (gen_scratch.f)
static u8 dist(u8 a, u8 b) { return a > b ? a - b : b - a; }
static u8 is_nest(u8 x, u8 y) { return (x == nestx[0] && y == nesty[0]) || (x == nestx[1] && y == nesty[1]); }
static u8 near_nest(u8 x, u8 y) { return (dist(x, nestx[0]) <= 1 && dist(y, nesty[0]) <= 1) || (dist(x, nestx[1]) <= 1 && dist(y, nesty[1]) <= 1); }
static u8 emode;                                   // 1 while playing EMPIRE MODE
static void emp_dec(u8 x, u8 y);
static u8 wx0, wy0;                               // the 32x32-cell window of the world that BG0 / BG2 currently hold
static u8 inwin(u8 x, u8 y) { return (u8)(x - wx0) < 32 && (u8)(y - wy0) < 32; }
static void draw_dec(u8 x, u8 y) {
  if (emode) { emp_dec(x, y); return; }
  u16 t = 0; u8 i, p;
  for (i = 0; i < 2; i++)
    if (dist(x, nestx[i]) <= 1 && dist(y, nesty[i]) <= 1) t = (u16)(DEC + 4 + (y + 1 - nesty[i]) * 3 + (x + 1 - nestx[i]));
  if (!t) {
    p = ph[y][x];
    if (food[y][x] && hgt[y][x]) t = (u16)(DEC + ((x ^ y) & 1));
    else if (!O(O_TRAILS) && p > 150) t = DEC + 3;
    else if (!O(O_TRAILS) && p > 50) t = DEC + 2;
  }
  if (inwin(x, y)) BGMAP2[(y & 31) * 32 + (x & 31)] = t ? (u16)(t | (12 << 12)) : 0;
}
static u8 sstate(u8 h, s8 x, s8 y) {              // how one side of a cell at level h looks: 0 plain, 1 step / shore, 2 cliff
  u8 n;
  if (x < 0 || y < 0 || x >= W || y >= H) return 0;
  n = hgt[y][x];
  if (h == 0) return n > 0;
  if (n >= h) return 0;
  return (h - n >= 2) ? 2 : 1;
}
static void draw_cell(u8 x, u8 y) {
  u8 h = hgt[y][x]; u16 c;
  if (!inwin(x, y)) return;
  c = (u16)(sstate(h, x, y - 1) + 3 * sstate(h, x + 1, y) + 9 * sstate(h, x, y + 1) + 27 * sstate(h, x - 1, y));
  BGMAP0[(y & 31) * 32 + (x & 31)] = (u16)((TB + h * 81 + c) | (11 << 12));
  draw_dec(x, y);
}
static void draw_around(u8 x, u8 y) {             // a height changed: the cell and its 4 neighbours change their edges
  u8 k; s8 a, b;
  draw_cell(x, y);
  for (k = 0; k < 4; k++) { a = (s8)x + DX[k]; b = (s8)y + DY[k]; if (a >= 0 && b >= 0 && a < W && b < H) draw_cell((u8)a, (u8)b); }
  pdirty = 1;
}
static u8 can_go(u8 x, u8 y, s8 dx, s8 dy) {
  s8 a = (s8)x + dx, b = (s8)y + dy, d;
  if (a < 0 || b < 0 || a >= W || b >= H) return 0;
  if (hgt[b][a] == 0) return 0;
  d = (s8)hgt[b][a] - (s8)hgt[y][x];
  return d >= -1 && d <= 1;
}
#ifdef __arm__
#define IWRAMF __attribute__((section(".data.iwram"), long_call, noinline))
#else
#define IWRAMF
#endif
// The path search runs in the background, a few hundred cells per frame, into the spare buffer; when it is done the buffers swap,
// so ants always read a complete map. bfs_full() does it all at once (new game).
static u8 bj_t = 255, bj_again;                    // team being searched (255 = idle), "land changed again while searching"
static u16 bj_hd, bj_tl, (*bj_d)[MAXW];
static void bfs_start(u8 t) {
  u8 y, x; u16 i;
  bj_d = (dnp[0] != dbuf[0] && dnp[1] != dbuf[0]) ? dbuf[0] : (dnp[0] != dbuf[1] && dnp[1] != dbuf[1]) ? dbuf[1] : dbuf[2];
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) bj_d[y][x] = NOPATH;
  bj_d[nesty[t]][nestx[t]] = 0; i = (u16)((nesty[t] << 7) | nestx[t]); bq[0] = i; bj_hd = 0; bj_tl = 1; bj_t = t;
}
IWRAMF static void bfs_run(u16 budget) {           // expand up to budget cells (runs from IWRAM)
  u16 hd = bj_hd, tl = bj_tl, c, v, *d = &bj_d[0][0], ni; u8 x, y, h, nh, w = mw, hh = mh; const u8 *g = &hgt[0][0];
  while (hd < tl && budget--) {
    c = bq[hd++]; y = (u8)(c >> 7); x = (u8)(c & 127); v = (u16)(d[c] + 1); h = g[c];
#define BFS_TRY(cond, off) if (cond) { ni = (u16)(c + (off)); nh = g[ni]; if (nh && nh + 1 >= h && nh <= h + 1 && d[ni] == NOPATH) { d[ni] = v; bq[tl++] = ni; } }
    BFS_TRY(x + 1 < w, 1) BFS_TRY(x > 0, -1) BFS_TRY(y + 1 < hh, MAXW) BFS_TRY(y > 0, -MAXW)
  }
  bj_hd = hd; bj_tl = tl;
}
static void bfs_tick(u16 budget) {                 // call once per frame
  if (bj_t == 255) { if (!pdirty) return; pdirty = 0; bfs_start(0); }
  bfs_run(budget);
  if (bj_hd < bj_tl) return;
  dnp[bj_t] = bj_d;                                // finished: swap in the new map
  if (bj_t == 0) bfs_start(1); else { bj_t = 255; if (bj_again) { bj_again = 0; pdirty = 1; } }
}
static void bfs(u8 t) { bfs_start(t); bfs_run(0xFFFF); dnp[t] = bj_d; bj_t = 255; }   // all at once
// ----- little pop-up effects (sprites 51..58): +1 food, splash, puff, heart -----
#define FX_PLUS 9
#define FX_SPLASH 10
#define FX_PUFF 11
#define FX_HEART 12
typedef struct { s16 x, y; u8 t, tile; } Fx;
static Fx fx[8]; static u8 fxn;
static void fx_add(u8 x, u8 y, u8 tile) { if (O(O_POPS)) return; { Fx *f = &fx[fxn++ & 7]; f->x = (s16)(x * 8); f->y = (s16)(y * 8); f->t = 30; f->tile = tile; } }
static void alert(u8 x, u8 y, const char *m) {   // something bad happened to you: remember where, SELECT jumps there
  if (O(O_ALERTS)) return;
  if (!alt) { say(m); sfx_alarm(); }
  alt = 240; alx = x; aly = y;
}
static u8 spawn(u8 team) {
  u8 i, n = 0;
  for (i = 0; i < MAXA; i++) if (ant[i].alive && ant[i].team == team) n++;
  if (n >= MAXA / 2) return 0;
  for (i = 0; i < MAXA; i++) if (!ant[i].alive) {
    Ant *a = &ant[i];
    a->x = a->ox = nestx[team]; a->y = a->oy = nesty[team]; a->team = team; a->alive = 1; a->carry = 0; a->age = 8; a->fl = team;
    a->sol = ((hatched[team]++ & 3) == 0);
    return 1;
  }
  return 0;
}
static void count(u8 *c) { u8 i; c[0] = c[1] = 0; for (i = 0; i < MAXA; i++) if (ant[i].alive) c[ant[i].team]++; }

static void apr(s8 d) { s16 v = (s16)appr + d; appr = v < 0 ? 0 : v > 99 ? 99 : (u8)v; }
static void die(Ant *a, u8 drown) { a->alive = 0; if (a->team == 0) apr(-2); if (drown && a->team == 1 && ++rdrown >= 20) ach_get(3); fx_add(a->x, a->y, drown ? FX_SPLASH : FX_PUFF); }
static void set_rules(void) {
  fcost = 8; hc0 = 3; hc1 = RCOST[diff]; tm = TRK[diff]; dm = 31; qbase = (u8)(3 + O(O_HEARTS)); qmax0 = qbase;
  if (O(O_REFILL) == 0) tm = (u8)((tm << 1) | 1); else if (O(O_REFILL) == 2) tm >>= 1;
  if (O(O_REDN) == 0) hc1++; else if (O(O_REDN) == 2 && hc1 > 1) hc1--;
  if (perk & 1) fcost = 4;                       // FLOOD 4
  if (perk & 2) hc0 = 2;                         // FASTEGG
  if (perk & 4) tm >>= 1;                        // AP UP
  if (perk & 8) dm = (u8)((dm << 1) | 1);        // CALM: popularity sinks slower, disasters rarer
  if (perk & 16) qmax0 = (u8)(qbase + 1);        // QUEENUP
  if (qhp[0] > qmax0) qhp[0] = qmax0; if (qhp[1] > qbase) qhp[1] = qbase;
  if (perk & 32) hc1++;                          // SLOWRED
}
static void offer(void) {                        // two different perks you do not own yet; the play loop shows the perk card
  if (perk == 63) return;
  do pa = (u8)(rnd() & 7); while (pa > 5 || ((perk >> pa) & 1));
  pb = pa;
  do pb = pb > 4 ? 0 : pb + 1; while ((perk >> pb) & 1);
  pend = 1;
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
      if (!hgt[y][x]) food[y][x] = 0;
    }
  for (y = (s8)py - 2; y <= (s8)py + 2; y++) for (x = (s8)px - 2; x <= (s8)px + 2; x++)
    if (x >= 0 && y >= 0 && x < W && y < H) draw_cell(x, y);
  pdirty = 1; shk = 16;
}
static u8 dmask_dis(void) {
  u8 b = diff == 0 ? 127 : diff == 1 ? 63 : 31;
  if (O(O_DIS) == 1) b = (u8)((b << 1) | 1); else if (O(O_DIS) == 3) b >>= 1;
  return (perk & 8) ? (u8)((b << 1) | 1) : b;
}
static void disaster(void) {                     // hits both colonies alike, anywhere on the map
  if (cht[2] == 1) return;                        // WEATHER: CALM
  u8 q = (u8)(rnd() & 1), x = (u8)(rnd() % W), y = (u8)(rnd() % H);
  hit(x, y, q);
  sfx_flood(); alx = x; aly = y; alt = 120; (void)0; say(q ? "EARTHQUAKE!  \6 LOOK" : "FLASH FLOOD!  \6 LOOK");
}
static void election(void) {
  u8 ok = appr >= 25;                            // anything but a coup earns a perk offer
  if (appr >= 50) { mana = (mana + 8 > MANA_MAX) ? MANA_MAX : mana + 8; sfx_mana(); say("ANTLECTION WON! +8 AP"); if (++ewins >= 3 && !tut) ach_get(11); }
  else if (appr >= 25) { sfx_deny(); say("ANTLECTION: NO BONUS"); }
  else { stock[0] >>= 1; mana = 0; appr = 40; sfx_qdead(); say("COUP! FOOD AND AP LOST"); }
  if (ok && !tut && !sandbox && !O(O_PERKS)) offer();
}
static void step_ant(Ant *a) {
  u8 d, t = a->team, e = t ^ 1, found = 0, bd = 0, tx, ty, px = a->ox, py = a->oy; u16 dh, de;
  s16 sc, best_sc = -32000;
  static const u8 RTH[3] = {12, 8, 5}, MTH[3] = {5, 8, 12};
  u8 soldier = !a->carry && qhp[e] && ncnt[t] >= (t ? RTH[O(O_REDATK)] : MTH[O(O_MYATK)]) && a->sol && !(t && cht[8]);
  if (!a->carry && food[a->y][a->x]) { food[a->y][a->x] = 0; a->carry = 1; draw_dec(a->x, a->y); }
  if (a->carry) { u8 p = ph[a->y][a->x]; ph[a->y][a->x] = p > 215 ? 255 : p + 40; }
  for (d = 0; d < 4; d++) {
    if (!can_go(a->x, a->y, DX[d], DY[d])) continue;
    tx = a->x + DX[d]; ty = a->y + DY[d]; dh = dnp[t][ty][tx]; de = dnp[e][ty][tx];
    if (a->carry) sc = dh != NOPATH ? -12 * (s16)(dh > 2000 ? 2000 : dh) + (rnd() & 7) : -10 * (s16)(dist(tx, nestx[t]) + dist(ty, nesty[t])) + (rnd() & 15);
    else if (soldier) sc = de != NOPATH ? -14 * (s16)(de > 2000 ? 2000 : de) + (rnd() & 7) : -14 * (s16)(dist(tx, nestx[e]) + dist(ty, nesty[e])) + (rnd() & 31);
    else {                                       // worker: follow trails and food, drift away from home, do not turn straight back
      sc = ph[ty][tx] + (food[ty][tx] ? 120 : 0) + (rnd() & 31) + (dh != NOPATH ? (dh < 12 ? dh : 12) * 2 : 0);
      if (tx == px && ty == py) sc -= 24;
    }
    if (sc > best_sc) { best_sc = sc; bd = d; found = 1; }
  }
  a->ox = a->x; a->oy = a->y;
  if (found) { a->x += DX[bd]; a->y += DY[bd]; a->age = 0; if (DX[bd]) a->fl = DX[bd] < 0; }
  if (a->carry && a->x == nestx[t] && a->y == nesty[t]) {
    a->carry = 0; if (stock[t] < 99) stock[t]++;
    if (t == 0) { mana = (mana + MANA_PER_FOOD > MANA_MAX) ? MANA_MAX : mana + MANA_PER_FOOD; apr(1); sfx_food(); fx_add(a->x, a->y, FX_PLUS); }
  }
  if (!a->carry && qhp[e] && a->x == nestx[e] && a->y == nesty[e]) {
    if (a->sol) {
      if (rnd() & 1) {
        qhp[e]--; qfl[e] = 10; fx_add(a->x, a->y, FX_HEART); if (!e) qlost = 1;
        if (e == 0) { apr(-3); alert(a->x, a->y, "QUEEN ATTACKED!  \6 LOOK"); }
        if (qhp[e]) sfx_hit(); else sfx_qdead();
      }
    }
    else if (stock[e]) { stock[e]--; a->carry = 1; sfx_fight(); if (e == 0) alert(a->x, a->y, "FOOD STOLEN!  \6 LOOK"); }
  }
}
static void grow_food(void) {                    // food regrows, mostly around the berry patches
  u8 i; s8 x, y;
  for (nfood = 0, y = 0; y < H; y++) for (x = 0; x < W; x++) nfood += food[y][x];
  for (i = 0; i < 2 && nfood < (30 + (u16)W * H / 40) * (O(O_FOOD) + 1) / 2; i++) {
    if (npatch && (rnd() & 3)) { u8 k = (u8)(rnd() % npatch); x = (s8)(pfx[k] + (s8)(rnd() % 5) - 2); y = (s8)(pfy[k] + (s8)(rnd() % 5) - 2); }
    else { x = (s8)(rnd() % W); y = (s8)(rnd() % H); }
    if (x < 0 || y < 0 || x >= W || y >= H) continue;
    if (hgt[y][x] && !food[y][x] && !near_nest(x, y)) { food[y][x] = 1; nfood++; draw_dec(x, y); }
  }
}
static void food_rain(void) {                    // FOOD RAIN cheat: food drops anywhere, past the normal limit
  static const u8 FR[4] = {0, 2, 6, 16}; u8 i, x, y;
  for (i = 0; i < FR[cht[10]] && nfood < (u16)W * H / 4; i++) {
    x = (u8)(rnd() % W); y = (u8)(rnd() % H);
    if (hgt[y][x] && !food[y][x] && !near_nest(x, y)) { food[y][x] = 1; nfood++; draw_dec(x, y); }
  }
}
static void tick_slice(void) {                    // 1/8 of the ants per call; every 8th call runs the colony-level work
  u8 i, j, x, y, cst;
  for (i = 0; i < MAXA; i++) if (ant[i].age < 8) ant[i].age++;
  for (i = slice; i < MAXA; i += 8) {
    if (!ant[i].alive) continue;
    if (hgt[ant[i].y][ant[i].x] == 0) { die(&ant[i], 1); continue; }
    step_ant(&ant[i]);
  }
  for (i = slice; i < MAXA; i += 8)
    if (ant[i].alive) for (j = i + 1; j < MAXA; j++)
      if (ant[j].alive && ant[i].team != ant[j].team && ant[i].x == ant[j].x && ant[i].y == ant[j].y)
        { u8 iw = (u8)(rnd() & 1);
          if (cht[11] && ant[i].team != ant[j].team) {     // STRONG ANTS: your ant wins more often
            static const u8 SW[4] = {50, 67, 80, 100}; u8 mine_i = (u8)(ant[i].team == 0);
            u8 win = (u8)(rnd() % 100 < SW[cht[11]]); iw = (u8)(mine_i ? win : !win); }
          if (iw) die(&ant[j], 0); else die(&ant[i], 0); sfx_fight(); if (!ant[i].alive) break; }
  if ((gt & 3) == 0) for (y = slice; y < H; y += 8) for (x = 0; x < W; x++) if (ph[y][x]) ph[y][x]--;
  if (++slice < 8) return;
  slice = 0; gt++;
  count(ncnt);
  for (i = 0; i < 2; i++) {
    if (!qhp[i]) continue;
    cst = i ? hc1 : hc0;
    if (i == 1 && cht[15] && (cht[15] == 2 || (gt & 1))) { if (ncnt[1] == 0 && (gt & 31) == 0) spawn(1); }   // SLEEPY REDS
    else if (stock[i] >= cst && spawn(i)) { stock[i] -= cst; if (i == 0) { u8 b; for (b = 0; b < cht[12]; b++) spawn(0); sfx_spawn(); apr(1); } }   // BIG BROOD: free twins
    else if (ncnt[i] == 0 && (gt & 31) == 0) spawn(i);
    if ((gt & 127) == 0 && qhp[i] < (i ? qbase : qmax0)) qhp[i]++;
  }
  if (!O(O_ELECT) && (gt & dm) == 0) apr(!stock[0] && ncnt[0] ? -2 : -1);
  if (cht[2] == 2 && !tut && gt > 30 && (gt & 31) == 0) disaster();      // WEATHER: CHAOS
  if (cht[10]) food_rain();
  if (O(O_DIS) && !tut && !sandbox && gt > 90 && (gt & dmask_dis()) == 0 && !(rnd() & 3)) disaster();   // rarer on EASY / with CALM
  if (!O(O_ELECT) && ++etk >= 450) { etk = 0; election(); }
  if ((gt & (O(O_REGROW) == 0 ? 15 : O(O_REGROW) == 1 ? 7 : 3)) == 0) grow_food();
  if ((gt & tm) == 0 && mana < MANA_MAX) mana++;
}

// ---------- map generator: 4 kinds of land, always a walking path between the two nests ----------
static void blob(u8 n, u8 r0, u8 r1, s16 amp) {   // n round bumps (amp > 0) or dents (amp < 0) of radius r0..r1
  u8 k; s8 x, y, cx0, cy0, r; s16 d, rr;
  for (k = 0; k < n; k++) {
    cx0 = (s8)(rnd() % W); cy0 = (s8)(rnd() % H); r = (s8)(r0 + rnd() % (r1 - r0 + 1)); rr = (s16)(r * r);
    for (y = cy0 - r; y <= cy0 + r; y++) for (x = cx0 - r; x <= cx0 + r; x++) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      d = (s16)((x - cx0) * (x - cx0) + (y - cy0) * (y - cy0));
      if (d < rr) fld[y][x] += (s16)((s32)amp * (rr - d) / rr);
    }
  }
}
static void smooth(void) {                        // neighbouring land never differs by more than 1 (no cliffs)
  u8 pass, x, y, a, b;
  for (pass = 0; pass < 3; pass++) for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    a = hgt[y][x]; if (!a) continue;
    if (x + 1 < W) { b = hgt[y][x + 1]; if (b) { if (a > b + 1) hgt[y][x] = b + 1; else if (b > a + 1) hgt[y][x + 1] = a + 1; } }
    a = hgt[y][x];
    if (y + 1 < H) { b = hgt[y + 1][x]; if (b) { if (a > b + 1) hgt[y][x] = b + 1; else if (b > a + 1) hgt[y + 1][x] = a + 1; } }
  }
}
static void beach(void) {                         // land touching water becomes sand
  u8 x, y, k; s8 a, b;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) if (hgt[y][x] >= 2)
    for (k = 0; k < 4; k++) { a = (s8)x + DX[k]; b = (s8)y + DY[k]; if (a >= 0 && b >= 0 && a < W && b < H && !hgt[b][a]) { fld[y][x] = -999; break; } }
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) if (fld[y][x] == -999) hgt[y][x] = 1;
}
static void path(u8 x, u8 y, u8 tx, u8 ty) {      // walk from (x,y) to (tx,ty), shaping each next cell so an ant can follow
  u8 h, n, guard = 0;
  while ((x != tx || y != ty) && guard++ < 250) {
    h = hgt[y][x];
    if (x != tx && (y == ty || (rnd() & 1))) x = x < tx ? x + 1 : x - 1; else y = y < ty ? y + 1 : y - 1;
    n = hgt[y][x];
    if (n == 0) n = 1;
    if (n > h + 1) n = h + 1;
    if (h > 1 && n < h - 1) n = h - 1;
    hgt[y][x] = n;
  }
}
static void flat_nests(void) {
  u8 i; s8 x, y;
  for (i = 0; i < 2; i++) for (y = (s8)nesty[i] - 2; y <= (s8)nesty[i] + 2; y++) for (x = (s8)nestx[i] - 2; x <= (s8)nestx[i] + 2; x++)
    if (x >= 0 && y >= 0 && x < W && y < H) hgt[y][x] = (dist(x, nestx[i]) <= 1 && dist(y, nesty[i]) <= 1) ? 2 : (hgt[y][x] ? hgt[y][x] : 1);
}
static void river(u8 vert, u8 wid, u8 amp, u8 nford) {   // a wiggly river across the map, with sandy fords
  u8 i, k, n = vert ? H : W, ph0 = (u8)rnd(), fq = (u8)(6 + rnd() % 10), f[4]; s16 v; s8 c;
  for (k = 0; k < 4; k++) f[k] = k < nford ? (u8)(n / 6 + rnd() % (n * 2 / 3)) : 255;
  for (i = 0; i < n; i++) {
    v = (s16)((vert ? W : H) * i / n) + (SINT[(i * fq + ph0) & 255] * amp >> 8);
    if (vert) v = (s16)(W / 2 + (SINT[(i * fq + ph0) & 255] * amp >> 8));
    for (k = 0; k < wid; k++) {
      c = (s8)(v - wid / 2 + k);
      if (c < 0 || c >= (vert ? W : H)) continue;
      { u8 fd = (u8)(f[0] == i || f[0] + 1 == i || f[1] == i || f[1] + 1 == i || f[2] == i || f[3] == i);
        if (vert) hgt[i][c] = fd ? 1 : 0; else hgt[c][i] = fd ? 1 : 0; }
    }
  }
}
static void hand_map(const HandMap *m) {           // a hand-made map from maps.h (any size 32..96)
  u8 x, y; char c;
  npatch = 0; nestx[0] = 4; nesty[0] = (u8)(H - 6); nestx[1] = (u8)(W - 5); nesty[1] = 5;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    c = m->cells[y * W + x];
    hgt[y][x] = c == 'w' ? 0 : c == 's' ? 1 : c == 'h' ? 3 : 2;
    if (c == 'Y') { nestx[0] = x; nesty[0] = y; }
    if (c == 'R') { nestx[1] = x; nesty[1] = y; }
    if (c == 'f' && npatch < 16) { pfx[npatch] = x; pfy[npatch] = y; npatch++; }
  }
}
static void gen_world(void) {
  const HandMap *hm = hmap(mapt);
  u8 x, y, k, i, t = mapt == MMY ? 0 : mapt, hand = (u8)(hm != 0); s16 v; s8 a, b;
  u16 area; u8 sc;                                 // size scale: blob counts grow with the area
  mw = hand ? hm->w : tw; mh = hand ? hm->h : th;
  if (mw < 32) mw = 32; if (mw > 96) mw = 96; if (mh < 32) mh = 32; if (mh > 96) mh = 96; area = (u16)(W * H); sc = (u8)(area / 1024); if (!sc) sc = 1;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { fld[y][x] = 0; food[y][x] = 0; ph[y][x] = 0; }
  nestx[0] = (u8)(3 + (rnd() & 1)); nesty[0] = (u8)(H - 6 + (rnd() & 1)); nestx[1] = (u8)(W - 5 + (rnd() & 1)); nesty[1] = (u8)(4 + (rnd() & 1));
  if (hand) hand_map(hm);
  else if (t == MINF) {                            // INFINITE: every knob comes from the 320-bit generator
    u8 sym, cliffy, nb, nd, nl, nr, noise, smp, cor; s16 base, amp;
    genm = 1;
    base = (s16)(rnd() % 70) - 30; amp = (s16)(40 + rnd() % 80);
    nb = (u8)((4 + rnd() % 12) * sc); nd = (u8)((rnd() % 10) * sc); nl = (u8)(rnd() % 4); nr = (u8)(rnd() % 3);
    noise = (u8)(rnd() % 30); smp = (u8)(rnd() % 4); cliffy = (u8)((rnd() & 3) == 0); sym = (u8)(rnd() % 3); cor = (u8)(rnd() & 3);
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) fld[y][x] = (s16)(base + (noise ? (s16)(rnd() % (noise * 2 + 1)) - noise : 0));
    blob(nb, (u8)(2 + rnd() % 3), (u8)(5 + rnd() % 5), amp);
    blob(nd, 2, (u8)(3 + rnd() % 4), (s16)-(30 + rnd() % 60));
    for (k = 0; k < nl; k++) blob(1, (u8)(3 + rnd() % 6), (u8)(6 + rnd() % 6), -220);
    if (sym) for (y = 0; y < H; y++) for (x = 0; x < W; x++) {   // fair maps: mirror (sym 1) or point symmetry (sym 2)
      u8 mx2 = (u8)(W - 1 - x), my2 = (u8)(sym == 2 ? H - 1 - y : y);
      if (y * W + x < my2 * W + mx2) fld[my2][mx2] = fld[y][x];
    }
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
      v = fld[y][x];
      hgt[y][x] = cliffy ? (v < -60 ? 0 : v < 15 ? 1 : v < 55 ? 2 : 3) : (v <= -25 ? 0 : v < 0 ? 1 : v < 50 ? 2 : 3);
    }
    for (k = 0; k < nr; k++) river((u8)(rnd() & 1), (u8)(2 + rnd() % 3), (u8)(rnd() % 6), (u8)(1 + rnd() % 3));
    if (cor == 1) { nestx[0] = 4; nesty[0] = 4; nestx[1] = (u8)(W - 5); nesty[1] = (u8)(H - 5); }
    else if (cor == 2) { nestx[0] = 4; nesty[0] = (u8)(H / 2); nestx[1] = (u8)(W - 5); nesty[1] = (u8)(H / 2); }
    else if (cor == 3) { nestx[0] = (u8)(W / 2); nesty[0] = (u8)(H - 5); nestx[1] = (u8)(W / 2); nesty[1] = 4; }
    flat_nests();
    if (!cliffy) { beach(); for (k = 0; k < smp; k++) smooth(); }
    flat_nests();
    for (k = 0; k < (cliffy ? 2 : 0); k++) { x = (u8)(W / 4 + rnd() % (W / 2)); y = (u8)(H / 4 + rnd() % (H / 2)); path(nestx[0], nesty[0], x, y); path(x, y, nestx[1], nesty[1]); }
  } else {
    if (t == 0) { blob((u8)(14 * sc), 3, 6, 70); blob((u8)(9 * sc), 2, 3, -70); }                 // MEADOW
    else if (t == 1) { blob((u8)(12 * sc), 3, 6, 70); blob((u8)(3 * sc), 2, 3, -60); }            // RIVER (the river comes below)
    else if (t == 2) { blob((u8)(10 * sc), 3, 6, 70); blob((u8)(1 * sc), 6, 7, -200); blob((u8)(3 * sc), 3, 4, -160); }   // LAKES
    else { blob((u8)(16 * sc), 3, 6, 90); blob((u8)(10 * sc), 2, 5, -70); }                      // CANYON
    if (t == 2) {                                   // LAKES: one big lake in the middle
      s16 r2 = (s16)((W < H ? W : H) * (W < H ? W : H) / 45);
      for (y = 0; y < H; y++) for (x = 0; x < W; x++)
        if ((x - W / 2) * (x - W / 2) + (y - H / 2) * (y - H / 2) < r2 + (s16)(rnd() & 7)) fld[y][x] -= 200;
    }
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
      v = fld[y][x];
      if (t == 3) hgt[y][x] = v < -60 ? 0 : v < 15 ? 1 : v < 55 ? 2 : 3;
      else hgt[y][x] = v <= -25 ? 0 : v < 0 ? 1 : v < 50 ? 2 : 3;
    }
    if (t == 1) river(0, 4, 3, 2);                  // RIVER: corner to corner, between the nests
    flat_nests();
    if (t != 3) { beach(); smooth(); }
    flat_nests();
    if (t == 3) for (k = 0; k < 2 + sc / 2; k++) {  // CANYON: winding ramps through the cliffs, via random midpoints
      x = (u8)(W / 4 + rnd() % (W / 2)); y = (u8)(H / 4 + rnd() % (H / 2)); path(nestx[0], nesty[0], x, y); path(x, y, nestx[1], nesty[1]);
    }
  }
  if (hgt[nesty[0]][nestx[0]] == 0) hgt[nesty[0]][nestx[0]] = 2;
  if (hgt[nesty[1]][nestx[1]] == 0) hgt[nesty[1]][nestx[1]] = 2;
  bfs(0);
  if (dnp[0][nesty[1]][nestx[1]] == NOPATH) path(nestx[0], nesty[0], nestx[1], nesty[1]);   // always a way from nest to nest
  if (!hand) {                                      // berry patches: one near each nest, the rest anywhere on land
    u8 want = (u8)(6 + 2 * sc); if (want > 16) want = 16;
    npatch = 0;
    for (i = 0; i < 2; i++) {
      for (k = 0; k < 20; k++) {
        a = (s8)(nestx[i] + (nestx[i] < W / 2 ? 1 : -1) * (s8)(3 + rnd() % 3)); b = (s8)(nesty[i] + (nesty[i] < H / 2 ? 1 : -1) * (s8)(2 + rnd() % 3));
        if (a >= 0 && b >= 0 && a < W && b < H && hgt[b][a]) break;
      }
      pfx[npatch] = (u8)a; pfy[npatch] = (u8)b; npatch++;
    }
    for (k = 0; k < 120 && npatch < want; k++) {
      x = (u8)(rnd() % W); y = (u8)(rnd() % H);
      if (hgt[y][x] && dist(x, nestx[0]) + dist(y, nesty[0]) > 6 && dist(x, nestx[1]) + dist(y, nesty[1]) > 6) { pfx[npatch] = x; pfy[npatch] = y; npatch++; }
    }
  }
  for (i = 0; i < npatch; i++) for (k = 0; k < (O(O_FOOD) == 0 ? 5 : O(O_FOOD) == 1 ? 9 : 14); k++) {
    a = (s8)(pfx[i] + (s8)(rnd() % 5) - 2); b = (s8)(pfy[i] + (s8)(rnd() % 5) - 2);
    if (a >= 0 && b >= 0 && a < W && b < H && hgt[b][a] && !near_nest(a, b)) food[b][a] = 1;
  }
  genm = 0;
  pdirty = 1;
}

// ---------- camera + sprites ----------
static s8 shx;                                     // screen shake offset this frame
static void follow(void) {
  u8 t;
  if (cx < camx + 3) camx = cx > 3 ? cx - 3 : 0;
  else if (cx + 3 >= camx + VW) { t = cx + 4 - VW; camx = t > W - VW ? W - VW : t; }
  if (camx > W - VW) camx = W - VW;
  if (cy < camy + 3) camy = cy > 3 ? cy - 3 : 0;
  else if (cy + 3 >= camy + VH) { t = cy + 4 - VH; camy = t > H - VH ? H - VH : t; }
  if (camy > H - VH) camy = H - VH;
}
static void scroll_step(void) {                    // glide to the camera target, faster when it is far away (SELECT jumps)
  s16 tx = (s16)camx * 8, ty = (s16)camy * 8, d;
  d = tx - scx; d = d > 0 ? 4 + d / 6 : d < 0 ? -4 + d / 6 : 0; scx += d; if ((d > 0 && scx > tx) || (d < 0 && scx < tx)) scx = tx;
  d = ty - scy; d = d > 0 ? 4 + d / 6 : d < 0 ? -4 + d / 6 : 0; scy += d; if ((d > 0 && scy > ty) || (d < 0 && scy < ty)) scy = ty;
}
static void win_col(u8 x) { u8 y; for (y = wy0; y < (u8)(wy0 + 32) && y < H; y++) draw_cell(x, y); }
static void win_row(u8 y) { u8 x; for (x = wx0; x < (u8)(wx0 + 32) && x < W; x++) draw_cell(x, y); }
static void win_all(void) { u8 y; wx0 = (u8)(scx >> 3); wy0 = (u8)(scy >> 3); for (y = wy0; y < (u8)(wy0 + 32) && y < H; y++) win_row(y); }
static void win_sync(void) {                       // keep the drawn window under the camera: draw the new column / row as it scrolls in
  u8 tx = (u8)(scx >> 3), ty = (u8)(scy >> 3);
  while (wx0 < tx) { wx0++; win_col((u8)(wx0 + 31)); }
  while (wx0 > tx) { wx0--; win_col(wx0); }
  while (wy0 < ty) { wy0++; win_row((u8)(wy0 + 31)); }
  while (wy0 > ty) { wy0--; win_row(wy0); }
}
static void place_px(u8 s, u8 tile, u8 pal, int px, int py, u8 flip) {
  int sx = px - scx - shx, sy = py - scy;
  if (sx > -8 && sx < 240 && sy > -8 && sy < VH * 8) spr(s, sx, sy, tile, pal, 1, flip); else spr_hide(s);
}
static void place16(u8 s, u8 tile, u8 pal, int px, int py, u8 flip) {   // 16x16 sprite centred on a cell
  int sx = px - 4 - scx - shx, sy = py - 4 - scy;
  if (sx > -16 && sx < 240 && sy > -16 && sy < VH * 8) { spr(s, sx, sy, tile, pal, 1, flip); oam[s * 4 + 1] |= 0x4000; } else spr_hide(s);
}

static void draw_sprites(void) {
  u8 i, tl; Ant *a; Fx *f;
  for (i = 0; i < MAXA; i++) {
    a = &ant[i];
    if (!a->alive) { spr_hide(i); continue; }
    tl = a->carry ? 5 : a->sol ? 7 : 3;
    if (a->age < 8 && ((frc >> 2) + i) & 1) tl++;
    place_px(i, tl, a->team, a->ox * 8 + (a->x - a->ox) * a->age, a->oy * 8 + (a->y - a->oy) * a->age, a->fl);
  }
  for (i = 0; i < 2; i++) {
    if (qhp[i]) place16(MAXA + i, 16, qfl[i] ? 4 : i, nestx[i] * 8, nesty[i] * 8 - ((frc >> 4) & 1), i); else spr_hide(MAXA + i);
    if (qfl[i]) qfl[i]--;
  }
  place16(MAXA + 2, 20, (is_nest(cx, cy) || (!mana && !cheat && !sandbox)) ? 5 : 2, cx * 8, cy * 8, 0);
  for (i = 0; i < 8; i++) {
    f = &fx[i];
    if (!f->t) { spr_hide(MAXA + 3 + i); continue; }
    f->t--; if (f->tile != FX_PUFF && (f->t & 1)) f->y--;
    place_px(MAXA + 3 + i, f->tile, 3, f->x, f->y, 0);
  }
}

// ---------- new game ----------
static void newgame(void) {
  u8 i, k;
  DISPCNT = 0x80; BLDY = 0;
  ov_fill(1, 1, 0, 19); ui_full();
  ps(5, 8, "BUILDING THE WORLD...", 2);
  ps((u8)(15 - (blen2(mname(mapt)) + 1) / 2), 10, mname(mapt), 1);
  vsync(); DISPCNT = 0x0A00;                        // only the text + soil layers while the map is made
  gen_world();
  DISPCNT = 0x80; BLDY = 16;
  for (i = 0; i < MAXA; i++) ant[i].alive = 0;
  for (i = 0; i < 8; i++) fx[i].t = 0;
  set_rules(); stock[0] = stock[1] = 0; qhp[0] = qhp[1] = qbase; hatched[0] = hatched[1] = 0; qfl[0] = qfl[1] = 0;
  for (k = 0; k < (O(O_ANTS) == 0 ? 3 : O(O_ANTS) == 1 ? 5 : O(O_ANTS) == 2 ? 8 : 12); k++) { spawn(0); spawn(1); }
  count(ncnt);
  perk = 0; pend = 0; set_rules(); spd = (u8)(3 + O(O_GSPD)); sf = 0; appr = 50; etk = 0; slice = 0; cheat = 0; cx = nestx[0] + 2; cy = nesty[0] - 2; mana = (u8)(5 + 5 * O(O_AP)); gt = 0; over = 0; msgt = 0; timeup = 0;
  shk = 0; shx = 0; alt = 0; jmp = 0; qlost = 0; rdrown = 0; ewins = 0;
  camx = 0; camy = H - VH; follow(); scx = (s16)camx * 8; scy = (s16)camy * 8;
  win_all();
  bfs(0); bfs(1); pdirty = 0; bj_again = 0;
  ov_clear(); hud();
  say(sandbox ? "SANDBOX: NO LIMITS" : "\5 MENU \6 JUMP \3 FLOOD \4 FAST");
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy;
  BG2CNT = 1 | (2 << 2) | (30 << 8);
  hide_all(); draw_sprites(); oam_flush();
  DISPCNT = 0x1F40;
  music_start();
  fade(16, 0);
}

static void raise_land(void) {
  if (mana && hgt[cy][cx] < 3 && !is_nest(cx, cy)) { hgt[cy][cx]++; mana--; draw_around(cx, cy); sfx_raise(); tev |= 2; return; }
  sfx_deny(); say(is_nest(cx, cy) ? "NESTS CANT BE CHANGED" : hgt[cy][cx] >= 3 ? "ALREADY HIGHEST: \2 LOWERS" : "NO AP: WAIT OR FIND FOOD");
}
static void lower_land(void) {
  if (mana && hgt[cy][cx] > 0 && !is_nest(cx, cy)) {
    hgt[cy][cx]--; mana--; if (!hgt[cy][cx]) food[cy][cx] = 0;
    draw_around(cx, cy); sfx_lower(); tev |= 4; return;
  }
  sfx_deny(); say(is_nest(cx, cy) ? "NESTS CANT BE CHANGED" : hgt[cy][cx] == 0 ? "ALREADY WATER: \1 FILLS" : "NO AP: WAIT OR FIND FOOD");
}
static void offering(void) {
  if (stock[0] < OFFER_FOOD) { sfx_deny(); say("NEED 2 FOOD IN THE NEST"); return; }
  if (mana >= MANA_MAX) { sfx_deny(); say("AP IS ALREADY FULL"); return; }
  stock[0] -= OFFER_FOOD; mana = (mana + OFFER_MANA > MANA_MAX) ? MANA_MAX : mana + OFFER_MANA;
  apr(-5); sfx_mana(); say("TRADED 2 FOOD: +4 AP");
}
static void flood(void) {
  s8 x, y;
  if (mana < fcost) { sfx_deny(); say(fcost == 4 ? "FLOOD NEEDS 4 AP" : "FLOOD NEEDS 8 AP"); return; }
  mana -= fcost; sfx_flood(); tev |= 8; shk = 10;
  for (y = (s8)cy - 1; y <= (s8)cy + 1; y++) for (x = (s8)cx - 1; x <= (s8)cx + 1; x++)
    if (x >= 0 && y >= 0 && x < W && y < H && hgt[y][x] && !is_nest(x, y)) { hgt[y][x]--; if (!hgt[y][x]) food[y][x] = 0; }
  for (y = (s8)cy - 2; y <= (s8)cy + 2; y++) for (x = (s8)cx - 2; x <= (s8)cx + 2; x++)
    if (x >= 0 && y >= 0 && x < W && y < H) draw_cell(x, y);
  pdirty = 1;
}
static void jump(void) {                           // SELECT: alert spot, else HOME -> ARMY -> RED NEST
  u8 i, bi = MAXA; u16 b = NOPATH, d;
  if (alt) { cx = alx; cy = aly; alt = 0; say("HERE!"); sfx_spawn(); return; }
  jmp = (u8)((jmp + 1) % 3);
  if (jmp == 1) {
    for (i = 0; i < MAXA; i++) if (ant[i].alive && !ant[i].team) { d = dnp[1][ant[i].y][ant[i].x]; if (d < b) { b = d; bi = i; } }
    if (bi == MAXA) jmp = 2; else { cx = ant[bi].x; cy = ant[bi].y; say("YOUR ARMY: CLOSEST TO RED"); }
  }
  if (jmp == 2) { cx = nestx[1]; cy = nesty[1]; say("THE RED NEST"); }
  if (jmp == 0) { cx = nestx[0]; cy = nesty[0]; say("YOUR NEST"); }
  sfx_spawn();
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
static u8 tsel;                                    // NEW GAME rows: 0 LEVEL, 1 MAP, 2 WIDTH, 3 HEIGHT, 4 SANDBOX, 5 START
#define BG1HOFS IO16(0x14)
#define BG3HOFS IO16(0x1C)
#define BIGT 24
#define ORN  764                                       // 764 rule, 765/766 centre diamond, 767/768 pip on/off, 769 arrow
#define ICNT_T 96                                  // OBJ tiles of the home screen icons: book 96, map 112, trophy 128, gear 144
// ---------- MAP PICTURE: the whole world drawn into 12 x 12 BG tiles (96 x 96 pixels), palette bank 6 ----------
// Used by the NEW GAME preview and the pause menu's MAP. Scale: 1, 2 or 3 pixels per cell so the map fills the box.
#define MAPT 832                                   // BG tiles 832..975
static u8 mv_m, mv_ox, mv_oy;                      // the longer side in cells (it fills 96 pixels), image offset in the box
static void mv_pal(void) {
  static const u16 MP[16] = {0, RGB(3,2,1), RGB(4,9,20), RGB(26,23,13), RGB(8,19,6), RGB(17,12,7), RGB(31,31,31), RGB(31,5,4),
    RGB(2,2,4), RGB(31,10,8), RGB(31,27,6), RGB(26,20,4), RGB(10,8,6), RGB(6,14,28), RGB(12,24,9), RGB(31,31,12)};
  u8 i; for (i = 0; i < 16; i++) BGPAL[96 + i] = MP[i];
}
// colours: 1 frame, 2 water, 3 sand, 4 grass, 5 hill, 6 your nest, 7 red nest, 8 your ant, 9 red ant, 10 food, 11 gold frame
static u8 mv_cell(u8 cx2, u8 cy2) {                // the colour of one world cell
  u8 h;
  if (dist(cx2, nestx[0]) <= 1 && dist(cy2, nesty[0]) <= 1) return 6;
  if (dist(cx2, nestx[1]) <= 1 && dist(cy2, nesty[1]) <= 1) return 7;
  h = hgt[cy2][cx2];
  if (food[cy2][cx2] && h) return 10;
  return (u8)(h == 0 ? 2 : h == 1 ? 3 : h == 2 ? 4 : 5);
}
static void map_img(u8 col0, u8 row0, u8 objs) {  // draw it, with the tiles placed on BG1 at (col0, row0)
  u8 tx, ty, y, x, i; u32 w; u16 m = (u16)(W > H ? W : H);
  mv_m = (u8)m;
  mv_ox = (u8)((96 - W * 96 / m) / 2); mv_oy = (u8)((96 - H * 96 / m) / 2);
  mv_pal();
  for (ty = 0; ty < 12; ty++) {                    // one pixel row at a time: work out its 96 colours once, then pack 12 tiles
    for (y = 0; y < 8; y++) {
      static u8 row[96]; u8 py = (u8)(ty * 8 + y); s16 wy = (s16)py - mv_oy;
      u8 cyy = (u8)(wy * mv_m / 96), in_y = (u8)(wy >= 0 && cyy < H && wy < 96 - 2 * mv_oy), x2;
      for (x2 = 0; x2 < 96; x2++) {
        s16 wx2 = (s16)x2 - mv_ox;
        u8 cxx = (u8)(wx2 * mv_m / 96);
        row[x2] = (u8)(in_y && wx2 >= 0 && wx2 < 96 - 2 * mv_ox && cxx < W ? mv_cell(cxx, cyy) : 0);
      }
      for (tx = 0; tx < 12; tx++) {
        w = 0; for (x = 0; x < 8; x++) w |= (u32)row[tx * 8 + x] << (4 * x);
        TILE32[(MAPT + ty * 12 + tx) * 8 + y] = w;
      }
    }
    for (tx = 0; tx < 12; tx++) BGMAP1[(row0 + ty) * 32 + col0 + tx] = (u16)((MAPT + ty * 12 + tx) | (6 << 12));
  }
  if (objs) for (i = 0; i < MAXA; i++) if (ant[i].alive) {        // ants: one dot each (written straight into the tiles)
    u8 px = (u8)(mv_ox + (ant[i].x * 2 + 1) * 48 / mv_m), py = (u8)(mv_oy + (ant[i].y * 2 + 1) * 48 / mv_m);
    u16 t = (u16)(MAPT + (py >> 3) * 12 + (px >> 3)); u8 sh = (u8)(4 * (px & 7));
    TILE32[t * 8 + (py & 7)] = (TILE32[t * 8 + (py & 7)] & ~(15u << sh)) | ((u32)(ant[i].team ? 9 : 8) << sh);
  }
}
// the random number state, saved so the NEW GAME preview is exactly the map you then play
static u32 snap_rs, snap_mst[10]; static u8 snap_mpi;
static void snap_take(void) { u8 i; snap_rs = rs; for (i = 0; i < 10; i++) snap_mst[i] = mst[i]; snap_mpi = mpi; }
static void snap_put(void) { u8 i; rs = snap_rs; for (i = 0; i < 10; i++) mst[i] = snap_mst[i]; mpi = snap_mpi; }
static void map_view(void) {                       // pause menu > MAP: the whole world; move the box, A jumps there
  u16 k, p, prev = joy(), fr = 0; u8 mx = cx, my = cy, rep = 0;
  hide_all(); oam_flush();
  ov_fill(0, 0, 0, 19); ui_full();
  ps(1, 0, "THE WHOLE MAP", 9);
  map_img(9, 2, 1);
  ps(1, 15, "\1 GO THERE  \2 BACK", 10);
  ps(1, 17, "BLACK: YOUR ANTS  RED: FOES", 3); ps(1, 18, "YELLOW: FOOD  WHITE: HOME", 3);
  for (;;) {
    vsync(); fr++; k = joy(); p = k & ~prev; prev = k;
    if (k & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) { if (++rep > 12 && (fr & 1)) p |= k; } else rep = 0;
    if (p & J_LEFT && mx) mx--; if (p & J_RIGHT && mx < W - 1) mx++;
    if (p & J_UP && my) my--; if (p & J_DOWN && my < H - 1) my++;
    if (p & J_B) break;
    if (p & J_A) { cx = mx; cy = my; follow(); sfx_spawn(); break; }
    {
      int sx = 9 * 8 + mv_ox + (mx * 2 + 1) * 48 / mv_m - 8, sy = 2 * 8 + mv_oy + (my * 2 + 1) * 48 / mv_m - 8;
      if (fr & 8) { spr(0, sx, sy, 20, 2, 0, 0); oam[1] |= 0x4000; } else spr_hide(0);   // the cursor box
      oam_flush();
    }
  }
  hide_all(); oam_flush();
  while (joy()) vsync();
}
// ---------- NEW GAME: pick the level, the map, its size and sandbox, then go ----------
static void ng_preview(void) {                     // build the map now (from the saved random state) and show it
  { u8 x, y; for (y = 4; y < 16; y++) for (x = 17; x < 29; x++) BGMAP1[y * 32 + x] = 0; }
  if (mapt == MMY && !has_my) { ps(18, 9, "NOT MADE", 15); ps(18, 10, "YET", 15); return; }
  ps(19, 9, "DIGGING", 10);
  snap_put(); gen_world(); snap_put();
  map_img(17, 4, 0);
}
static void ng_text(void) {                        // the settings on the left (cols 1-16), the map picture on the right
  const HandMap *hm = hmap(mapt); u8 hand = (u8)(hm != 0), s, x, y;
  static const u8 NGR[6] = {4, 7, 10, 11, 13, 16};
  for (y = 3; y < 18; y++) for (x = 1; x < 17; x++) BGMAP1[y * 32 + x] = (u16)(1 | (3 << 12));
  ov_fill(1, 3, 16, 17);
  for (s = 0; s < 6; s++) if (tsel == s) BGMAP1[NGR[s] * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
  ps(3, 4, "LEVEL", tsel == 0 ? 9 : 3); if (wins[diff]) { pn(10, 4, wins[diff], 13); ps(13, 4, "WON", 10); }
  ps(3, 5, "<", 10); ps((u8)(9 - blen2(DNAME[diff]) / 2), 5, DNAME[diff], 9); ps(14, 5, ">", 10);
  ps(3, 7, "MAP", tsel == 1 ? 9 : 3);
  ps(2, 8, "<", 10); ps((u8)(9 - blen2(mname(mapt)) / 2), 8, mname(mapt), 9); ps(15, 8, ">", 10);
  ps(3, 10, "WIDE", tsel == 2 ? 9 : 3); ps(3, 11, "TALL", tsel == 3 ? 9 : 3);
  if (hand) { pn(10, 10, hm->w, 15); pn(10, 11, hm->h, 15); ps(13, 10, "SET", 15); }
  else { pc(8, 10, '<', 10); pn(10, 10, tw, 9); pc(13, 10, '>', 10); pc(8, 11, '<', 10); pn(10, 11, th, 9); pc(13, 11, '>', 10); }
  ps(3, 13, "SANDBOX", tsel == 4 ? 9 : 3); ps(11, 13, sandbox ? "ON" : "OFF", sandbox ? 13 : 10);
  ps(3, 14, sandbox ? "NO LIMITS" : "BEAT THE RED", 15);
  ps((u8)(15 - (blen2(mdesc(mapt)) + 1) / 2), 17, mdesc(mapt), 10);
  ps(3, 16, mapt == MMY && !has_my ? "MAKE IT FIRST" : "START GAME!", tsel == 5 ? 9 : 13);
}
static void ng_draw(void) {
  ov_fill(1, 3, 0, 19); ui_full();
  big(0, "NEW GAME"); pm_rule(2); pm_rule(18);
  ps(1, 19, "\1 GO  \2 BACK  \6 NEW MAP", 10);
  ng_text(); ng_preview();
}
static u8 newgame_menu(void) {                     // 0 = back, 1 = play, 2 = open the editor (to make MY MAP first)
  u16 k, p, prev = joy(), fr = 0; u8 dirty = 0;
  hide_all(); oam_flush();
  if (tsel > 5) tsel = 5;
  snap_take(); ng_draw();
  for (;;) {
    vsync(); fr++; k = joy(); p = k & ~prev; prev = k;
    if (!p) { if (dirty && ++dirty > 30) { dirty = 0; ng_preview(); } continue; }   // re-dig the picture once you stop pressing
    if (p & J_B) { sfx_food(); return 0; }
    if (p & J_SEL) { pool_mix(fr * 2654435761u ^ VCOUNT); rs ^= (u32)fr * 40503u + VCOUNT; if (!rs) rs = 1; snap_take(); sfx_mana(); ng_preview(); continue; }
    if (p & J_DOWN) { tsel = (u8)(tsel == 5 ? 0 : tsel + 1); if (hmap(mapt) && (tsel == 2 || tsel == 3)) tsel = 4; sfx_food(); ng_text(); }
    if (p & J_UP)   { tsel = (u8)(tsel == 0 ? 5 : tsel - 1); if (hmap(mapt) && (tsel == 2 || tsel == 3)) tsel = 1; sfx_food(); ng_text(); }
    if (p & (J_LEFT | J_RIGHT)) {
      s8 dd = (p & J_RIGHT) ? 1 : -1;
      if (tsel == 0) diff = (u8)((diff + 3 + dd) % 3);
      else if (tsel == 1) { mapt = (u8)((mapt + MAPN + dd) % MAPN); dirty = 1; }
      else if (tsel == 2 && !hmap(mapt)) { tw = (u8)(tw + dd * 8 < 32 ? 96 : tw + dd * 8 > 96 ? 32 : tw + dd * 8); dirty = 1; }
      else if (tsel == 3 && !hmap(mapt)) { th = (u8)(th + dd * 8 < 32 ? 96 : th + dd * 8 > 96 ? 32 : th + dd * 8); dirty = 1; }
      else if (tsel == 4) sandbox ^= 1;
      else continue;
      sfx_food(); ng_text();
    }
    if ((p & J_A) && tsel == 4) { sandbox ^= 1; sfx_food(); ng_text(); continue; }
    if ((p & J_START) || ((p & J_A) && tsel != 4)) {
      tutor = 0; sfx_mana(); snap_put();
      if (mapt == MMY && !has_my) { sandbox = 0; return 2; }
      return 1;
    }
  }
}
// ---------- the HOME screen: six picture cards, move freely with the D-pad ----------
#define NCARD 6
static const char *const CARDN[NCARD] = {"PLAY", "EMPIRE", "LEARN", "EDITOR", "EXTRAS", "OPTIONS"};
static const char *const CARDD[NCARD] = {"FIGHT THE RED ANTS", "BUILD A WHOLE ANT CITY", "LEARN TO PLAY STEP BY STEP",
  "DRAW YOUR OWN MAP", "TROPHIES  CHEATS  RECORDS", "SOUND  SPEED  COLORS"};
static u8 hsel;
static u8 card_x(u8 c) { return (u8)(1 + (c % 3) * 10); }
static u8 card_y(u8 c) { return (u8)(c < 3 ? 4 : 11); }
static u8 card_lock(u8 c) { return c == 1 && !emp_unlocked(); }
static void card_draw(u8 c) {                      // bark frame + dark tunnel inside; the chosen card uses the gold palette bank 6
  u8 x0 = card_x(c), y0 = card_y(c), x, y, pal = c == hsel ? 6 : 0; u16 t; const char *n = card_lock(c) ? "LOCKED" : CARDN[c];
  for (y = y0; y <= y0 + 6; y++) for (x = x0; x <= x0 + 8; x++) {
    if (y == y0) t = x == x0 ? UIB + 4 : x == x0 + 8 ? UIB + 6 : UIB + 5;
    else if (y == y0 + 6) t = x == x0 ? UIB + 9 : x == x0 + 8 ? UIB + 11 : UIB + 10;
    else if (x == x0) t = UIB + 7; else if (x == x0 + 8) t = UIB + 8;
    else t = UIB + 31;
    BGMAP3[y * 32 + x] = (u16)(t | (pal << 12));
    if (y > y0 && y < y0 + 6 && x > x0 && x < x0 + 8) BGMAP1[y * 32 + x] = 0;
  }
  ps((u8)(x0 + 1 + (7 - blen2(n)) / 2), (u8)(y0 + 5), n, card_lock(c) ? 15 : c == hsel ? 9 : 3);
}
static void hub_desc(void) {
  const char *d = card_lock(hsel) ? "WIN ONE GAME TO UNLOCK" : CARDD[hsel];
  ov_fill(0, 0, 18, 18); ps((u8)(15 - (blen2(d) + 1) / 2), 18, d, card_lock(hsel) ? 8 : 2);
}
static void hub_scene(void) {
  u8 x, y, c;
  for (y = 0; y < 20; y++) for (x = 0; x < 32; x++) {
    BGMAP3[y * 32 + x] = ((x * 11 + y * 5) % 17 == 0) ? UIB + 30 : uisoil(x, y);
    BGMAP1[y * 32 + x] = 0;
  }
  big(0, "EMPIRE ANTS");
  ps(5, 2, "DIPPINN PRODUCTIONS", 10);
  for (c = 0; c < NCARD; c++) card_draw(c);
  hub_desc();
  ps(2, 19, "\1 OPEN  \3 EMPIRE  \4 OPTIONS", 10);
  for (x = 0; x < 16; x++) BGPAL[96 + x] = BGPAL[x];
}
static void hub_glow(u8 fr) {                      // the chosen card's bark frame shines gold and pulses
  u8 t = (u8)(fr & 63), s = (u8)(t < 32 ? t : 63 - t);   // 0..31..0
  BGPAL[96 + 8] = RGB(12 + s / 6, 7 + s / 8, 1);
  BGPAL[96 + 9] = RGB(22 + s / 6, 15 + s / 6, 2);
  BGPAL[96 + 10] = RGB(31, 25 + s / 6, 6 + s / 3);
}
static u8 wx[5];
static u8 kk_busy;
static u8 kk_mid(void) { return kk_busy; }
static u8 title(void) {                            // returns 0 = play, 1 = open the map editor, 2 = empire, 3 = extras
  static const s8 BOB[16] = {0, -1, -2, -3, -3, -4, -4, -4, -4, -4, -3, -3, -2, -1, 0, 0};
  static const u16 ICO[NCARD] = {BIGT, BIGT + 32, ICNT_T, ICNT_T + 16, ICNT_T + 32, ICNT_T + 48};
  u8 i, fr = 0, edit = 0, old; u16 k, p, prev = joy();
  (void)kk_mid;
  DISPCNT = 0x80; BLDY = 16;
  hide_all(); oam_flush();
  hub_scene(); hub_glow(0);
  BG1HOFS = 4; BG3HOFS = 4;                        // centre the 3 columns of cards
  wx[0] = 14; wx[1] = 62; wx[2] = 112; wx[3] = 200; wx[4] = 150;
  DISPCNT = 0x1B40;
  tm_start();
  fade(16, 0);
  for (;;) {
    vsync(); oam_flush(); hub_glow(fr);
    fr++; rs ^= fr + (u32)VCOUNT * 2654435761u; if (!rs) rs = 1;
    k = joy(); p = k & ~prev; prev = k;
    pool_mix(fr ^ ((u32)VCOUNT << 8) ^ ((u32)k << 20));
    if (p) pool_mix(rs);
    if (p) {                                        // the Konami code unlocks EMPIRE MODE
      static const u16 KON[10] = {J_UP, J_UP, J_DOWN, J_DOWN, J_LEFT, J_RIGHT, J_LEFT, J_RIGHT, J_B, J_A}; static u8 kk;
      if (p == KON[kk]) { if (++kk == 10) { kk = 0; SRAM[62] = 'U'; ach_get(31); sfx_mana(); jingle(WIN_TUNE, 6);
          hsel = 1; hub_scene(); ov_fill(0, 0, 18, 18); ps(4, 18, "EMPIRE MODE UNLOCKED! \3", 9); continue; } }
      else kk = (u8)(p == KON[0]);
      kk_busy = (u8)(kk >= 8);
    }
    old = hsel;
    if (p & J_LEFT)  hsel = (u8)(hsel % 3 ? hsel - 1 : hsel + 2);
    if (p & J_RIGHT) hsel = (u8)(hsel % 3 == 2 ? hsel - 2 : hsel + 1);
    if (p & (J_UP | J_DOWN)) hsel = (u8)((hsel + 3) % 6);
    if (hsel != old) { sfx_food(); card_draw(old); card_draw(hsel); hub_desc(); }
    if (p & J_L) hsel = 1, p |= J_A;                // shortcuts: L = EMPIRE, R = OPTIONS
    if (p & J_R) hsel = 5, p |= J_A;
    if (p & (J_A | J_START)) {
      if (hsel == 0) {                              // PLAY: the NEW GAME screen
        u8 r;
        BG1HOFS = 0; BG3HOFS = 0; sfx_mana();
        r = newgame_menu();
        if (r == 1) { edit = 0; break; }
        if (r == 2) { edit = 1; break; }
        hide_all(); oam_flush(); hub_scene(); BG1HOFS = 4; BG3HOFS = 4; prev = joy(); continue;
      }
      if (hsel == 1) { if (emp_unlocked()) { edit = 2; break; } sfx_deny(); card_draw(1); hub_desc(); continue; }
      if (hsel == 2) { tutor = 1; sandbox = 0; break; }
      if (hsel == 3) { edit = 1; break; }
      if (hsel == 4) { edit = 3; break; }
      BG1HOFS = 0; BG3HOFS = 0; sfx_mana();         // OPTIONS
      options_screen(); hide_all(); oam_flush();
      DISPCNT = 0x1B40; hub_scene(); BG1HOFS = 4; BG3HOFS = 4; prev = joy();
      if (!nomus && !tmu) tm_start();
      continue;
    }
    for (i = 0; i < NCARD; i++) {                   // the card pictures: 32x32 sprites; the chosen one bobs
      int sx = card_x(i) * 8 - 4 + 20, sy = card_y(i) * 8 + 8 + (i == hsel ? BOB[(fr >> 2) & 15] : 0);
      u8 pal = card_lock(i) ? 10 : i < 2 ? 0 : 6;
      spr((u8)(8 + i), sx, sy, (u8)ICO[i], pal, 0, 0); oam[(8 + i) * 4 + 1] |= 0x8000;
    }
    for (i = 0; i < 5; i++) {                      // 3 black ants march right, 2 red ants left, under the logo
      if (!(fr & 1)) { if (i < 3) { if (++wx[i] >= 248) wx[i] = 0; } else wx[i] = wx[i] ? wx[i] - 1 : 247; }
      spr(i, (int)wx[i] - 8, 24, (u8)(3 + (((fr >> 3) + i) & 1)), i < 3 ? 0 : 1, 0, i >= 3);
    }
  }
  fade(0, 16);
  tm_stop();
  BG1HOFS = 0; BG3HOFS = 0;
  hide_all(); oam_flush();
  ov_clear();
  while (joy()) vsync();
  return edit;
}

// ---------- ANT EYE: first-person view from a black ant (SELECT+START) ----------
// Video mode 1. BG2 (affine) is a Mode-7 floor: the 32x32 world as 8bpp tiles in the real terrain colours, with a grid line on
// every cell so you can count steps. HBlank DMA0 rewrites PA/PC + the reference point per scanline (perspective), HBlank DMA1
// fades the far floor into the sky (BLDALPHA). Ants, queens and food stand on the floor as hardware-scaled 32x32 sprites
// (one affine matrix each, nearest first). BG1 row 0 is a compass strip, rows 16-19 the status panel.
// Projection (same as the floor): an object D cells ahead and S cells to the right is at x = 120 + 192*S/D, its feet at
// y = 64 + 31.5/D.
#define HZ 64                                         // horizon scanline
static s16 fox, foy;                                  // world cell shown at the top-left of the 64x64 floor map (re-centred as you walk)
static u16 skyg[161];                                 // per scanline backdrop: sky gradient, then the haze the floor fades into
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
// BIGT (24): OBJ tiles of the big sprites: ant 24, carrying ant 40, queen 56, food 72
#ifdef __arm__
#define IWRAM __attribute__((section(".data.iwram"), long_call, noinline))   // hot code runs from IWRAM (ROM is ~6x slower)
#else
#define IWRAM
#endif
typedef struct { u32 ab, cd; s32 x, y; } M7;          // PA|PB<<16, PC|PD<<16, BG2X, BG2Y = registers 0x04000020..2F
static const char *const COMPASS[8] = {"EAST", "SOUTH EAST", "SOUTH", "SOUTH WEST", "WEST", "NORTH WEST", "NORTH", "NORTH EAST"};
EWRAM static u8 occ[MAXH][MAXW]; static u8 vang;                            // vang = facing, 0..255
static u16 KK[160], KQ[160], blt[160];                // per scanline: ground distance (cells * 2048), K * 4/3, BLDALPHA fog
static M7 m7t[2][161];                                // double-buffered HBlank DMA tables
static void view_init(void) {
  u16 y; s32 k;
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
// The ground as the Mode-7 hardware sees it. Screen pixel (x, y) shows the point P = cam + D*fwd + D*(u/192)*right, u = x + 0.5 - 120,
// with D = K(y) cells ahead. In 8.8 texels (1 cell = 8 texels): PA,PC = K/192 * right, X0,Y0 = cam + K*fwd - 120 * (PA,PC).
IWRAM static void m7_build(M7 *t, u8 ang, s16 fx, s16 fy) {
  s32 cs = SINT[(ang + 64) & 255], sn = SINT[ang], bx = ((s32)fx - fox * 256) * 8, by = ((s32)fy - foy * 256) * 8, K, q, pa, pc;
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
  DMAR(2, 8) = 0; BGPAL[0] = skyg[0];
  DMAR(2, 0) = (u32)(skyg + 1); DMAR(2, 4) = 0x05000000u; DMAR(2, 8) = DMA_ON | DMA_HBL | DMA_REP | DMA_FIX | 1;
}
// floor: 8bpp 8x8 tiles 180..188 (free space at the end of charblock 2), colours in palette bank 6 (96..110).
// 180 water, 181/182 sand, 183/184 grass, 185/186 hill (two shades, chequered by cell so you can see yourself move), 187 nest, 188 outside
#define FT8 180
static void floor_tiles(void) {
  static const u8 BASE[9] = {96, 98, 99, 101, 102, 104, 105, 107, 109}, SPEC[9] = {97, 100, 100, 103, 103, 106, 106, 107, 110};
  volatile u32 *d = TILE32 + 0x2000 + FT8 * 16;
  u8 t, p, k, px, py, hh, c; u32 w;
  for (t = 0; t < 9; t++) for (p = 0; p < 16; p++) {
    w = 0;
    for (k = 0; k < 4; k++) {
      px = (u8)((p & 1) * 4 + k); py = (u8)(p >> 1);
      hh = (u8)((px * 37 + py * 91 + t * 53) & 15);
      if (t == 0) c = ((py & 3) == 1 && ((px + py) & 7) < 3) ? SPEC[0] : BASE[0];
      else if (t == 7) c = (px >= 2 && px <= 5 && py >= 2 && py <= 5) ? 108 : BASE[7];
      else c = hh < 2 ? SPEC[t] : BASE[t];
      w |= (u32)c << (k * 8);
    }
    d[t * 16 + p] = w;
  }
}
static void floor_map(volatile u16 *m) {              // 64x64 one-byte entries, read as halfwords
  u8 x, y, k, h, wx, wy, c; u16 e;
  for (y = 0; y < 64; y++) for (x = 0; x < 64; x += 2) {
    e = 0;
    for (k = 0; k < 2; k++) {
      wx = (u8)(x + k + fox); wy = (u8)(y + foy);
      if (wx >= W || wy >= H) c = FT8 + 8;
      else if (is_nest(wx, wy)) c = FT8 + 7;
      else { h = hgt[wy][wx]; c = h == 0 ? FT8 : (u8)(FT8 - 1 + h * 2 + ((wx + wy) & 1)); }
      e |= (u16)c << (k * 8);
    }
    m[y * 32 + x / 2] = e;
  }
}
static void sky_init(void) {                           // DAY / SUNSET / NIGHT
  static const u8 TOP[3][3] = {{7, 13, 27}, {10, 6, 16}, {1, 1, 5}}, HOR[3][3] = {{23, 25, 31}, {31, 18, 10}, {5, 7, 14}};
  u8 y, t, k = O(O_SKY), c[3], j;
  for (y = 0; y < 161; y++) {
    t = y < 64 ? y / 4 : 16;                           // 0 at the top .. 16 at the horizon
    for (j = 0; j < 3; j++) c[j] = (u8)(TOP[k][j] + (HOR[k][j] - TOP[k][j]) * t / 16);
    skyg[y] = RGB(c[0], c[1], c[2]);
  }
}
static void pano_init(void) {                          // skyline on BG0: tiles 64.. of charblock 0, map in screenblock 27
  u16 i; volatile u16 *m = (volatile u16 *)0x0600D800;
  for (i = 0; i < PANO_N * 8; i++) TILE32[64 * 8 + i] = PANO[i];
  for (i = 0; i < 1024; i++) m[i] = i < 256 && PANOM[i] != 0xFFFF ? (u16)((64 + PANOM[i]) | (5 << 12)) : 0;
  for (i = 16 * 32; i < 20 * 32; i++) m[i] = i < 17 * 32 ? (u16)(UIB + 12 + ((i * 5) & 1)) : uisoil((u8)(i & 31), (u8)(i >> 5));   // dirt under the view
  for (i = 0; i < 16; i++) {                            // the skyline, tinted for sunset / night
    u16 v = PAL_PANO[i]; u8 r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
    if (O(O_SKY) == 1) { r = (u8)(r + 6 > 31 ? 31 : r + 6); b = (u8)(b * 2 / 3); }
    else if (O(O_SKY) == 2) { r >>= 2; g >>= 2; b = (u8)(b / 2 + 2); }
    BGPAL[80 + i] = RGB(r, g, b);
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
static const char *eye_ahead(u8 vx, u8 vy) {         // what a step forward would run into
  s8 dx, dy, a, b, d; u8 o;
  eye_dir(&dx, &dy);
  a = (s8)vx + dx; b = (s8)vy + dy;
  if (a < 0 || b < 0 || a >= W || b >= H) return "MAP EDGE";
  o = occ[b][a];
  if (o) return o == 1 ? "FRIEND" : o == 2 ? "ENEMY!" : o == 3 ? "HOME" : "RED NEST";
  if (hgt[b][a] == 0) return "WATER";
  d = (s8)hgt[b][a] - (s8)hgt[vy][vx];
  return d > 1 ? "CLIFF UP" : d < -1 ? "CLIFF DOWN" : d == 1 ? "STEP UP" : d == -1 ? "STEP DOWN" : "CLEAR";
}
// Status panel (rows 16-19):  ANT 3 OF 12        LEVEL 2 GRASS  /  FACING SOUTH   AHEAD STEP UP  /  the controls
static void eye_status(u8 vi, u8 vx, u8 vy) {
  static const char *const LN[4] = {"WATER", "SAND", "GRASS", "HILL"};
  u8 i, n = 0, r = 0, x;
  ov_fill(1, 1, 16, 19);
  if (vi < MAXA) {
    for (i = 0; i < MAXA; i++) if (vok(i)) { n++; if (i == vi) r = n; }
    ps(0, 16, "ANT", 2); pn(4, 16, r, 1); ps(7, 16, "OF", 2); pn(10, 16, n, 1);
  } else ps(0, 16, "YOUR QUEEN", 2);
  ps(15, 16, "LEVEL", 2); pc(21, 16, (char)('0' + hgt[vy][vx]), 1); ps(23, 16, LN[hgt[vy][vx]], 1);
  ps(0, 17, "FACING", 2); x = psx(7, 17, COMPASS[((vang + 16) >> 5) & 7], 1);
  { const char *a = eye_ahead(vx, vy); u8 bad = a[0] == 'W' || a[0] == 'M' || (a[0] == 'E') || (a[0] == 'C' && a[1] == 'L' && a[2] == 'I');
    ps(18, 17, ">", 2); ps(20, 17, a, bad ? 8 : 13); }
  (void)x;
  ps(0, 18, "UP/DOWN WALK  LEFT/RIGHT TURN", 1);
  ps(0, 19, "\1 NEXT ANT \2 CURSOR \5 EXIT", 1);
}
static void eye_step(u8 *vx, u8 *vy, s8 dir) {
  s8 dx, dy;
  eye_dir(&dx, &dy);
  dx *= dir; dy *= dir;
  if (can_go(*vx, *vy, dx, dy)) { *vx += dx; *vy += dy; }
}
static const u8 EWK[3] = {28, 20, 12}, EGL[3] = {10, 14, 24};   // ant eye: frames per step, glide speed
static s16 ease(s16 v, s16 t, s16 s) { s16 d = t - v; return d > s ? v + s : d < -s ? v - s : t; }
static u8 bearing(s16 dx, s16 dy) {                   // angle (0..255, 0 = east, clockwise) that points along (dx, dy)
  u8 a, b = 0; s32 v, bv = -0x7FFFFFFF;
  for (a = 0; a < 64; a++) { v = (s32)dx * SINT[((a << 2) + 64) & 255] + (s32)dy * SINT[a << 2]; if (v > bv) { bv = v; b = (u8)(a << 2); } }
  return b;
}
static void eye_compass(u8 fa, u8 vx, u8 vy) {        // row 0: N E S W letters + the two nests where they are around you
  static const char CL[4] = {'E', 'S', 'W', 'N'};
  u8 k; s16 r; s8 c;
  ov_fill(1, 1, 0, 0);
  pc(15, 0, '+', 15);
  for (k = 0; k < 4; k++) { r = (s16)(((k * 64 - fa + 128) & 255) - 128); c = (s8)(15 + r * 15 / 64); if (c >= 0 && c < 30) pc((u8)c, 0, CL[k], 2); }
  for (k = 0; k < 2; k++) {
    if (vx == nestx[k] && vy == nesty[k]) continue;
    r = (s16)(((bearing((s16)nestx[k] - vx, (s16)nesty[k] - vy) - fa + 128) & 255) - 128);
    c = (s8)(15 + r * 15 / 64); if (c < 0) c = 0; if (c > 29) c = 29;
    pc((u8)c, 0, k ? '@' : '$', k ? 8 : 1);
  }
}
// objects on the floor: up to 32, nearest drawn in front. kind: 0 black ant, 1 red ant, 2/3 carrying, 4 your queen, 5 red queen, 6 food
static s16 obd[64]; static u8 obx[64], oby[64], obk[64], obf[64], obn;
static void ob_add(u8 x, u8 y, u8 k, u8 f) { if (obn < 64) { obx[obn] = x; oby[obn] = y; obk[obn] = k; obf[obn] = f; obn++; } }
static void eye_objs(u8 fa, s16 fx, s16 fy, u8 vi) {
  s32 cs = SINT[(fa + 64) & 255], sn = SINT[fa], rx, ry, d, sd, sc, xx, yg, cy0;
  u8 i, j, n, k, ord[64], t;
  obn = 0;
  for (i = 0; i < MAXA; i++) if (ant[i].alive && i != vi) ob_add(ant[i].x, ant[i].y, (u8)(ant[i].team + (ant[i].carry ? 2 : 0)), ant[i].fl);
  for (i = 0; i < 2; i++) if (qhp[i]) ob_add(nestx[i], nesty[i], (u8)(4 + i), i);
  { s16 x0 = (fx >> 8) - 14, y0 = (fy >> 8) - 14, xx2, yy2;      // food: only the cells around you
    for (yy2 = y0; yy2 <= y0 + 28; yy2++) if (yy2 >= 0 && yy2 < H) for (xx2 = x0; xx2 <= x0 + 28; xx2++)
      if (xx2 >= 0 && xx2 < W && food[yy2][xx2] && hgt[yy2][xx2]) ob_add((u8)xx2, (u8)yy2, 6, 0); }
  for (i = 0, n = 0; i < obn; i++) {                  // distance ahead (1/256 cells); keep what is in front and not too far
    rx = (s32)obx[i] * 256 + 128 - fx; ry = (s32)oby[i] * 256 + 128 - fy;
    if (obk[i] < 4) { rx += (i * 37 & 63) - 32; ry += (i * 23 & 63) - 32; }   // ants on the same cell do not overlap exactly
    d = (rx * cs + ry * sn) >> 8;
    if (d < 220 || d > 16 * 256) continue;
    obd[i] = (s16)d;
    for (j = n; j > 0 && obd[ord[j - 1]] > d; j--) ord[j] = ord[j - 1];   // insertion sort, nearest first
    ord[j] = i; n++;
  }
  hide_all();
  for (j = 0, t = 0; j < n && t < 32; j++) {
    i = ord[j]; k = obk[i];
    rx = (s32)obx[i] * 256 + 128 - fx; ry = (s32)oby[i] * 256 + 128 - fy;
    if (k < 4) { rx += (i * 37 & 63) - 32; ry += (i * 23 & 63) - 32; }
    d = obd[i]; sd = (-rx * sn + ry * cs) >> 8;
    xx = 120 + 192 * sd / d; yg = 64 + 8064 / d; if (yg > 127) yg = 127;
    sc = 251658 / d;                                  // sprite scale * 256 (an ant is ~0.6 cells long)
    if (k >= 4 && k < 6) sc = sc * 3 / 2; else if (k == 6) sc = sc * 3 / 4;
    if (sc > 512) sc = 512; if (sc < 24) sc = 24;
    cy0 = yg - ((k == 6 ? 15 : 13) * sc >> 8);
    if (xx < -40 || xx > 280) continue;
    { s16 pa = (s16)(65536 / sc);                     // the ant looks the way it walks: its x heading against the view's right vector
      if (k < 6 && ((obf[i] ? 1 : -1) * sn > 0)) pa = (s16)-pa;   // right = (-sn, cs): facing +x shows as facing right when sn < 0
      oam[(t * 4) * 4 + 3] = (u16)pa; oam[(t * 4 + 1) * 4 + 3] = 0; oam[(t * 4 + 2) * 4 + 3] = 0; oam[(t * 4 + 3) * 4 + 3] = (u16)(65536 / sc); }
    oam[t * 4] = (u16)(((cy0 - 32) & 0xFF) | 0x300);
    oam[t * 4 + 1] = (u16)(((xx - 32) & 0x1FF) | (t << 9) | (2 << 14));
    oam[t * 4 + 2] = (u16)((BIGT + (k == 6 ? 48 : k >= 4 ? 32 : k >= 2 ? 16 : 0)) | (1 << 10) | ((k == 6 ? 6 : (k & 1)) << 12));
    t++;
  }
}
static void eye(void) {                              // time stands still while you look through a black ant's eyes
  u8 i, vi = MAXA, best = 255, d, rep = 0, vx = 0, vy = 0, dirs, rt = 0, cur = 0, jump, chg;
  s16 fa = vang, fx, fy, tx, ty, da;
  s8 td, wd, tlast = 0, wlast = 0;
  u16 k, p, prev, sky = BGPAL[0];
  tev |= 16; ach_get(7);
  for (i = 0; i < MAXA; i++) if (vok(i)) { d = (u8)(dist(ant[i].x, cx) + dist(ant[i].y, cy)); if (d < best) { best = d; vi = i; } }
  if (vi < MAXA) { vx = ant[vi].x; vy = ant[vi].y; vang = ant[vi].fl ? 128 : 0; } else { vx = nestx[0]; vy = nesty[0]; vang = 192; }
  fa = vang; fx = (s16)(vx * 256 + 128); fy = (s16)(vy * 256 + 128);
  hide_all(); vsync(); oam_flush();
  BGPAL[96] = RGB(6,13,26);   BGPAL[97] = RGB(13,21,31);                                   // water
  BGPAL[98] = RGB(28,24,14);  BGPAL[99] = RGB(26,22,12);  BGPAL[100] = RGB(23,19,10);          // sand
  BGPAL[101] = RGB(9,21,7);   BGPAL[102] = RGB(8,19,6);   BGPAL[103] = RGB(13,25,9);           // grass
  BGPAL[104] = RGB(20,14,8);  BGPAL[105] = RGB(18,12,7);  BGPAL[106] = RGB(25,20,14);          // hill
  BGPAL[107] = RGB(15,9,4);   BGPAL[108] = RGB(1,1,1);                                         // nest + hole
  BGPAL[109] = RGB(11,10,9);  BGPAL[110] = RGB(8,7,6);                                         // outside the map
  sky_init(); pano_init();
  ov_clear();
  occ_set(1);
  floor_tiles();
  fox = (s16)vx - 32; foy = (s16)vy - 32;
  floor_map((volatile u16 *)0x0600B000);              // screenblock 22 (free space at the end of charblock 2), 64x64
  m7_sky(m7t[0]); m7_sky(m7t[1]);
  m7_build(m7t[0], vang, fx, fy);
  eye_status(vi, vx, vy); eye_compass((u8)fa, vx, vy);
  eye_objs((u8)fa, fx, fy, vi);
  while (joy()) vsync();
  vsync(); eye_arm(m7t[0]); oam_flush();
  BG2CNT = 0x8000 | (22 << 8) | (2 << 2) | 3 | 0x80;  // BG2: priority 3, charblock 2, screenblock 22, 512x512 affine, 8bpp
  BLDCNT = 0x2044;                                    // alpha blend: BG2 over the backdrop (= sky colour)
  BG0CNT = 3 | (27 << 8); BG0HOFS = (u16)(fa * 5 + (fa >> 2)); BG0VOFS = 0;   // BG0: skyline (charblock 0, screenblock 27)
  DISPCNT = 0x1741;                                   // mode 1: BG0 + BG1 + BG2 + OBJ
  prev = 0;
  for (;;) {
    vsync();
    eye_arm(m7t[cur]); oam_flush();                   // the table and sprites built last frame
    k = joy(); p = k & ~prev; prev = k;
    if (p & J_START) break;
    if (p & J_B) { cx = vx; cy = vy; break; }
    jump = chg = 0;
    if (p & J_A) {
      i = 0; do { vi = (vi == MAXA) ? 0 : vi + 1; } while (!vok(vi) && ++i <= MAXA); sfx_food();
      if (vi < MAXA) { vx = ant[vi].x; vy = ant[vi].y; } else { vx = nestx[0]; vy = nesty[0]; }
      jump = chg = 1;
    }
    dirs = (u8)((k & 15) | ((k & J_L) ? J_LEFT : 0) | ((k & J_R) ? J_RIGHT : 0));   // L / R turn too
    td = (dirs & J_RIGHT) ? 1 : (dirs & J_LEFT) ? -1 : 0;   // turning and walking are independent: walk while you turn
    wd = (dirs & J_UP) ? 1 : (dirs & J_DOWN) ? -1 : 0;
    if (td != tlast) { rt = 0; tlast = td; }
    if (wd != wlast) { rep = 0; wlast = wd; }
    if (td) {
      if (rt == 0) { vang = (u8)(vang + td * TURN); chg = 1; }          // a tap = one 11.25 degree click
      else if (rt >= 14 && !(rt & 1)) { vang = (u8)(vang + td); chg = 1; }   // held = a slow, smooth sweep
      if (rt < 250) rt++;
    } else if (vang & 7) { vang = (u8)((vang + 4) & 0xF8); chg = 1; }   // let go: settle on the 8-unit grid so walking stays square
    if (wd) {
      if (rep == 0 || (rep >= EWK[O(O_EYESPD)] && !(rep % EWK[O(O_EYESPD)]))) {   // held = keep walking
        u8 ox = vx, oy = vy; eye_step(&vx, &vy, wd);
        if (vx != ox || vy != oy) { chg = 1; sfx_lower(); } else if (!(rep & 15)) sfx_deny();   // bumped into something: a thud
      }
      if (rep < 250) rep++;
    }
    tx = (s16)(vx * 256 + 128); ty = (s16)(vy * 256 + 128);
    if (jump) { fx = tx; fy = ty; fa = vang; }        // jumping to another ant: no glide
    if (chg) eye_status(vi, vx, vy);
    da = (s16)(((vang - fa + 128) & 255) - 128);      // ease the camera toward the wanted pose
    fa = (s16)((fa + (da > 2 ? 2 : da < -2 ? -2 : da)) & 255);
    fx = ease(fx, tx, EGL[O(O_EYESPD)]); fy = ease(fy, ty, EGL[O(O_EYESPD)]);
    if (vx - fox < 16 || vx - fox > 47 || vy - foy < 16 || vy - foy > 47) {   // walked far: re-centre the floor map
      fox = (s16)vx - 32; foy = (s16)vy - 32; floor_map((volatile u16 *)0x0600B000);
    }
    cur ^= 1;
    m7_build(m7t[cur], (u8)fa, fx, fy);               // next frame's table (the other buffer is being read by DMA)
    eye_compass((u8)fa, vx, vy);
    eye_objs((u8)fa, fx, fy, vi);
    BG0HOFS = (u16)(fa * 5 + (fa >> 2));               // the skyline turns with you
  }
  vsync();
  DMAR(0, 8) = 0; DMAR(1, 8) = 0; DMAR(2, 8) = 0;
  occ_set(0);
  hide_all(); oam_flush();
  BGPAL[0] = sky;
  BG0CNT = 2 | (2 << 2) | (28 << 8); BG2CNT = 1 | (2 << 2) | (30 << 8); DISPCNT = 0x1F40; BLDCNT = 0xFF;   // back to terrain + decor
  ov_clear(); hud();
  while (joy()) vsync();
}

// ---------- TUTORIAL: lessons (title screen: press A) ----------
// Each lesson is a full-screen card (START next, B skip); lessons with a task then show a live hint on the HUD until you do it.
#define TN 10
static const u8 TEV[TN] = {0, 1, 2, 4, 0, 8, 16, 0, 0, 0};          // event bit that completes each lesson (0 = read only)
static const char *const THINT[TN] = {0, "TRY: MOVE THE CURSOR", "TRY: PRESS A TO RAISE", "TRY: PRESS B TO LOWER", 0,
  "PRESS L TO FLOOD", "SEL+START: ANT EYE", 0, 0, 0};
static const char *const TTITLE[TN] = {"WELCOME ANTPEROR!", "THE CURSOR", "RAISE LAND", "LOWER LAND", "ANT POWER", "FLOOD", "ANT EYE", "THE COLONY", "POPULARITY", "READY TO RULE!"};
// Lesson text: 7 lines of up to 28 letters (the font has A-Z 0-9 : ! / . - + ? only). *asterisks* switch gold highlighting on and off.
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
  "BY ONE LEVEL: COST *1 AP*\n"
  "ANTS ONLY STEP *1 LEVEL*:\n"
  "SOFT EDGES = STEPS, OK\n"
  "*DARK EDGES = CLIFFS*: NO WAY\n"
  "BUILD RAMPS TO CONNECT LAND\n"
  "NESTS CANT BE CHANGED",
  "*B* LOWERS THE TILE BY ONE\n"
  "LEVEL: COST *1 AP*\n"
  "\n"
  "LEVEL 0 IS *WATER*: ANTS CANT\n"
  "WALK OR LIVE THERE: DIG\n"
  "MOATS TO STOP RED ANTS AND\n"
  "RAISE LAND TO BRIDGE GAPS",
  "*AP* IS ANT POWER: EVERY\n"
  "EDIT COSTS AP: GET MORE BY:\n"
  "*WAITING* A LITTLE\n"
  "*FOOD* YOUR ANTS BRING HOME\n"
  "*ANTLECTION* WINS\n"
  "\n"
  "*SEL+A* TRADES 2 FOOD: 4 AP",
  "*L* LOWERS A 3X3 AREA BY ONE\n"
  "LEVEL: COST *8 AP*\n"
  "\n"
  "ANTS ON TILES THAT HIT LEVEL\n"
  "0 *DROWN*: GREAT AGAINST RED\n"
  "ARMIES BUT CAREFUL WITH\n"
  "YOUR OWN!",
  "HOLD *SEL* AND TAP *START*:\n"
  "SEE THE WORLD AS AN ANT!\n"
  "*UP DOWN* WALK *LEFT RIGHT* TURN\n"
  "*A* NEXT ANT  *B* CURSOR HERE\n"
  "TOP BAR: *N E S W* COMPASS\n"
  "CROWN: HOME   RED ANT: ENEMY\n"
  "TIME STOPS WHILE YOU LOOK",
  "*BLACK ANTS* FIND FOOD AND\n"
  "CARRY IT HOME: *3 FOOD*\n"
  "HATCHES A NEW ANT\n"
  "EVERY 4TH ANT IS A *SOLDIER*:\n"
  "WITH 8 ANTS THEY MARCH ON\n"
  "THE RED NEST\n"
  "ANTS FOLLOW *TRAILS*!",
  "*SMILE* IS POPULARITY: FOOD\n"
  "AND NEW ANTS RAISE IT: DEAD\n"
  "ANTS AND HUNGER CUT IT\n"
  "\n"
  "ANTLECTION EVERY MINUTE:\n"
  "*50 UP*: +8 AP AND A PERK\n"
  "*UNDER 25*: COUP! AP LOST",
  "KILL THE *RED QUEEN* TO WIN:\n"
  "LOSE YOURS AND ITS OVER\n"
  "\n"
  "*SELECT* JUMPS: HOME ARMY RED\n"
  "*START* PAUSE MENU\n"
  "*R* FAST FORWARD\n"
  "*LONG LIVE THE QUEEN!*",
};

// ----- card tiles (built once at boot, above the ANT EYE overlay tiles) and the picture canvas -----
#define BIGF 544                                       // 2x gold title font, 2 tiles per glyph (top, bottom): 544..623
#define BAND 624                                       // the picture: BW x BH tiles: 624..763
// ORN 764:                                      // 764 rule, 765/766 centre diamond, 767/768 pip on/off, 769 arrow
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
  BGPAL[145] = RGB(2,1,0); BGPAL[146] = RGB(16,9,3); BGPAL[147] = RGB(31,27,8);       // bank 9: gold on night (titles, highlights, rules)
  BGPAL[161] = RGB(2,1,0); BGPAL[162] = RGB(12,9,6);  BGPAL[163] = RGB(26,21,14);   // tan      // bank 10: dim steel (small labels)
  for (i = 0; i < 16; i++) BGPAL[112 + i] = PIC[i];                                    // bank 7: the picture palette (see C_*)
  for (g = 0; g < 40; g++) {                                                           // big font: the 3x5 glyph doubled, shadow then face
    for (y = 0; y < 16; y++) for (x = 0; x < 8; x++) p[y][x] = 0;
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
  tdraw(ORN,     "00000000" "00000000" "00000000" "22222222" "23222322" "00000200" "00000000" "00000000");   // twig
  tdraw(ORN + 1, "00000000" "00000003" "00000033" "22223333" "22333332" "00000333" "00000030" "00000000");   // leaf (left half)
  tdraw(ORN + 2, "00000000" "30000000" "33000000" "33332222" "23332222" "33300000" "03000000" "00000000");   // leaf (right half)
  tdraw(ORN + 3, "00000000" "00000000" "00333300" "03333330" "03332330" "00333300" "00000000" "00000000");   // seed (done)
  tdraw(ORN + 4, "00000000" "00000000" "00222200" "02000020" "02000020" "00222200" "00000000" "00000000");   // seed (to do)
  tdraw(ORN + 5, "00000303" "00000030" "02200333" "22223323" "22223333" "02202330" "02020200" "00000000");   // ant pointer
}

// ----- the picture band. Each card draws its static background ONCE into bgbuf (EWRAM). Every frame tut_anim copies it to sbuf,
// paints the moving parts on top, and band_put() DMAs the result into the band tiles during VBlank: smooth, no tearing -----
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
    btext(192, 20, "1 LEVEL", 1, C_CREAM); btext(192, 27, "1 AP", 1, C_GOLD);
    break;
  case 3:
    bground(26, C_GRASS);
    break;
  case 4:
    btext(8, 5, "AP", 2, C_GOLD);
    bdisc(70, 26, 4, C_WATER); btri(67, 17, 7, 6, C_WATER); bctext(70, 33, "TRICKLE", 1, C_CREAM);
    bdisc(112, 25, 5, C_LEAF); bdisc(111, 24, 3, C_GRASS); brect(111, 18, 2, 3, C_HILL); bctext(112, 33, "FOOD", 1, C_CREAM);
    brect(154, 25, 15, 8, C_GRAY); brect(157, 19, 9, 7, C_CREAM); brect(157, 25, 9, 1, C_NAVY); bctext(162, 33, "ANTLECTION", 1, C_CREAM);
    break;
  case 5:
    bctext(24, 33, "8 AP", 1, C_GOLD);
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
    bctext(88, 17, "NOTHING", 1, C_CREAM); bctext(160, 17, "8 AP AID", 1, C_WHITE);
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
  ov_fill(1, 3, 0, 19); ui_full();
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

// ---------- OPTIONS screen ----------
static const u8 PRESETS[3][O_TIME] = {              // KIDS, CLASSIC, HARD for options 1..13 (index 0 unused)
  {0, 1, 3, 2, 0, 0, 0, 3, 0, 1, 0, 2, 2},
  {0, 0, 1, 1, 1, 1, 1, 2, 2, 0, 0, 1, 1},
  {0, 0, 0, 0, 2, 2, 2, 0, 3, 0, 1, 0, 0}};
static u8 opt_n(u8 i) { const char *v = OPTS[i].vals; u8 n = 1; while (*v) if (*v++ == '|') n++; return n; }
static void opt_val(u8 i, char *b) {                // the text of option i's current value
  const char *v = OPTS[i].vals; u8 k = opt[i], n = 0;
  while (k && *v) if (*v++ == '|') k--;
  while (*v && *v != '|' && n < 9) b[n++] = *v++;
  b[n] = 0;
}
static const char *opt_group(u8 i) { return i <= O_TIME ? "GAME RULES" : i <= O_ALERTS ? "CONTROLS" : i <= O_ANTCOL ? "LOOK" : i <= O_SFX ? "SOUND" : "ANT EYE"; }
static void opts_apply(void) {                      // colours, sound and rules follow the options right away

  static const u16 ANTC[8][4] = {{RGB(1,1,2), RGB(6,6,8), RGB(16,16,19), RGB(14,15,22)},
    {RGB(1,2,9), RGB(4,9,23), RGB(14,20,31), RGB(10,14,28)}, {RGB(1,6,1), RGB(4,16,4), RGB(14,28,12), RGB(10,20,8)},
    {RGB(6,1,8), RGB(15,5,21), RGB(25,17,31), RGB(19,11,27)}, {RGB(10,6,0), RGB(26,20,2), RGB(31,30,14), RGB(31,24,6)},
    {RGB(12,12,14), RGB(26,26,28), RGB(31,31,31), RGB(22,24,30)}, {RGB(14,3,8), RGB(30,12,20), RGB(31,24,28), RGB(31,16,24)},
    {RGB(0,0,0), RGB(2,1,4), RGB(8,4,12), RGB(6,2,10)}};
  u8 i; u16 sh = RGB(2, 1, 0);                      // every text shadow: near-black brown
  BGPAL[17] = BGPAL[33] = BGPAL[49] = BGPAL[65] = BGPAL[129] = BGPAL[145] = BGPAL[161] = BGPAL[209] = BGPAL[225] = BGPAL[241] = sh;
  for (i = 0; i < 12; i++) BGPAL[4 + i] = UIPAL[O(O_PANEL)][i];   // the soil theme (palette bank 0, colours 4..15)
  BGPAL[243] = RGB(15, 12, 9);                      // dim text: dry-leaf brown
  for (i = 0; i < 4; i++) OBJPAL[1 + i] = ANTC[O(O_ANTCOL)][i];
  nosfx = O(O_SFX);
  if (O(O_MUSIC) != nomus) { nomus = O(O_MUSIC); if (nomus) { music_stop(); tmu = 0; } }
  set_rules();
}
static void opts_save(void) {
  u8 i, sum = 0;
  SRAM[24] = 'O'; SRAM[25] = 'P';
  for (i = 0; i < NOPT; i++) { SRAM[26 + i] = opt[i]; sum = (u8)(sum + opt[i] + i); }
  SRAM[60] = sum;
}
static void opts_load(void) {
  u8 i, sum = 0, ok = SRAM[24] == 'O' && SRAM[25] == 'P';
  for (i = 0; i < NOPT; i++) { opt[i] = SRAM[26 + i]; sum = (u8)(sum + opt[i] + i); if (opt[i] >= opt_n(i)) ok = 0; }
  if (!ok || sum != SRAM[60]) for (i = 0; i < NOPT; i++) opt[i] = OPTS[i].def;
  for (i = 0; i < NOPT; i++) if (opt_locked(i, opt[i])) opt[i] = OPTS[i].def;
}
static void opt_draw(u8 sel, u8 top) {
  u8 i, y; char b[10];
  ov_fill(1, 3, 0, 19); ui_full();
  big(0, "OPTIONS");
  ps(1, 2, opt_group(sel), 9); pn(24, 2, (u8)(sel + 1), 3); pc(26, 2, '/', 10); pn(27, 2, NOPT, 3);
  pm_rule(3);
  for (i = 0; i < 10 && top + i < NOPT; i++) {
    u8 k = (u8)(top + i); y = (u8)(4 + i);
    if (k == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
    ps(3, y, OPTS[k].name, k == sel ? 9 : 3);
    opt_val(k, b);
    pc(17, y, '<', 10); ps(19, y, b, k == sel ? 9 : 3); pc(28, y, '>', 10);
  }
  if (top > 0) pc(29, 4, '-', 10);
  if (top + 10 < NOPT) pc(29, 13, '+', 10);
  pm_rule(14);
  ps(1, 15, OPTS[sel].h1, 3); ps(1, 16, OPTS[sel].h2, 3);
  ps(1, 18, "UP DOWN PICK  LEFT RIGHT SET", 10);
  ps(1, 19, "\5 DONE   \6 ALL BACK TO NORMAL", 10);
}
static void options_screen(void) {
  u8 sel = 0, top = 0, i, n; u16 k, p, prev = 0, last = 0, rep = 0;
  hide_all(); vsync(); oam_flush();
  while (joy()) vsync();
  opt_draw(sel, top);
  for (;;) {
    vsync(); k = joy(); p = k & ~prev; prev = k;
    if ((k & (J_UP | J_DOWN)) != last) { rep = 0; last = k & (J_UP | J_DOWN); }
    if (last && ++rep > 14 && !(rep & 3)) p |= last;   // hold UP / DOWN to scroll
    if (!p) continue;
    if (p & (J_START | J_B)) break;
    if (p & J_UP)   { sel = sel ? sel - 1 : NOPT - 1; sfx_food(); }
    if (p & J_DOWN) { sel = sel == NOPT - 1 ? 0 : sel + 1; sfx_food(); }
    if (p & (J_LEFT | J_RIGHT | J_A)) {
      n = opt_n(sel);
      do opt[sel] = (u8)((p & J_LEFT) ? (opt[sel] + n - 1) % n : (opt[sel] + 1) % n); while (opt_locked(sel, opt[sel]));
      if (sel == O_PRESET && opt[sel]) for (i = 1; i < O_TIME; i++) opt[i] = PRESETS[opt[sel] - 1][i];
      else if (sel >= 1 && sel < O_TIME) opt[O_PRESET] = 0;
      opts_apply(); sfx_mana();
    }
    if (p & J_SEL) { for (i = 0; i < NOPT; i++) opt[i] = OPTS[i].def; opts_apply(); sfx_flood(); }
    if (sel < top) top = sel; if (sel >= top + 10) top = (u8)(sel - 9);
    opt_draw(sel, top);
  }
  opts_save();
  while (joy()) vsync();
  ov_clear();
}

// ---------- PAUSE MENU (START): live colony status + a menu (UP DOWN, A; START or B resumes) ----------
#define PM_N 9
#define PM_MUS 6
#define PM_SPD 7
static const char *const PITEM[PM_N] = {"RESUME", "ANT EYE VIEW", "TRADE FOOD FOR AP", "OPTIONS", "CONTROLS", "HOW TO PLAY", "MUSIC", "SPEED", "QUIT TO MENU"};
static const char *const PHINT[PM_N] = {"BACK TO THE GAME", "SEE THE WORLD AS AN ANT", "2 FOOD BECOMES 4 AP", "RULES  LOOK  SOUND AND MORE", "BUTTONS AND RULES",
  "SEE THE WHOLE WORLD", "MUSIC ON OR OFF", "LEFT RIGHT CHANGES SPEED", "GIVE UP AND LEAVE"};
static u8 pm_do;                                       // what to do after the menu closes: 1 ant eye, 2 trade
static void pm_rule(u8 y) {
  u8 x; for (x = 1; x < 29; x++) BGMAP1[y * 32 + x] = (u16)(ORN | (9 << 12));
  BGMAP1[y * 32 + 14] = (u16)((ORN + 1) | (9 << 12)); BGMAP1[y * 32 + 15] = (u16)((ORN + 2) | (9 << 12));
}
static void pm_time(u8 x, u8 y, u32 sec) { pn(x, y, (u8)(sec / 60), 3); pc(x + 2, y, ':', 3); pz(x + 3, y, (u8)(sec % 60), 3); }
static void pm_draw(void) {                            // everything except the menu rows
  u8 i, n, x, c = 0, g;
  ov_fill(1, 3, 0, 19); ui_full();
  ps(1, 0, "THE COLONY WAITS", 10); ps(sandbox ? 22 : 24, 0, sandbox ? "SANDBOX" : DNAME[diff], 10);
  for (n = 0; n < 6; n++) {
    g = gi("PAUSED"[n]); x = (u8)(12 + n);
    BGMAP1[32 + x] = (u16)((BIGF + g * 2) | (9 << 12)); BGMAP1[64 + x] = (u16)((BIGF + g * 2 + 1) | (9 << 12));
  }
  pm_rule(3);
  ps(1, 4, "AP", 9);      pn(6, 4, mana, 3);          ps(16, 4, "FOOD", 9);   pn(23, 4, stock[0], 3);
  ps(1, 5, "ANTS", 9);     pn(6, 5, ncnt[0], 3);       ps(16, 5, "FOES", 9);   pn(23, 5, ncnt[1], 3);
  ps(1, 6, "POPULAR", 9);  pn(9, 6, appr, 3);          ps(16, 6, "QUEENS", 9);
  pc(23, 6, (char)('0' + qhp[0]), 3); pc(24, 6, '/', 3); pc(25, 6, (char)('0' + qhp[1]), 3);
  ps(1, 7, "ANTLECT", 9); pm_time(10, 7, (u32)(450 - etk) * 2 / 15);   ps(16, 7, "TIME", 9); pm_time(23, 7, (u32)gt * 2 / 15);
  ps(1, 8, "PERKS", 9);
  for (i = 0; i < 6; i++) if ((perk >> i) & 1) {
    for (n = 0; n < 7; n++) pc((u8)(7 + (c % 3) * 8 + n), (u8)(8 + c / 3), PN[i * 7 + n], 3);
    c++;
  }
  if (!c) ps(7, 8, "NONE YET", 10);

}
static const char *const PBTN[PM_N] = {"RESUME", "ANT EYE", "TRADE", "OPTIONS", "HELP", "MAP", "MUSIC", "SPEED", "QUIT"};
static void pm_menu(u8 sel, u8 conf) {                 // a 3 x 3 board of bark buttons (rows 10-18); the chosen one glows gold
  u8 i, x, y, x0, y0, w, pal; u16 t; const char *n;
  for (i = 0; i < PM_N; i++) {
    x0 = (u8)(1 + (i % 3) * 9); w = (u8)(i % 3 == 2 ? 10 : 9); y0 = (u8)(10 + (i / 3) * 3); pal = i == sel ? 6 : 0;
    for (y = y0; y < y0 + 3; y++) for (x = x0; x < x0 + w; x++) {
      if (y == y0) t = x == x0 ? UIB + 4 : x == x0 + w - 1 ? UIB + 6 : UIB + 5;
      else if (y == y0 + 2) t = x == x0 ? UIB + 9 : x == x0 + w - 1 ? UIB + 11 : UIB + 10;
      else t = x == x0 ? UIB + 7 : x == x0 + w - 1 ? UIB + 8 : UIB + 31;
      BGMAP3[y * 32 + x] = (u16)(t | (pal << 12)); BGMAP1[y * 32 + x] = 0;
    }
    n = (conf && i == sel) ? "SURE?" : PBTN[i];
    ps((u8)(x0 + (w - blen2(n)) / 2), (u8)(y0 + 1), n, (conf && i == sel) ? 8 : i == sel ? 9 : 3);
  }
  for (x = 1; x < 30; x++) BGMAP1[19 * 32 + x] = (u16)(1 | (3 << 12));
  if (conf) ps(1, 19, "QUIT THE GAME?  \1 YES  \2 NO", 8);
  else if (sel == PM_MUS) { ps(1, 19, "MUSIC IS", 10); ps(10, 19, nomus ? "OFF" : "ON", 9); ps(15, 19, "\1 SWITCH", 10); }
  else if (sel == PM_SPD) { ps(1, 19, SPNAME[spd], 9); ps(17, 19, spd_ok() ? "\1 UP \3 DOWN" : "\1 CHANGE", 10); }
  else ps(1, 19, PHINT[sel], 10);
}
static void pause_menu(void) {
  u8 sel = 0, conf = 0, run = 1, act, i; u16 k, p, prev = 0;
  hide_all(); vsync(); oam_flush();
  while (joy()) vsync();
  pm_draw(); for (i = 0; i < 16; i++) BGPAL[96 + i] = BGPAL[i]; pm_menu(sel, conf);
  while (run) {
    vsync(); k = joy(); p = k & ~prev; prev = k; hub_glow((u8)frc++);
    if (!p) continue;
    if (conf) {                                        // quit confirmation: A = yes, anything else cancels
      if (p & J_A) { over = 3; tut = 0; break; }
      conf = 0; pm_menu(sel, conf); continue;
    }
    if (p & (J_START | J_B)) break;
    if (p & J_UP)    { sel = (u8)((sel + 6) % 9); sfx_food(); }
    if (p & J_DOWN)  { sel = (u8)((sel + 3) % 9); sfx_food(); }
    if (p & J_LEFT)  { sel = (u8)(sel % 3 ? sel - 1 : sel + 2); sfx_food(); }
    if (p & J_RIGHT) { sel = (u8)(sel % 3 == 2 ? sel - 2 : sel + 1); sfx_food(); }
    act = (u8)((p & J_A) || ((p & J_L) && sel == PM_SPD));
    if (act) {
      switch (sel) {
      case 0: run = 0; break;
      case 1: pm_do = 1; run = 0; break;
      case 2: pm_do = 2; run = 0; break;
      case 3: options_screen(); if (!nomus && !mus_on) music_start(); pm_draw(); for (i = 0; i < 16; i++) BGPAL[96 + i] = BGPAL[i]; prev = joy(); break;
      case 4: help_show(); pm_draw(); prev = joy(); break;
      case 5: map_view(); run = 0; break;
      case PM_MUS: nomus ^= 1; opt[O_MUSIC] = nomus; if (nomus) music_stop(); else music_start(); sfx_mana(); break;
      case PM_SPD: spd_next((p & J_L) ? -1 : 1); sfx_mana(); break;
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
    if (tut > TN) { tut = 0; say("GOOD LUCK ANTPEROR!"); sfx_mana(); break; }
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
static void big(u8 y, const char *t) {             // centred 2x gold title (letters, digits, : ! / only)
  u8 n = blen2(t), x, g, i;
  for (i = 0; i < n; i++) {
    g = gi(t[i]); x = (u8)((30 - n) / 2 + i);
    BGMAP1[y * 32 + x] = (u16)((BIGF + g * 2) | (9 << 12)); BGMAP1[(y + 1) * 32 + x] = (u16)((BIGF + g * 2 + 1) | (9 << 12));
  }
}
static void wait_keys(u16 m) { u16 k; while (joy()) vsync(); do { vsync(); k = joy(); } while (!(k & m)); }
// ---------- PERK CARD: after a good antlection the game stops and you pick one of two perks ----------
static void perk_card(void) {
  u16 k; u8 i, c;
  hide_all(); vsync(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  ps(1, 0, "THE COLONY LOVES YOU", 10);
  big(1, "PICK A PERK");
  pm_rule(3); pm_rule(16);
  for (i = 0; i < 2; i++) {
    c = i ? pb : pa;
    ps(2, (u8)(5 + i * 5), i ? "\2" : "\1", 9); ps(4, (u8)(5 + i * 5), "PRESS", 10);
    for (k = 0; k < 7; k++) pc((u8)(10 + k), (u8)(5 + i * 5), PN[c * 7 + k], 9);
    ps(4, (u8)(6 + i * 5), PD1[c], 3); ps(4, (u8)(7 + i * 5), PD2[c], 3);
  }
  ps(1, 18, "PRESS \1 OR \2 TO CHOOSE", 9);
  while (joy()) vsync();
  do { vsync(); k = joy(); } while (!(k & (J_A | J_B)));
  pick((k & J_A) ? pa : pb);
  while (joy()) vsync();
  ov_clear(); hud();
}
static void end_card(void) {
  u32 s = (u32)gt * 2 / 15; u8 nb = 0, nh = 0; u16 sc = 0;
  hide_all(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  if (over == 1) {
    if (!sandbox && !cheat) {
      if (wins[diff] < 99) wins[diff]++;
      if (!best[diff] || s < best[diff]) { best[diff] = (u16)s; nb = 1; }
      ach_get(0); if (s < 180) ach_get(1); if (diff == 2) ach_get(2); if (mw == 96 && mh == 96) ach_get(4); if (mapt == MINF) ach_get(5); if (!qlost) ach_get(8);
      if (sc = calc_score(s), sc > hiscore[diff]) { hiscore[diff] = sc; nh = 1; }
      save_write();
    }
    if (!sc) sc = calc_score(s);
    big(2, "VICTORY!"); ps(5, 5, timeup ? "TIME UP: YOUR COLONY WINS" : "THE RED QUEEN HAS FALLEN", 3);
  } else { big(2, "DEFEAT"); ps(5, 5, timeup ? "TIME UP: THE RED ANTS WIN" : "YOUR QUEEN HAS FALLEN", 3); }
  pm_rule(7);
  ps(5, 9, "TIME", 9);   pn(16, 9, (u8)(s / 60 > 99 ? 99 : s / 60), 3); pc(18, 9, ':', 3); pz(19, 9, (u8)(s % 60), 3); if (nb) ps(22, 9, "BEST!", 9);
  ps(5, 10, "ANTS LEFT", 9); pn(16, 10, ncnt[0], 3);
  ps(5, 11, "MAP", 9); ps(16, 11, mname(mapt), 3); pn(5, 12, mw, 3); ps(8, 12, "X", 10); pn(10, 12, mh, 3);
  ps(16, 12, DNAME[diff], 3);
  if (over == 1) { ps(5, 13, "SCORE", 9); pd(16, 13, sc, 3); if (nh) ps(22, 13, "HIGH!", 9); }
  pm_rule(15);
  ps(7, 17, "PRESS \5 START", 9);
}
static void play(void) {
  u16 k, prev, p, last = 0; u8 dirs, rep = 0, fire, t = 4, ki = 0, selused = 0, selprev = 0, n, mx, my;
  while (joy()) vsync();
  prev = 0;
  if (tutor) { tut = 1; tut_enter(); prev = joy(); }
  while (!over) {
    vsync(); frc++;
    shx = (shk && !O(O_SHAKE)) ? ((shk & 2) ? 2 : -2) : 0; if (shk) shk--;
    BG0HOFS = BG2HOFS = (u16)(scx + shx); BG0VOFS = BG2VOFS = (u16)scy; oam_flush();
    BGPAL[176 + 3] = (frc & 32) ? RGB(14,22,31) : RGB(10,18,30); BGPAL[176 + 14] = (frc & 32) ? RGB(10,18,30) : RGB(14,22,31);   // water shimmer
    OBJPAL[32 + 3] = RGB(31, 26 + ((frc >> 2) & 3), (frc & 16) ? 4 : 12);                   // the cursor pulses
    for (n = 0; n < 2; n++) { for (mx = wx0; mx < (u8)(wx0 + 31) && mx < W; mx++) draw_dec(mx, (u8)(wy0 + hrow)); hrow = (u8)((hrow + 1) % 19); }   // trails fade in / out
    if (alt) alt--;
    k = joy(); p = k & ~prev; prev = k;
    if (pend && !tut) { perk_card(); prev = joy(); continue; }
    if ((p & J_START) && (k & J_SEL)) { selused = 1; eye(); prev = joy(); continue; }   // SELECT+START: ant eye
    if (p & J_START) {
      pm_do = 0; pause_menu(); hud();
      if (pm_do == 1) eye(); else if (pm_do == 2) { offering(); hud(); }
      prev = joy(); continue;
    }
    dirs = (u8)(k & 15); mx = cx; my = cy;
    if (dirs != last) { rep = 0; last = dirs; }
    if (dirs) {
      fire = (rep == 0) || (rep >= CDLY[O(O_CURSOR)] && ((rep - CDLY[O(O_CURSOR)]) % CRAT[O(O_CURSOR)]) == 0);
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
    if (k & J_SEL) {                                 // SELECT: modifier. SEL+A trade food, SEL+B fast forward, alone (on release) jump
      if (p & J_SEL) selused = 0;
      if (p & J_A) { offering(); selused = 1; }
      if (p & J_B) { spd_toggle(); selused = 1; }
      if (dirs) selused = 1;
    } else {
      if (selprev && !selused) jump();
      if (p & J_A) raise_land();
      if (p & J_B) lower_land();
    }
    selprev = (u8)(k & J_SEL);
    if (p & J_L) flood();                            // GBA shoulder shortcuts
    if (p & J_R) { if (spd == 1) { for (n = 8; n; n--) tick_slice(); sfx_food(); } else spd_toggle(); }
    if (tut && (cx != lcx || cy != lcy)) { tev |= 1; lcx = cx; lcy = cy; }
    if (tut && (tev & TEV[tut - 1])) { sfx_mana(); tut++; tut_enter(); prev = joy(); }
    if (tut) { qhp[0] = qhp[1] = qbase; if (appr < 50) appr = 50; }          // no game over mid-lesson
    if (cheat) { mana = MANA_MAX; stock[0] = 99; appr = 99; }
    if (cht[0]) mana = MANA_MAX;
    if (cht[1] && qhp[0] < qmax0) qhp[0] = qmax0;
    party_tick();
    if (!(frc & 31) && !sandbox && !cheat && !tut) { if (appr >= 99) ach_get(9); if (ncnt[0] >= 24) ach_get(10); }
    if (sandbox) { mana = MANA_MAX; qhp[0] = qhp[1] = qbase; appr = 99; }
    follow(); scroll_step(); win_sync();
    bfs_tick(450);                                   // paths update in the background after the land changes
    n = SPN[spd]; if (n && (sf++ & SPM[spd])) n = 0;     // PAUSED / FRAME ADVANCE: none; slow speeds skip frames
    for (; n; n--) tick_slice();
    if (++t >= 8 || p) {
      t = 0; count(ncnt);
      if (tut && !msgt && THINT[tut - 1]) { msg = THINT[tut - 1]; msgt = 1; }   // keep the lesson goal on the HUD
      hud();
    } else if ((mx != cx || my != cy) && !msgt) { ov_fill(1, 1, 19, 19); cur_info(); }
    if (!qhp[1]) over = 1; else if (!qhp[0]) over = 2;
    if (O(O_TIME) && !tut && !sandbox && !over && (u32)gt * 2 / 15 >= (O(O_TIME) == 1 ? 300u : O(O_TIME) == 2 ? 600u : 1200u)) {
      timeup = 1; over = (u8)(ncnt[0] + qhp[0] * 3 >= ncnt[1] + qhp[1] * 3 ? 1 : 2);   // time is up: more ants (and hearts) wins
    }
    draw_sprites();
  }
  music_stop();
  if (over == 3) { fade(0, 16); return; }              // quit from the pause menu: straight back to the title
  for (n = 0; n < 60; n++) { vsync(); frc++; draw_sprites(); oam_flush(); }   // a moment to see the queen fall
  end_card();
  if (over == 1) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  vsync(); oam_flush();
  wait_keys(J_START);
  while (joy()) vsync();
  fade(0, 16);
}

static void gfx_init(void) {                          // palettes, tiles, maps, layers (everything main() needs before the first screen)
  u8 i, x, y; u16 k; static const u8 BARRM[4] = {1, 2, 2, 3};
  for (i = 0; i < 255; i++) { BGPAL[i] = 0; OBJPAL[i] = 0; }
  SNDX = 0x80; SNDH = 0x0002; SNDL = 0xFF77;          // PSG on, 100% DMG volume, all channels both sides
  BLDCNT = 0xFF; BLDY = 16;
  // palettes
  BGPAL[0] = RGB(8,16,28); BGPAL[1] = RGB(28,24,14); BGPAL[2] = RGB(8,20,6); BGPAL[3] = RGB(6,4,3);
  BGPAL[17] = RGB(0,0,0); BGPAL[18] = RGB(20,20,20); BGPAL[19] = RGB(31,31,31);
  BGPAL[33] = RGB(0,0,0); BGPAL[34] = RGB(22,12,3);  BGPAL[35] = RGB(31,27,8);
  BGPAL[49] = RGB(1,1,5); BGPAL[50] = RGB(11,11,22); BGPAL[51] = RGB(31,29,22);
  BGPAL[67] = RGB(14,8,2);
  // tiles: 0 blank, 1 solid colour 1, 2 solid colour 3, 3 star, 4.. font, 44.. terrain + mana bar
  fill_tile(TILE32, 0); fill_tile(TILE32 + 8, 0); fill_tile(TILE32 + 16, 0x33333333u);   // tile 1 = "blank" (see-through now)
  fill_tile(TILE32 + 24, 0);
  for (k = 0; k < 32 * 8; k++) TILE32[UIB * 8 + k] = UIT[k];                          // termite UI tiles
  for (i = 0; i < NGLYPH; i++) mk_glyph(TILE32 + (FT + i) * 8, FONT8[i]);
  for (i = 0; i < 6; i++) put_tile(TILE32 + (BT + i) * 8, BGT + i * 16, 0);         // old terrain: the title's grass and sand
  (void)BARRM;
  for (y = 0; y < 8; y++) {                                                            // AP bar segments (palette bank 14)
    TILE32[BAR_F * 8 + y] = (y == 0 || y == 7) ? 0x11111111u : (y == 1 || y == 6) ? 0x12222221u : 0x13333331u;
    TILE32[BAR_E * 8 + y] = (y == 0 || y == 7) ? 0x11111111u : (y == 1 || y == 6) ? 0x12222221u : 0x12111121u;
  }
  {                                                                                    // button chips
    static const u8 CIR[8] = {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C}, PIL[8] = {0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E};
    static const char *const CL[6] = {"A", "B", "L", "R", "ST", "SE"};
    static const u8 CC[6] = {2, 4, 7, 7, 8, 9};
    u8 c, yy, xx, n, g; u32 w;
    for (c = 0; c < 6; c++) for (yy = 0; yy < 8; yy++) {
      w = 0; n = CL[c][1] ? 2 : 1;
      for (xx = 0; xx < 8; xx++) {
        u8 v = (((n == 2 ? PIL : CIR)[yy] >> (7 - xx)) & 1) ? CC[c] : 0;
        for (g = 0; g < n; g++) {
          s8 lx = (s8)xx - (n == 2 ? (g ? 4 : 0) : 2) - (n == 2 ? 1 : 0), ly = (s8)yy - 2;
          if (lx >= 0 && lx < 3 && ly >= 0 && ly < 5 && ((FONT[gi(CL[c][g])] >> (3 * (4 - ly))) & (4 >> lx))) v = 5;
        }
        w |= (u32)v << (4 * xx);
      }
      TILE32[(CHIP + c) * 8 + yy] = w;
    }
    BGPAL[65] = RGB(2,3,8); BGPAL[66] = RGB(4,20,6); BGPAL[68] = RGB(24,4,4); BGPAL[69] = RGB(31,31,31);
    BGPAL[71] = RGB(13,13,16); BGPAL[72] = RGB(26,20,2); BGPAL[73] = RGB(8,11,20);
  }
  for (i = 0; i < 16; i++) TILE2[i] = 0;                                               // charblock 2 tile 0/1: transparent
  for (k = 0; k < 4 * 81 * 8; k++) TILE2[TB * 8 + k] = TERR[k];                        // terrain auto-tiles
  for (k = 0; k < 13 * 8; k++) TILE2[DEC * 8 + k] = DECO[k];                           // food, trails, nest mound
  for (k = 0; k < 24 * 8; k++) OBJT32[k] = SPRT[k];                                    // sprites
  for (k = 0; k < 64 * 8; k++) OBJT32[BIGT * 8 + k] = BIGS[k];                         // ANT EYE big sprites
  for (k = 0; k < 64 * 8; k++) OBJT32[ICNT_T * 8 + k] = ICNT[k];                       // home screen icons
  for (i = 0; i < 16; i++) {
    BGPAL[176 + i] = PAL_TERR[i]; BGPAL[192 + i] = PAL_DEC[i];
    OBJPAL[i] = PAL_ANT0[i]; OBJPAL[16 + i] = PAL_ANT1[i]; OBJPAL[48 + i] = PAL_FX[i]; OBJPAL[96 + i] = PAL_DEC[i]; OBJPAL[64 + i] = RGB(31, 31, 31);
  }
  OBJPAL[33] = RGB(2,2,2); OBJPAL[35] = RGB(31,28,4);                                  // bank 2: yellow cursor
  OBJPAL[81] = RGB(2,2,2); OBJPAL[83] = RGB(31,6,4);                                   // bank 5: red cursor (cannot edit here)
  BGPAL[17] = BGPAL[33] = RGB(2,3,8);                                                  // panels: dark navy, not black
  BGPAL[129] = RGB(2,3,8); BGPAL[131] = RGB(31,7,7);                                   // text bank 8: red
  BGPAL[209] = RGB(2,3,8); BGPAL[211] = RGB(10,28,8);                                  // text bank 13: green
  BGPAL[225] = RGB(2,3,8); BGPAL[226] = RGB(5,9,12); BGPAL[227] = RGB(10,26,31);       // text bank 14: cyan (+ bar)
  BGPAL[241] = RGB(2,3,8); BGPAL[243] = RGB(9,10,14);                                  // text bank 15: dim
  for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) { BGMAP0[y * 32 + x] = TB | (11 << 12); BGMAP1[y * 32 + x] = 0; BGMAP2[y * 32 + x] = 0; }
  BGPAL[81] = RGB(10,18,28); view_init();
  card_init();
  BG0CNT = 2 | (2 << 2) | (28 << 8);                  // terrain: priority 2, charblock 2, screenblock 28
  BG2CNT = 1 | (2 << 2) | (30 << 8);                  // decor: priority 1, charblock 2, screenblock 30
  BG3CNT_ = 0 | (31 << 8);                            // termite UI: priority 0 (under the text), charblock 0, screenblock 31
  for (k = 0; k < 1024; k++) BGMAP3[k] = 0;
  BG1CNT = 0 | (29 << 8);                             // priority 0
  hide_all(); oam_flush();
}

// ---------- MAP EDITOR (title screen: tap SELECT). Paint a map, it is saved on the cartridge as MY MAP ----------
#define ET_N 7
static const char *const ETN[ET_N] = {"WATER", "SAND", "GRASS", "HILL", "BERRIES", "YOUR NEST", "RED NEST"};
static u8 etool = 3, ebr = 1;
static void ed_bar(void) {
  static const u8 SWT[ET_N] = {2, 2, 2, 2, 0, 0, 0}, SWP[ET_N] = {14, 2, 13, 4, 8, 1, 8};
  static const char SWC[ET_N] = {0, 0, 0, 0, '%', '$', '@'};
  u8 i, x;
  ov_fill(1, 1, 17, 19);
  ps(0, 17, "\1 PAINT \3\4 TOOL \5 MENU", 1); pn(24, 17, mw, 10); pc(26, 17, 'X', 10); pn(27, 17, mh, 10);
  for (i = 0; i < ET_N; i++) {
    x = (u8)(1 + i * 4);
    pc((u8)(x - 1), 18, i == etool ? '>' : ' ', 2);
    if (SWC[i]) { pc(x, 18, SWC[i], SWP[i]); pc((u8)(x + 1), 18, SWC[i], SWP[i]); }
    else { BGMAP1[18 * 32 + x] = BGMAP1[18 * 32 + x + 1] = (u16)(SWT[i] | (SWP[i] << 12)); }
  }
  ps(0, 19, ETN[etool], 2); ps(11, 19, "\6 BRUSH", 1); pc(19, 19, (char)('0' + ebr), 1); ps(23, 19, "\2 PICK", 1);
}
static void ed_area(s8 x0, s8 y0, s8 r) {          // redraw cells around a spot (edges and nest mounds change)
  s8 x, y;
  for (y = (s8)(y0 - r); y <= y0 + r; y++) for (x = (s8)(x0 - r); x <= x0 + r; x++) if (x >= 0 && y >= 0 && x < W && y < H) draw_cell((u8)x, (u8)y);
}
static void ed_put(s8 x, s8 y) {                    // the current tool on one cell
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  if (etool < 4) {
    if (is_nest((u8)x, (u8)y) && etool == 0) return;   // a nest never sits in water
    hgt[y][x] = etool; food[y][x] = 0;
  } else if (etool == 4) { if (!hgt[y][x]) hgt[y][x] = 2; if (!is_nest((u8)x, (u8)y)) food[y][x] = 1; }
}
static void ed_paint(void) {
  s8 r = (s8)(ebr >> 1), dx, dy; u8 t, ox, oy;
  if (etool >= 5) {                                 // move a nest
    t = (u8)(etool - 5);
    if (cx == nestx[t ^ 1] && cy == nesty[t ^ 1]) return;
    ox = nestx[t]; oy = nesty[t]; nestx[t] = cx; nesty[t] = cy;
    if (!hgt[cy][cx]) hgt[cy][cx] = 2;
    food[cy][cx] = 0;
    ed_area((s8)ox, (s8)oy, 2); ed_area((s8)cx, (s8)cy, 2);
    return;
  }
  for (dy = -r; dy <= r; dy++) for (dx = -r; dx <= r; dx++) ed_put((s8)(cx + dx), (s8)(cy + dy));
  ed_area((s8)cx, (s8)cy, (s8)(r + 1));
}
static void ed_pick(void) {
  etool = (cx == nestx[0] && cy == nesty[0]) ? 5 : (cx == nestx[1] && cy == nesty[1]) ? 6 : food[cy][cx] ? 4 : hgt[cy][cx];
}
static void ed_store(void) {                        // the world -> MY MAP letters -> cartridge
  static const char LC[4] = {'w', 's', 'g', 'h'};
  u8 x, y; char c;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    c = LC[hgt[y][x]];
    if (food[y][x] && hgt[y][x]) c = 'f';
    if (x == nestx[0] && y == nesty[0]) c = 'Y';
    if (x == nestx[1] && y == nesty[1]) c = 'R';
    cmap[y * W + x] = c;
  }
  myhm.w = mw; myhm.h = mh; my_save(); ach_get(6);
}
static void ed_resize(u8 nw, u8 nh) {
  u8 x, y, i;
  for (y = 0; y < nh; y++) for (x = 0; x < nw; x++) if (x >= mw || y >= mh) { hgt[y][x] = 2; food[y][x] = 0; ph[y][x] = 0; }
  mw = nw; mh = nh;
  for (i = 0; i < 2; i++) { if (nestx[i] >= W) nestx[i] = (u8)(W - 5); if (nesty[i] >= H) nesty[i] = (u8)(H - 5); if (!hgt[nesty[i]][nestx[i]]) hgt[nesty[i]][nestx[i]] = 2; }
  if (cx >= W) cx = (u8)(W - 1); if (cy >= H) cy = (u8)(H - 1);
}
#define EM_N 7
static const char *const EITEM[EM_N] = {"KEEP EDITING", "SAVE AND PLAY", "SAVE AND QUIT", "WIDTH", "HEIGHT", "CLEAR TO GRASS", "QUIT WITHOUT SAVING"};
static void em_draw(u8 sel, u8 conf) {
  u8 i, y, x, n = 0, ok;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) n += (u8)(food[y][x] && hgt[y][x] && n < 250);
  bfs(0); ok = dnp[0][nesty[1]][nestx[1]] != NOPATH;
  ov_fill(1, 3, 0, 19); ui_full();
  big(1, "MAP EDITOR");
  pm_rule(3);
  ps(2, 4, "SIZE", 9); pn(11, 4, mw, 3); ps(14, 4, "X", 10); pn(16, 4, mh, 3);
  ps(2, 5, "BERRIES", 9); pn(11, 5, n, 3); if (n > 16) ps(14, 5, "GAME USES 16", 10);
  ps(2, 6, "PATH", 9); ps(11, 6, ok ? "ANTS CAN WALK" : "NONE: GAME CUTS ONE", ok ? 13 : 8);
  ps(11, 7, "NEST TO NEST", 10);
  pm_rule(9);
  for (i = 0; i < EM_N; i++) {
    y = (u8)(10 + i);
    if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
    ps(3, y, (conf && i == sel) ? "SURE? \1 YES  \2 NO" : EITEM[i], i == sel ? 9 : 3);
    if (i == 3) { pc(18, y, '<', 10); pn(20, y, mw, 9); pc(23, y, '>', 10); }
    if (i == 4) { pc(18, y, '<', 10); pn(20, y, mh, 9); pc(23, y, '>', 10); }
  }
  ps(1, 18, sel == 3 || sel == 4 ? "LEFT RIGHT: CHANGE SIZE" : "\1 CHOOSE   \5 BACK", 10);
}
static u8 ed_menu(void) {                           // 0 keep editing, 1 save + play, 2 save + quit, 3 quit
  u8 sel = 0, conf = 0, r = 255; u16 k, p, prev;
  hide_all(); vsync(); oam_flush();
  while (joy()) vsync();
  em_draw(sel, conf); prev = 0;
  while (r == 255) {
    vsync(); k = joy(); p = k & ~prev; prev = k;
    if (!p) continue;
    if (conf) { if (p & J_A) r = 3; else { conf = 0; em_draw(sel, conf); } continue; }
    if (p & (J_START | J_B)) { r = 0; break; }
    if (p & J_UP)   { sel = sel ? sel - 1 : EM_N - 1; sfx_food(); }
    if (p & J_DOWN) { sel = sel == EM_N - 1 ? 0 : sel + 1; sfx_food(); }
    if ((sel == 3 || sel == 4) && (p & (J_LEFT | J_RIGHT))) {
      s8 d = (p & J_RIGHT) ? 8 : -8; u8 v = (u8)((sel == 3 ? mw : mh) + d);
      if (v >= 32 && v <= 96) { ed_resize(sel == 3 ? v : mw, sel == 4 ? v : mh); sfx_mana(); }
    }
    if (p & J_A) {
      if (sel == 0) r = 0; else if (sel == 1) r = 1; else if (sel == 2) r = 2;
      else if (sel == 5) { u8 x, y; for (y = 0; y < H; y++) for (x = 0; x < W; x++) { hgt[y][x] = 2; food[y][x] = 0; } sfx_flood(); }
      else if (sel == 6) { conf = 1; sfx_deny(); }
    }
    em_draw(sel, conf);
  }
  while (joy()) vsync();
  return r;
}
static u8 editor(void) {                            // returns 1 = play MY MAP now
  u16 k, p, prev = 0, last = 0; u8 dirs, rep = 0, fire, mx, my, i, res = 255, r;
  DISPCNT = 0x80;
  if (mapt == MMY && !has_my) {                     // a fresh map: grass, with the two nests
    mw = tw; mh = th;
    for (my = 0; my < H; my++) for (mx = 0; mx < W; mx++) { hgt[my][mx] = 2; food[my][mx] = 0; ph[my][mx] = 0; }
    nestx[0] = 4; nesty[0] = (u8)(H - 6); nestx[1] = (u8)(W - 5); nesty[1] = 5; npatch = 0;
  } else {                                          // start from the map chosen on the title (or MY MAP itself)
    newgame_gen_only = 1; gen_world(); newgame_gen_only = 0;
    for (my = 0; my < H; my++) for (mx = 0; mx < W; mx++) { food[my][mx] = 0; ph[my][mx] = 0; }
    for (i = 0; i < npatch; i++) if (pfx[i] < W && pfy[i] < H && hgt[pfy[i]][pfx[i]]) food[pfy[i]][pfx[i]] = 1;
  }
  for (i = 0; i < MAXA; i++) ant[i].alive = 0;
  cx = (u8)(nestx[0] + 2); cy = (u8)(nesty[0] - 2); if (cx >= W) cx = (u8)(W - 1);
  camx = 0; camy = (u8)(H - VH); follow(); scx = (s16)camx * 8; scy = (s16)camy * 8;
  win_all(); ov_clear(); ed_bar();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; BG2CNT = 1 | (2 << 2) | (30 << 8);
  hide_all(); place16(MAXA + 2, 20, 2, cx * 8, cy * 8, 0); oam_flush();
  DISPCNT = 0x1F40; BLDY = 16;
  music_start(); fade(16, 0);
  while (joy()) vsync();
  while (res == 255) {
    vsync(); frc++;
    BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; oam_flush();
    BGPAL[176 + 3] = (frc & 32) ? RGB(14,22,31) : RGB(10,18,30); BGPAL[176 + 14] = (frc & 32) ? RGB(10,18,30) : RGB(14,22,31);
    OBJPAL[32 + 3] = RGB(31, 26 + ((frc >> 2) & 3), (frc & 16) ? 4 : 12);
    k = joy(); p = k & ~prev; prev = k;
    dirs = (u8)(k & 15); mx = cx; my = cy;
    if (dirs != last) { rep = 0; last = dirs; }
    if (dirs) {
      fire = (rep == 0) || (rep >= CDLY[O(O_CURSOR)] && ((rep - CDLY[O(O_CURSOR)]) % CRAT[O(O_CURSOR)]) == 0);
      if (rep < 250) rep++;
      if (fire) {
        if ((dirs & J_LEFT) && cx > 0) cx--;
        if ((dirs & J_RIGHT) && cx < W - 1) cx++;
        if ((dirs & J_UP) && cy > 0) cy--;
        if ((dirs & J_DOWN) && cy < H - 1) cy++;
      }
    }
    if (p & J_L) { etool = (u8)((etool + ET_N - 1) % ET_N); sfx_food(); ed_bar(); }
    if (p & J_R) { etool = (u8)((etool + 1) % ET_N); sfx_food(); ed_bar(); }
    if (p & J_SEL) { ebr = ebr == 1 ? 3 : ebr == 3 ? 5 : 1; sfx_food(); ed_bar(); }
    if (p & J_B) { ed_pick(); sfx_spawn(); ed_bar(); }
    if ((k & J_A) && ((p & J_A) || mx != cx || my != cy)) { ed_paint(); if (p & J_A) sfx_raise(); }
    if (p & J_START) {
      r = ed_menu();
      if (r == 0) { ov_clear(); ed_bar(); follow(); scx = (s16)camx * 8; scy = (s16)camy * 8; win_all(); prev = joy(); continue; }
      if (r == 1 || r == 2) ed_store();
      res = r;
    }
    follow(); scroll_step(); win_sync();
    place16(MAXA + 2, 20, 2, cx * 8, cy * 8, 0);
  }
  music_stop(); fade(0, 16); hide_all(); oam_flush(); ov_clear();
  if (res == 1) { mapt = MMY; return 1; }
  if (res == 2) mapt = MMY;
  return 0;
}

#include "extras.c"
#include "empire.c"

int main(void) {
  DISPCNT = 0x80;
  IO16(0x204) = 0x4317;                               // WAITCNT: faster cartridge reads + prefetch (standard on GBA games)
  logo_play();                                        // Danny Steel boot logo (START skips)
  IO16(0x0C) = IO16(0x0E) = 0; IO16(0x14) = IO16(0x16) = 0; IO16(0x48) = IO16(0x4A) = 0;   // undo the logo's BG2/3, BG1 scroll, windows
  gfx_init();
  save_load(); ach_load(); opts_load(); opts_apply(); vram_reload(); opts_apply();
  diff = 1;
  for (;;) {
    u8 r = title();
    if (r == 2) { empire_main(); continue; }
    if (r == 3) { extras(); continue; }
    if (r == 1) { if (editor()) { newgame(); play(); } continue; }
    newgame(); play();
  }
}
