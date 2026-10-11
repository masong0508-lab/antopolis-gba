// ============================================================================================================================
//  EMPIRE MODE (included from main.c). Civilization + Age of Empires + Tropico + SimCity, played on the normal map screen.
//  Build a city cell by cell, paint zones that grow by themselves, research 12 techs through 4 ages, keep 4 factions happy
//  (edicts, elections), train armies of up to 500 ants and fight rival colonies on the map. L zooms a fight into a pixel battle.
//  STORY: 7 chapters with Counselor Chitin.  FREE PLAY: 0-3 rivals, any map, 3 ways to win, save + continue.
// ============================================================================================================================
#define EMT 512                                    // building tiles in charblock 2 (tile 512 = 0x0600C000, screenblocks 24-25)
#define ARMT_T 88                                  // army banner sprite tiles
static u8 eshell, earm, ebat_pal; static const char *ebat_name = "RED";
static void vram_reload(void);
#include "battle.c"

// ---------- techs ----------
enum { T_FUNGUS, T_DIG, T_BRICK, T_SOLD, T_SCENT, T_BARTER, T_SPORE, T_AMBER, T_PIONEER, T_SHELL, T_GENES, T_GREAT, T_N };
static const char *const TNAME[T_N] = {"FUNGUS FARMING", "DIGGING", "MUD BRICKS", "SOLDIER CASTE", "SCENT TRAILS", "BARTER", "SPORE RITES",
  "AMBER WORKING", "PIONEERS", "HARD SHELLS", "APHID HERDING", "GREAT MOUNDS"};
static const u16 TREQ[T_N] = {0, 0, 1 << T_DIG, 1 << T_DIG, 1 << T_FUNGUS, 1 << T_SCENT, 1 << T_FUNGUS, (1 << T_BRICK) | (1 << T_SCENT),
  1 << T_BARTER, (1 << T_SOLD) | (1 << T_AMBER), 1 << T_AMBER, (1 << T_PIONEER) | (1 << T_SHELL) | (1 << T_GENES)};
static const char *const TDESC[T_N] = {"FARMS  GRANARY  GARDEN", "PITS  CHAMBERS  NURSERY", "TOWER WALL SPIKES STATUE BRIDGE",
  "BARRACKS  ARMORY", "ARCHIVE: RESEARCH", "MARKET  TRADE  DANCE HALL", "TEMPLE  HEALER", "MINES SPITTERS ACADEMY",
  "OUTPOST VAULT PALACE EMBASSY", "MAJORS + TOUGHER SOLDIERS", "APHID RANCH  +1 FARM FOOD", "THE GREAT MOUND WONDER"};
static const char *const ERAN[4] = {"DIRT AGE", "LEAF AGE", "AMBER AGE", "CRYSTAL AGE"};

// ---------- buildings ----------
enum { B_NONE, B_NEST, B_FARM, B_PIT, B_HOUSE, B_BARR, B_TOWER, B_ARCH, B_MARKET, B_TEMPLE, B_MINE, B_OUTPOST, B_WONDER, B_APHID,
       B_GRAN, B_WALL, B_SPIKE, B_NURS, B_HEAL, B_ARMORY, B_SPIT, B_GARDEN, B_DANCE, B_STATUE, B_ACAD, B_VAULT, B_PALACE, B_EMBASSY,
       B_BRIDGE, B_TRADE, B_N };
typedef struct { const char *nm, *d1, *d2; u8 f, d, a, jobs, hp, tech, ter, tile; } BDef;   // ter: 1 water 2 sand 4 grass 8 hill
static const BDef BD[B_N] = {
  {"", "", "", 0, 0, 0, 0, 0, 0, 0, 0},
  {"NEST", "YOUR QUEEN LIVES HERE", "LOSE IT AND YOU LOSE", 0, 0, 0, 0, 250, T_DIG, 14, 0},
  {"FUNGUS FARM", "+3 FOOD  +1 BY WATER", "NEEDS 2 WORKERS", 0, 10, 0, 2, 12, T_FUNGUS, 6, 1},
  {"DIRT PIT", "+3 DIRT", "ON SAND OR HILLS", 5, 0, 0, 2, 12, T_DIG, 10, 2},
  {"CHAMBERS", "+5 ROOM FOR ANTS", "NO WORKERS NEEDED", 0, 12, 0, 0, 15, T_DIG, 14, 3},
  {"BARRACKS", "TRAINS SOLDIERS", "\1 ON IT TO TRAIN", 10, 20, 0, 1, 20, T_SOLD, 14, 4},
  {"TOWER", "SHOOTS ENEMY ARMIES", "WITHIN 3 SQUARES", 0, 20, 0, 1, 25, T_BRICK, 14, 5},
  {"ARCHIVE", "+2 RESEARCH", "NEEDS 2 WORKERS", 10, 20, 0, 2, 12, T_SCENT, 14, 6},
  {"MARKET", "+1 AMBER", "BUILDERS LIKE IT", 0, 25, 0, 1, 12, T_BARTER, 14, 7},
  {"SPORE TEMPLE", "MYSTICS LOVE IT", "EVERYONE CALMER", 0, 20, 10, 1, 15, T_SPORE, 14, 8},
  {"AMBER MINE", "+2 AMBER", "ON HILLS ONLY", 0, 15, 0, 2, 15, T_AMBER, 8, 9},
  {"OUTPOST", "GROWS YOUR LAND", "BUILD AT THE EDGE", 10, 20, 0, 0, 15, T_PIONEER, 14, 10},
  {"GREAT MOUND", "THE WONDER", "4 WORKERS FINISH IT: WIN", 100, 150, 80, 4, 80, T_GREAT, 14, 12},
  {"APHID RANCH", "+5 FOOD", "ON GRASS ONLY", 0, 15, 0, 2, 12, T_GENES, 4, 16},
  {"GRANARY", "ANTS EAT LESS", "-15 PERCENT FOOD USE", 0, 20, 0, 0, 15, T_FUNGUS, 14, 17},
  {"WALL", "ENEMY ARMIES MUST", "BREAK THROUGH IT", 0, 6, 0, 0, 30, T_BRICK, 14, 18},
  {"SPIKE PIT", "HURTS ARMIES THAT", "WALK OVER IT", 0, 10, 0, 0, 20, T_BRICK, 14, 19},
  {"NURSERY", "THE COLONY", "GROWS FASTER", 15, 15, 0, 1, 12, T_DIG, 14, 20},
  {"HEALER", "ARMIES NEARBY", "GET TROOPS BACK", 10, 15, 5, 1, 12, T_SPORE, 14, 21},
  {"ARMORY", "YOUR SOLDIERS", "HIT HARDER", 0, 25, 10, 1, 15, T_SOLD, 14, 22},
  {"SPITTER NEST", "TRAIN SPITTERS", "AND SHOOT 5 SQUARES", 0, 20, 15, 1, 20, T_AMBER, 14, 23},
  {"GARDEN", "FARMERS AND BUILDERS", "LIKE IT", 0, 5, 0, 0, 8, T_FUNGUS, 6, 24},
  {"DANCE HALL", "EVERYONE", "IS HAPPIER", 0, 20, 5, 1, 12, T_BARTER, 14, 25},
  {"STATUE", "WARRIORS ARE PROUD", "EVERYONE +A LITTLE", 0, 15, 10, 0, 20, T_BRICK, 14, 26},
  {"ACADEMY", "+4 RESEARCH", "NEEDS 2 WORKERS", 20, 30, 10, 2, 15, T_AMBER, 14, 27},
  {"AMBER VAULT", "STORED AMBER", "GROWS INTEREST", 0, 30, 10, 1, 25, T_PIONEER, 14, 28},
  {"PALACE", "+10 IN ELECTIONS", "EVERYONE +A LITTLE", 30, 40, 30, 1, 30, T_PIONEER, 14, 29},
  {"EMBASSY", "RIVALS ATTACK", "LESS OFTEN", 0, 30, 20, 1, 15, T_PIONEER, 14, 30},
  {"BRIDGE", "BUILD ON WATER", "ANTS CAN CROSS", 0, 10, 0, 0, 20, T_BRICK, 1, 31},
  {"TRADE POST", "5 FOOD BECOMES", "2 AMBER", 0, 20, 0, 1, 12, T_BARTER, 14, 32},
};
// land tools in the build menu
enum { L_PILE = 100, L_DIG, L_ZH, L_ZF, L_ZW, L_ZX };
#define NTAB 6
static const char *const TABN[NTAB] = {"FOOD + HOMES", "WORK", "ARMY", "HAPPY", "SPECIAL", "LAND + ZONES"};
static const u8 TABS[NTAB][8] = {
  {B_FARM, B_APHID, B_GRAN, B_HOUSE, B_NURS, 0},
  {B_PIT, B_MINE, B_MARKET, B_TRADE, B_VAULT, B_ARCH, B_ACAD, 0},
  {B_BARR, B_ARMORY, B_SPIT, B_TOWER, B_WALL, B_SPIKE, B_HEAL, 0},
  {B_TEMPLE, B_GARDEN, B_DANCE, B_STATUE, B_PALACE, B_EMBASSY, 0},
  {B_OUTPOST, B_BRIDGE, B_WONDER, 0},
  {L_PILE, L_DIG, L_ZH, L_ZF, L_ZW, L_ZX, 0}};
static const char *const LTN[6] = {"PILE UP LAND", "DIG DOWN LAND", "ZONE: HOMES", "ZONE: FARMS", "ZONE: WORK", "REMOVE ZONE"};
static const char *const LTD[6] = {"COSTS 5 DIRT", "GIVES 2 DIRT", "CHAMBERS GROW HERE", "FARMS GROW HERE", "PITS AND MINES GROW", "PAINT TO CLEAR"};

// ---------- edicts (Tropico) ----------
#define NED 8
static const char *const EDN[NED] = {"FEAST", "OVERTIME", "MARTIAL LAW", "SPORE FESTIVAL", "CONSCRIPTION", "RIG ELECTION", "TAX BREAK", "PROPAGANDA"};
static const char *const EDC[NED] = {"%30", "FREE", "]10", "]15", "%10", "]30", "FREE", "%20 [10"};
static const char *const EDD[NED] = {"FARMERS +15  ALL +5", "WORK +50 PERCENT  UNHAPPY", "NO PROTESTS  ALL -10", "MYSTICS +20  ALL +5",
  "+30 WARRIORS NOW", "WIN THE NEXT VOTE  ALL -5", "WORK -25 PERCENT  ALL HAPPY", "EVERYONE +8 NOW"};
static const u8 EDF[NED] = {30, 0, 0, 0, 10, 0, 0, 20}, EDA[NED] = {0, 0, 10, 15, 0, 30, 0, 0}, EDDI[NED] = {0, 0, 0, 0, 0, 0, 0, 10};
static const char *const FACN[4] = {"FARMERS", "BUILDERS", "WARRIORS", "MYSTICS"};
static const char *const FACW[4] = {"WANT: FOOD  FARMS", "WANT: HOMES  JOBS", "WANT: ARMY  TOWERS", "WANT: TEMPLES  FUN"};

// ---------- story ----------
enum { G_GROW, G_MARKET, G_SURVIVE, G_VOTE, G_AMBER, G_CONQUER, G_WONDER };
typedef struct { const char *name; u8 map, w, h, nriv, aggr; u16 techs, f, d, a; u8 goal; const char *goal_txt; const char *s1[6], *s2[6]; const char *outro; } Chap;
#define NCH 7
static const Chap CH[NCH] = {
  {"THE FIRST CHAMBER", 0, 40, 32, 0, 0, (1 << T_FUNGUS) | (1 << T_DIG), 30, 40, 0, G_GROW, "12 ANTS  2 FARMS  1 CHAMBERS",
   {"ANTPEROR! I AM CHITIN, YOUR", "HUMBLE COUNSELOR. THE OLD", "NEST FLOODED. ONLY YOU, THE", "QUEEN AND A FEW WORKERS", "MADE IT TO THIS MEADOW.", ""},
   {"PRESS \1 ON EMPTY GROUND TO", "BUILD. FARMS FEED US.", "CHAMBERS GIVE US ROOM.", "OR PAINT ZONES AND LET THE", "COLONY BUILD BY ITSELF!", ""},
   "A REAL COLONY AT LAST!"},
  {"THE DRY SEASON", 2, 48, 40, 0, 0, (1 << T_FUNGUS) | (1 << T_DIG) | (1 << T_SCENT), 20, 40, 0, G_MARKET, "BUILD A MARKET  60 FOOD",
   {"THE RAIN HAS STOPPED.", "THE FARMS GROW SLOWLY NOW.", "THE BEETLE TRADERS WILL", "ONLY TALK TO A COLONY", "WITH A MARKET.", ""},
   {"OPEN \5 MENU: RESEARCH.", "LEARN BARTER, THEN BUILD", "A MARKET. KEEP 60 FOOD", "IN STORE FOR THE TRADERS.", "", ""},
   "THE TRADERS ARRIVE. WE ARE RICH!"},
  {"RED ON THE HORIZON", 1, 48, 48, 1, 0, (1 << T_FUNGUS) | (1 << T_DIG) | (1 << T_BRICK) | (1 << T_SOLD), 40, 80, 10, G_SURVIVE, "STOP 5 RED ARMIES",
   {"SCOUTS SAW RED ANTS ACROSS", "THE RIVER. THEY WANT OUR", "FARMS. THEY WILL COME IN", "WAVES, EACH ONE BIGGER.", "", ""},
   {"BUILD BARRACKS AND TRAIN", "SOLDIERS. TOWERS AND WALLS", "HELP. WHEN ARMIES MEET THEY", "FIGHT. PRESS \3 TO ZOOM IN", "AND WATCH THE BATTLE!", ""},
   "THE RED ANTS RUN HOME!"},
  {"VOICES OF THE COLONY", 0, 48, 48, 0, 0, (1 << T_FUNGUS) | (1 << T_DIG) | (1 << T_SCENT) | (1 << T_SPORE) | (1 << T_BRICK), 60, 60, 20, G_VOTE, "WIN A VOTE  ALL GROUPS 45+",
   {"THE FARMERS GRUMBLE. THE", "BUILDERS WANT HOMES. THE", "MYSTICS WANT TEMPLES. THE", "WARRIORS WANT GLORY.", "AND A VOTE IS COMING...", ""},
   {"SEE \5 MENU: GROUPS TO LEARN", "WHAT EACH ONE WANTS. USE", "EDICTS: FEASTS, FESTIVALS,", "OR... RIG THE VOTE.", "", ""},
   "LONG LIVE THE ANTPEROR!"},
  {"THE AMBER AGE", 3, 56, 56, 1, 0, 0x00FF, 40, 60, 0, G_AMBER, "REACH THE AMBER AGE  50 AMBER",
   {"DEEP IN THE HILLS LIES", "AMBER: HARD, GOLDEN, AND", "WORTH MORE THAN FOOD.", "A NEW AGE IS CALLING.", "", ""},
   {"RESEARCH AMBER WORKING.", "BUILD AMBER MINES ON HILLS.", "STORE 50 AMBER.", "WATCH FOR THE RED ANTS!", "", ""},
   "THE AMBER AGE HAS BEGUN!"},
  {"THE LONG MARCH", 1, 64, 48, 1, 2, 0x03FF, 80, 120, 40, G_CONQUER, "DESTROY THE RED NEST",
   {"ENOUGH! THE RED QUEEN HAS", "RAIDED US FOR THE LAST", "TIME. RAISE AN ARMY AND", "MARCH ON HER NEST.", "", ""},
   {"TRAIN TROOPS. AT THE NEST", "PRESS \1: FORM ARMY. PUT THE", "CURSOR ON YOUR ARMY, PRESS", "\1 AND PICK WHERE TO MARCH.", "HER GUARDS WILL FIGHT BACK!", ""},
   "THE RED NEST HAS FALLEN!"},
  {"THE GREAT MOUND", 2, 64, 64, 2, 1, 0x07FF, 100, 150, 60, G_WONDER, "BUILD THE GREAT MOUND",
   {"ANTPEROR, ALL ANTS SPEAK", "OF YOU. LET US BUILD A", "MOUND SO TALL IT TOUCHES", "THE CLOUDS. BUT TWO RIVALS", "WILL TRY TO STOP US.", ""},
   {"RESEARCH GREAT MOUNDS.", "BUILD IT AND KEEP 4", "WORKERS ON IT UNTIL IT", "IS DONE. DEFEND IT!", "", ""},
   "THE GREAT MOUND STANDS FOREVER!"},
};

// ---------- state ----------
typedef struct { u8 x, y, t, hp, staff; } EB;
#define EBN 160
static EB ebl[EBN]; static u8 nbl;
EWRAM static u8 ebi[96][96], ezt[96][96];       // building index + 1; territory (bits 0-2) and zone (bits 3-4) packed
#define ETER(y, x) (ezt[y][x] & 7)
#define EZN(y, x) ((ezt[y][x] >> 3) & 3)
static void set_ter(u8 y, u8 x, u8 v) { ezt[y][x] = (u8)((ezt[y][x] & ~7) | v); }
static void set_zn(u8 y, u8 x, u8 v) { ezt[y][x] = (u8)((ezt[y][x] & 7) | (v << 3)); }
EWRAM static u16 efld[MAXH][MAXW];
typedef struct { u8 x, y, ox, oy, age, team, alive, ord, fl, foe; u16 w, s, m, peak; } EA;   // team 0 you, 1..3 rivals; ord 0 hold 1 march 2 home 3 attack
#define EAN 14
static EA ea[EAN];
typedef struct { u8 x, y, ox, oy, age, alive, b, ph, pl, pi, fl; } EW;   // worker: walks from its building home and back
#define EWN 12
static EW ew[EWN];
EWRAM static u16 wpth[EWN][100];
static u16 rf, rd, ra, erp, ecyc, wprog, gw, gs, gm, rgar[3], ewv;
static u8 pop, popcap, unemp, eres = 255, fac[4], edict = 255, edt, erig, elect_t, lowhap, echap, eaggr, enriv, eend, ewhy, era;
static u16 etech;
static u8 rx[3], ry[3], rhp[3], ralive[3], rwt[3], rwave[3];
static const u8 RPAL[3] = {1, 7, 8};
static const char *const RNAME[3] = {"RED", "GOLD", "VIOLET"};
static u8 eplace, eptab, epsel, etgt, etgt_a, ezoom, evote_ok, edry;
static s8 food_net;
static u8 nb_of[B_N];
static u8 rpeace[3], efarm_t, ezbuilt, ebeat, evpend = 255;   // peace cycles per rival, rain(+)/drought(-) timer, zone builds, enemy armies beaten, event to show
static s8 efarm_mod;

static u8 has_t(u8 t) { return (u8)((etech >> t) & 1); }
static u8 era_now(void) { u8 n = 0, i; for (i = 0; i < T_N; i++) n += has_t(i); return has_t(T_GREAT) ? 3 : has_t(T_AMBER) ? 2 : n >= 5 ? 1 : 0; }
static u8 fac_avg(void) { return (u8)((fac[0] + fac[1] + fac[2] + fac[3]) / 4); }
static u16 a_size(const EA *a) { return (u16)(a->w + a->s + a->m); }
static const u16 RICHV[4] = {0, 250, 500, 999}, MEGAV[4] = {0, 100, 250, 500};   // cheat levels
static const u8 ZONEV[4] = {1, 2, 5, 10}, RESV[4] = {1, 2, 5, 5};
static u16 bc(u16 v) { return cht[14] == 2 ? 0 : cht[14] == 1 ? (u16)(v / 2) : v; }   // FREE BUILD: HALF / FREE
static u16 tcost(void) { u8 n = 0, i; for (i = 0; i < T_N; i++) n += has_t(i); return (u16)(12 + 9 * n); }
static void radd(u16 *r, s16 v) { s32 t = (s32)*r + v; *r = (u16)(t < 0 ? 0 : t > 999 ? 999 : t); }

// ---------- map drawing hook (draw_dec calls this in empire mode) ----------
static u8 ter_edge(u8 x, u8 y, u8 v) {
  u8 k; s8 a, b;
  for (k = 0; k < 4; k++) { a = (s8)x + DX[k]; b = (s8)y + DY[k]; if (a < 0 || b < 0 || a >= W || b >= H) continue; if (ETER(b, a) != v) return 1; }
  return 0;
}
static void emp_dec(u8 x, u8 y) {
  u16 t = 0xFFFF; u8 i, e;
  if (ebi[y][x]) { EB *b = &ebl[ebi[y][x] - 1]; t = b->t == B_WONDER ? (wprog >= 40 ? 11 : 12) : BD[b->t].tile; }
  else {
    for (i = 0; i < enriv; i++) if (ralive[i] && x == rx[i] && y == ry[i]) t = 0;
    if (t == 0xFFFF) {
      e = ETER(y, x);
      if (EZN(y, x)) t = (u16)(32 + EZN(y, x));
      else if (e == 1 && ter_edge(x, y, 1)) t = 14;
      else if (e >= 2 && ter_edge(x, y, e)) t = 15;
    }
  }
  if (inwin(x, y)) BGMAP2[(y & 31) * 32 + (x & 31)] = t == 0xFFFF ? 0 : (u16)((EMT + t) | (12 << 12));
}
static void eredraw(u8 x, u8 y, u8 r) { s8 a, b; for (b = (s8)(y - r); b <= y + r; b++) for (a = (s8)(x - r); a <= x + r; a++) if (a >= 0 && b >= 0 && a < W && b < H) draw_cell((u8)a, (u8)b); }

// ---------- territory (Civ borders) ----------
static void terr_disc(u8 cx0, u8 cy0, u8 r, u8 v, u8 over) {
  s8 x, y; s16 rr = (s16)(r * r + r);
  for (y = (s8)(cy0 - r); y <= cy0 + r; y++) for (x = (s8)(cx0 - r); x <= cx0 + r; x++)
    if (x >= 0 && y >= 0 && x < W && y < H && (x - cx0) * (x - cx0) + (y - cy0) * (y - cy0) <= rr && (over || !ETER(y, x))) set_ter((u8)y, (u8)x, v);
}
static void terr_calc(void) {
  u8 x, y, i;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) set_ter(y, x, 0);
  for (i = 0; i < enriv; i++) if (ralive[i]) terr_disc(rx[i], ry[i], 5, (u8)(2 + i), 1);
  for (i = 0; i < nbl; i++) {
    EB *b = &ebl[i];
    terr_disc(b->x, b->y, b->t == B_NEST ? 7 : b->t == B_OUTPOST ? 6 : 2, 1, 0);
  }
  for (y = wy0; y < (u8)(wy0 + 32) && y < H; y++) for (x = wx0; x < (u8)(wx0 + 32) && x < W; x++) emp_dec(x, y);
}

// ---------- buildings ----------
static void count_b(void) { u8 i; for (i = 0; i < B_N; i++) nb_of[i] = 0; for (i = 0; i < nbl; i++) nb_of[ebl[i].t]++; }
static u8 add_b(u8 x, u8 y, u8 t) {
  EB *b;
  if (nbl >= EBN) return 0;
  b = &ebl[nbl]; b->x = x; b->y = y; b->t = t; b->hp = BD[t].hp; b->staff = 0;
  ebi[y][x] = (u8)(++nbl); set_zn(y, x, 0);
  if (t == B_BRIDGE) { hgt[y][x] = 1; draw_around(x, y); }
  count_b();
  return 1;
}
static void del_b(u8 i) {                          // remove building i (keeps the list packed)
  u8 x = ebl[i].x, y = ebl[i].y, j;
  if (ebl[i].t == B_BRIDGE) { hgt[y][x] = 0; draw_around(x, y); }
  if (ebl[i].t == B_WONDER) wprog = 0;
  ebi[y][x] = 0;
  for (j = i; j + 1 < nbl; j++) { ebl[j] = ebl[j + 1]; ebi[ebl[j].y][ebl[j].x] = (u8)(j + 1); }
  nbl--; count_b();
  fx_add(x, y, FX_PUFF);
}
static const char *can_build(u8 t, u8 x, u8 y, u8 pay) {   // 0 = OK, else the reason
  u8 h = hgt[y][x], i;
  if (t >= 100) {
    if (ETER(y, x) != 1) return "OUTSIDE YOUR LAND";
    if (ebi[y][x]) return "A BUILDING IS THERE";
    if (t == L_PILE) { if (h >= 3) return "ALREADY HIGHEST"; if (pay && rd < 5) return "NEEDS 5 DIRT"; }
    if (t == L_DIG && !h) return "ALREADY WATER";
    if (t >= L_ZH && t <= L_ZW && !h) return "NOT ON WATER";
    return 0;
  }
  if (!has_t(BD[t].tech)) return "RESEARCH IT FIRST";
  if (ETER(y, x) != 1) return "OUTSIDE YOUR LAND";
  if (ebi[y][x]) return "A BUILDING IS THERE";
  for (i = 0; i < enriv; i++) if (ralive[i] && x == rx[i] && y == ry[i]) return "ENEMY NEST";
  if (!((BD[t].ter >> h) & 1)) return t == B_BRIDGE ? "ONLY ON WATER" : t == B_MINE ? "ONLY ON HILLS" : h == 0 ? "NOT ON WATER" : "WRONG GROUND HERE";
  if (t == B_WONDER && nb_of[B_WONDER]) return "ONLY ONE WONDER";
  if (t == B_PALACE && nb_of[B_PALACE]) return "ONLY ONE PALACE";
  if (pay && (rf < bc(BD[t].f) || rd < bc(BD[t].d) || ra < bc(BD[t].a))) return "NOT ENOUGH STUFF";
  return 0;
}
static u8 do_build(u8 t, u8 x, u8 y) {
  const char *why = can_build(t, x, y, 1);
  if (why) { say(why); sfx_deny(); return 0; }
  if (t >= 100) {
    if (t == L_PILE) { rd -= 5; hgt[y][x]++; draw_around(x, y); sfx_raise(); }
    else if (t == L_DIG) { radd(&rd, 2); hgt[y][x]--; if (!hgt[y][x]) set_zn(y, x, 0); draw_around(x, y); sfx_lower(); }
    else { set_zn(y, x, t == L_ZX ? 0 : (u8)(t - L_ZH + 1)); emp_dec(x, y); }
    return 1;
  }
  rf -= bc(BD[t].f); rd -= bc(BD[t].d); ra -= bc(BD[t].a);
  add_b(x, y, t); terr_calc(); eredraw(x, y, 1); sfx_spawn(); fx_add(x, y, FX_PLUS);
  if (t == B_WONDER) say("THE GREAT MOUND BEGINS!");
  return 1;
}

// ---------- armies ----------
static EA *new_army(u8 team, u8 x, u8 y, u16 w, u16 s, u16 m) {
  u8 i;
  for (i = 0; i < EAN; i++) if (!ea[i].alive) {
    EA *a = &ea[i]; a->x = a->ox = x; a->y = a->oy = y; a->age = 8; a->team = team; a->alive = 1; a->ord = team ? 3 : 0;
    a->w = w; a->s = s; a->m = m; a->peak = (u16)(w + s + m); a->fl = team != 0; a->foe = 255; return a;
  }
  return 0;
}
static u16 my_troops(void) { u8 i; u16 n = (u16)(gw + gs + gm); for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team) n += a_size(&ea[i]); return n; }
static void efield(u8 tx, u8 ty) {                 // BFS walking distance to (tx, ty) for marching armies
  u16 hd = 0, tl = 0, c, v, *d = &efld[0][0], ni; u8 x, y, h, nh;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) efld[y][x] = NOPATH;
  efld[ty][tx] = 0; bq[tl++] = (u16)((ty << 7) | tx);
  while (hd < tl) {
    c = bq[hd++]; y = (u8)(c >> 7); x = (u8)(c & 127); v = (u16)(d[c] + 1); h = hgt[y][x];
#define EF_TRY(cond, off) if (cond) { ni = (u16)(c + (off)); nh = (&hgt[0][0])[ni]; if (nh && nh + 1 >= h && nh <= h + 1 && d[ni] == NOPATH) { d[ni] = v; bq[tl++] = ni; } }
    EF_TRY(x + 1 < W, 1) EF_TRY(x > 0, -1) EF_TRY(y + 1 < H, MAXW) EF_TRY(y > 0, -MAXW)
  }
}
static u8 step_down(u8 *x, u8 *y, u16 (*f)[MAXW]) { // one step to a neighbour with a smaller field value
  u8 k, bk = 255; u16 best = f[*y][*x];
  for (k = 0; k < 4; k++) if (can_go(*x, *y, DX[k], DY[k])) { u16 v = f[*y + DY[k]][*x + DX[k]]; if (v < best) { best = v; bk = k; } }
  if (bk == 255) return 0;
  *x = (u8)(*x + DX[bk]); *y = (u8)(*y + DY[bk]); return (u8)(bk + 1);
}
static void loss(u16 *w, u16 *s, u16 *m, u16 n) {  // n casualties, warriors first
  while (n) { if (*w) (*w)--; else if (*s) (*s)--; else if (*m) (*m)--; else break; n--; }
}
static u8 towers_near(u8 x, u8 y, u8 r) { u8 i, n = 0; for (i = 0; i < nbl; i++) if ((ebl[i].t == B_TOWER || ebl[i].t == B_SPIT) && dist(ebl[i].x, x) + dist(ebl[i].y, y) <= r) n++; return n; }
#ifdef BATTLETEST
static u16 dbg_fr;
#endif
static void zoom_battle(EA *me, u16 *ew_, u16 *es_, u16 *em_, u8 rv, u8 x, u8 y) {   // the pixel battle, then back to the map
  u16 *aw = me ? &me->w : &gw, *as = me ? &me->s : &gs, *am = me ? &me->m : &gm;
  ebat_pal = RPAL[rv]; ebat_name = RNAME[rv];
  if (battle(aw, as, am, ew_, es_, em_, hgt[y][x], towers_near(x, y, 4), 1) && (*aw + *as + *am)) ach_get(17);
  // after the bitmap screen: rebuild the map view
  win_all(); ov_clear(); hide_all(); oam_flush();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy;
  DISPCNT = 0x1F40; music_start(); fade(16, 0);
}

// ---------- workers (AoE villagers walking goods home) ----------
static void worker_spawn(void) {
  u8 i, j, tries;
  for (i = 0; i < EWN; i++) if (!ew[i].alive) {
    for (tries = 0; tries < 8; tries++) {
      j = (u8)(rnd() % (nbl ? nbl : 1)); if (!nbl) return;
      if (ebl[j].staff && ebl[j].t != B_NEST && dnp[0][ebl[j].y][ebl[j].x] != NOPATH) {
        EW *w = &ew[i]; w->x = w->ox = ebl[j].x; w->y = w->oy = ebl[j].y; w->age = 8; w->alive = 1; w->b = j; w->ph = 0; w->pl = 0; return;
      }
    }
    return;
  }
}
static void worker_step(EW *w) {
  u8 ox = w->x, oy = w->y;
  if (w->ph == 0) {                                // going home, remembering the way
    if (dnp[0][w->y][w->x] == 0 || w->pl >= 100) { w->ph = 1; w->pi = w->pl; if (!w->pl) w->alive = 0; fx_add(w->x, w->y, FX_PLUS); return; }
    if (!step_down(&w->x, &w->y, dnp[0])) { w->alive = 0; return; }
    wpth[w - ew][w->pl++] = (u16)((oy << 7) | ox);
  } else {                                         // back to work along the same path
    if (!w->pi) { w->alive = 0; return; }
    w->pi--; w->x = (u8)(wpth[w - ew][w->pi] & 127); w->y = (u8)(wpth[w - ew][w->pi] >> 7);
  }
  w->ox = ox; w->oy = oy; w->age = 0; if (w->x != ox) w->fl = w->x < ox;
}

// ---------- the economy cycle (every 30 ticks, about 4 seconds) ----------
static void apr_f(u8 i, s8 d) { s16 v = (s16)fac[i] + d; fac[i] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v); }
static void approval_all(s8 d) { u8 i; for (i = 0; i < 4; i++) apr_f(i, d); }
static void end_game(u8 win, u8 why) {
  if (eend) return;
  eend = (u8)(win ? 1 : 2); ewhy = why;
  if (win) {
    if (why == 3 || (why == 10 && echap == 5)) ach_get(19);
    if (why == 2 || (why == 10 && echap == 6)) ach_get(20);
    if (why == 10 && echap == 0) ach_get(24);
    if (why == 10 && echap == NCH - 1) ach_get(23);
  }
}
static void eelection(void);
static void ecycle(void) {
  u8 i, x, y, work = pop, protest, starve = 0;
  s16 f = 0, d = 0, a = 0, r = 1, eat, m;
  u8 tg[4];
  ecyc++;
  count_b(); eshell = has_t(T_SHELL); earm = nb_of[B_ARMORY] ? 1 : 0;
  if (cht[3]) { u16 v = RICHV[cht[3]]; if (rf < v) rf = v; if (rd < v) rd = v; if (ra < v) ra = v; }
  if (efarm_t && !--efarm_t) efarm_mod = 0;
  for (i = 0; i < 3; i++) if (rpeace[i] && !--rpeace[i] && ralive[i]) say("A PEACE TREATY HAS ENDED");
  if (ecyc % 20 == 0 && ecyc >= 20 && echap != 0) evpend = (u8)(rnd() % 10);   // random events (Tropico)
  // trophies
  if (nbl >= 10) ach_get(12); if (nbl >= 40) ach_get(13); if (pop >= 60) ach_get(14); if ((etech & 0xFFF) == 0xFFF) ach_get(15);
  if (era_now() >= 2) ach_get(16); if (my_troops() >= 500) ach_get(18);
  if (fac[0] >= 80 && fac[1] >= 80 && fac[2] >= 80 && fac[3] >= 80) ach_get(21);
  if (rf >= 999 || rd >= 999 || ra >= 999) ach_get(28);
  if (ezbuilt >= 10) ach_get(25); if (ebeat >= 10) ach_get(27);
  for (i = 0; i < nbl; i++) { EB *b = &ebl[i]; b->staff = BD[b->t].jobs < work ? BD[b->t].jobs : work; work -= b->staff; }
  unemp = work;
  protest = fac_avg() < 30 && edict != 2;
  m = 100 + (edict == 1 ? 50 : 0) - (edict == 6 ? 25 : 0) - (protest ? 40 : 0);
  for (i = 0; i < nbl; i++) {
    EB *b = &ebl[i]; u8 full = b->staff >= BD[b->t].jobs, any = b->staff > 0;
    if (BD[b->t].jobs && !any) continue;
    switch (b->t) {
    case B_NEST: f += 2; r += 1; break;
    case B_FARM: { u8 k, wv = 0; for (k = 0; k < 4; k++) { s8 xx = (s8)b->x + DX[k], yy = (s8)b->y + DY[k]; if (xx >= 0 && yy >= 0 && xx < W && yy < H && !hgt[yy][xx]) wv = 1; }
      f += (s16)((3 + wv + has_t(T_GENES)) * (full ? 2 : 1) / 2 / (edry ? 2 : 1) * (efarm_mod > 0 ? 2 : 1) / (efarm_mod < 0 ? 2 : 1)); break; }
    case B_APHID: f += full ? 5 : 2; break;
    case B_PIT: d += full ? 3 : 1; break;
    case B_MINE: a += full ? 2 : 1; break;
    case B_MARKET: a += 1; break;
    case B_VAULT: a += (s16)(ra / 25 > 3 ? 3 : ra / 25); break;
    case B_TRADE: if (rf >= 5) { f -= 5; a += 2; } break;
    case B_ARCH: r += full ? 2 : 1; break;
    case B_ACAD: r += full ? 4 : 2; break;
    case B_WONDER: if (full && rf >= 2 && rd >= 3 && ra >= 1) { wprog++; rf -= 2; rd -= 3; ra -= 1; if (wprog == 40) { say("THE GREAT MOUND IS DONE!"); emp_dec(b->x, b->y); end_game(1, 2); } } break;
    }
  }
  if (f > 0) f = (s16)(f * m / 100); d = (s16)(d * m / 100); a = (s16)(a * m / 100);
  eat = (s16)(pop / 2 + my_troops() / 25);
  eat = (s16)(eat * (100 - 15 * (nb_of[B_GRAN] > 3 ? 3 : nb_of[B_GRAN])) / 100);
  f -= eat; food_net = (s8)(f < -99 ? -99 : f > 99 ? 99 : f);
  if ((s32)rf + f < 0) { rf = 0; starve = 1; } else radd(&rf, f);
  radd(&rd, d); radd(&ra, a);
  popcap = (u8)(8 + 5 * nb_of[B_HOUSE]);
  if (starve) { if (pop > 3) pop--; say("FAMINE! BUILD MORE FARMS"); }
  else if (pop < popcap && rf > pop) { pop = (u8)(pop + 1 + (nb_of[B_NURS] > 3 ? 3 : nb_of[B_NURS])); if (pop > popcap) pop = popcap; }
  if (eres != 255) { erp += (u16)(r * RESV[cht[13]]); if (cht[13] == 3) erp = tcost(); if (erp >= tcost()) { erp -= tcost(); etech |= (u16)(1 << eres); say("RESEARCH DONE! \5 MENU"); sfx_mana(); eres = 255; } }
  else if (!(ecyc & 7)) say("PICK RESEARCH: \5 MENU");
  i = era_now(); if (i != era) { era = i; say(ERAN[era]); jingle(WIN_TUNE, 6); }
  // factions (Tropico): each drifts toward what it wants
  tg[0] = (u8)(50 + (starve ? -25 : food_net > 0 ? 8 : 0) + (nb_of[B_FARM] + nb_of[B_APHID] > 5 ? 15 : 3 * (nb_of[B_FARM] + nb_of[B_APHID])) + 4 * (nb_of[B_GARDEN] > 3 ? 3 : nb_of[B_GARDEN]));
  tg[1] = (u8)(50 + (pop < popcap ? 5 : -8) + 5 * (nb_of[B_MARKET] > 3 ? 3 : nb_of[B_MARKET]) + 2 * (nb_of[B_HOUSE] > 5 ? 5 : nb_of[B_HOUSE]) + 3 * (nb_of[B_GARDEN] > 3 ? 3 : nb_of[B_GARDEN]) - (unemp > 6 ? 18 : 3 * unemp));
  tg[2] = (u8)(48 + 5 * (nb_of[B_BARR] > 2 ? 2 : nb_of[B_BARR]) + 3 * (nb_of[B_TOWER] > 4 ? 4 : nb_of[B_TOWER]) + 4 * nb_of[B_ARMORY] + 5 * (nb_of[B_STATUE] > 3 ? 3 : nb_of[B_STATUE]) + (my_troops() > 150 ? 15 : my_troops() / 10));
  tg[3] = (u8)(48 + 10 * (nb_of[B_TEMPLE] > 3 ? 3 : nb_of[B_TEMPLE]) + 5 * nb_of[B_PALACE] + (edict == 3 ? 20 : 0));
  for (i = 0; i < 4; i++) {
    s16 t = tg[i] + 6 * (nb_of[B_DANCE] > 2 ? 2 : nb_of[B_DANCE]) + 2 * (nb_of[B_STATUE] > 3 ? 3 : nb_of[B_STATUE]) + 6 * nb_of[B_PALACE] + (edict == 6 ? 8 : 0) - (edict == 1 ? 10 : 0) - (edict == 2 ? 10 : 0) + (edict == 2 && i == 2 ? 25 : 0);
    if (t > 100) t = 100; if (t < 0) t = 0;
    if (fac[i] < t) apr_f(i, (s8)(t - fac[i] > 3 ? 3 : t - fac[i])); else if (fac[i] > t) apr_f(i, (s8)-(fac[i] - t > 3 ? 3 : fac[i] - t));
  }
  if (edict != 255 && !--edt) { say("THE EDICT HAS ENDED"); edict = 255; }
  if (protest && !(ecyc & 3)) say("PROTESTS! WORK IS SLOW");
  if (fac_avg() < 15 && edict != 2) { if (++lowhap >= 5) end_game(0, 1); else say("REVOLUTION IS NEAR!"); } else lowhap = 0;
  if (!--elect_t) { elect_t = 45; eelection(); }
  // SimCity zones: the colony builds what it needs on painted land, at half price
  {
    u8 want[4] = {0, (u8)(pop + 1 >= popcap), (u8)(food_net < 3), (u8)(unemp == 0 && pop + 2 < popcap)}, built = 0; u16 tries;
    for (tries = 0; tries < 300 && built < ZONEV[cht[9]]; tries++) {
      x = (u8)(rnd() % W); y = (u8)(rnd() % H);
      if (!EZN(y, x) || ebi[y][x] || ETER(y, x) != 1 || !want[EZN(y, x)]) continue;
      { u8 z = EZN(y, x), t = z == 1 ? B_HOUSE : z == 2 ? ((hgt[y][x] == 2 && has_t(T_GENES) && (rnd() & 1)) ? B_APHID : B_FARM) : ((hgt[y][x] == 3 && has_t(T_AMBER) && (rnd() & 1)) ? B_MINE : B_PIT);
        if (!has_t(BD[t].tech) || !((BD[t].ter >> hgt[y][x]) & 1) || rf < BD[t].f / 2 || rd < BD[t].d / 2) continue;
        if (!cht[9]) { rf -= BD[t].f / 2; rd -= BD[t].d / 2; } add_b(x, y, t); terr_calc(); eredraw(x, y, 1); fx_add(x, y, FX_PLUS); built++; ezbuilt++; }
    }
  }
  // healers bring wounded back
  for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && a_size(&ea[i]) < ea[i].peak) {
    u8 j; for (j = 0; j < nbl; j++) if (ebl[j].t == B_HEAL && ebl[j].staff && dist(ebl[j].x, ea[i].x) + dist(ebl[j].y, ea[i].y) <= 4) ea[i].w += 2;
    if (a_size(&ea[i]) > ea[i].peak) ea[i].w = (u16)(ea[i].peak - ea[i].s - ea[i].m);
  }
  // rivals grow and send armies (AoE waves)
  for (i = 0; i < enriv; i++) if (ralive[i]) {
    u16 add = (u16)(2 + eaggr * 2 + (ecyc / 40 > 3 ? 3 : ecyc / 40) * 2 + i);
    rgar[i] = (u16)(rgar[i] + add > 500 ? 500 : rgar[i] + add);
    if (cht[8] || rpeace[i]) continue;
    if (!--rwt[i]) {
      u16 sz = (u16)(12 + rwave[i] * 18 * (eaggr + 1) / 2); EA *a;
      if (sz > rgar[i]) sz = rgar[i]; if (sz > 500) sz = 500;
      if (sz >= 6 && (a = new_army((u8)(1 + i), rx[i], ry[i], (u16)(sz * 7 / 10), (u16)(sz * 2 / 10 * has_t(T_AMBER) + 0), 0))) {
        a->m = (u16)(sz - a->w - a->s); if (era < 1) { a->w += a->m; a->m = 0; }
        rgar[i] -= sz; rwave[i]++; ewv++;
        alt = 240; alx = rx[i]; aly = ry[i]; say(i == 0 ? "A RED ARMY MARCHES! \6 LOOK" : i == 1 ? "A GOLD ARMY MARCHES! \6 LOOK" : "A VIOLET ARMY! \6 LOOK"); sfx_alarm();
      }
      rwt[i] = (u8)((eaggr == 0 ? 14 : eaggr == 1 ? 10 : 7) - (rwave[i] > 4 ? 4 : rwave[i]) + 2 * (nb_of[B_EMBASSY] > 2 ? 2 : nb_of[B_EMBASSY]));
    }
  }
  if (ecyc % 3 == 0) worker_spawn();
  // goals
  if (echap != 255) {
    u8 g = CH[echap].goal, ok = 0;
    if (g == G_GROW) ok = pop >= 12 && nb_of[B_FARM] >= 2 && nb_of[B_HOUSE] >= 1;
    else if (g == G_MARKET) ok = nb_of[B_MARKET] >= 1 && rf >= 60;
    else if (g == G_SURVIVE) { u8 any = 0; for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].team) any = 1; ok = ewv >= 5 && !any; }
    else if (g == G_VOTE) ok = evote_ok;
    else if (g == G_AMBER) ok = era >= 2 && ra >= 50;
    else if (g == G_CONQUER) { ok = 1; for (i = 0; i < enriv; i++) if (ralive[i]) ok = 0; }
    else if (g == G_WONDER) ok = wprog >= 40;
    if (ok) end_game(1, 10);
  } else if (enriv) { u8 ok = 1; for (i = 0; i < enriv; i++) if (ralive[i]) ok = 0; if (ok) end_game(1, 3); }
}
static void eelection(void) {
  u8 avg = fac_avg(), i, all = 1, won;
  for (i = 0; i < 4; i++) if (fac[i] < 45) all = 0;
  won = erig || avg + 10 * nb_of[B_PALACE] >= 50;
  if (erig) { erig = 0; say("THE VOTE WAS... ARRANGED"); }
  if (won) { radd(&ra, 20); if (!erig) say("YOU WON THE ELECTION! +20 AMBER"); sfx_mana(); if (all) evote_ok = 1; if (echap == 255 && pop >= 50 && avg >= 70) end_game(1, 4); }
  else { rf /= 2; rd /= 2; ra /= 2; for (i = 0; i < 4; i++) fac[i] = 40; say("LOST THE VOTE! COUP: HALF LOST"); sfx_qdead(); }
}

// ---------- the per-slice simulation ----------
static u8 eslice;
static void armies_tick(void) {
  u8 i, j, k, x, y; EA *a, *b;
  for (i = 0; i < EAN; i++) {
    a = &ea[i]; if (!a->alive) continue;
    if (!a_size(a)) { a->alive = 0; continue; }
    if (a->foe != 255) {                           // fighting on the map: both sides lose troops every tick
      b = a->foe < EAN ? &ea[a->foe] : 0;
      if (b && b->alive && a_size(b) && dist(a->x, b->x) + dist(a->y, b->y) <= 1) {
        u16 sa = (u16)(a->w + 2 * a->s + 4 * a->m), sb = (u16)(b->w + 2 * b->s + 4 * b->m);
        u16 la = (u16)(sb / 14 + 1), lb = (u16)(sa / 14 + 1);
        if (a->team == 0) lb += earm * lb / 3; else la += earm * la / 3;
        loss(&a->w, &a->s, &a->m, la); loss(&b->w, &b->s, &b->m, lb);
        fx_add(a->x, a->y, FX_PUFF); if (!(gt & 3) && !nosfx) sfx_fight();
        if (!a_size(a)) { a->alive = 0; b->foe = 255; say(a->team ? "ENEMY ARMY BEATEN!" : "OUR ARMY WAS LOST"); if (a->team) ebeat++; }
        if (!a_size(b)) { b->alive = 0; a->foe = 255; say(b->team ? "ENEMY ARMY BEATEN!" : "OUR ARMY WAS LOST"); if (b->team) ebeat++; }
        continue;
      }
      a->foe = 255;
    }
    if ((gt & 1) != (i & 1)) continue;             // armies move every other tick
    a->ox = a->x; a->oy = a->y;
    if (a->team) {                                 // rival army: march on your nest, smash what is in the way
      x = a->x; y = a->y;
      for (k = 0; k < 4; k++) {                    // attack a building next to it
        s8 xx = (s8)a->x + DX[k], yy = (s8)a->y + DY[k];
        if (xx < 0 || yy < 0 || xx >= W || yy >= H || !ebi[yy][xx]) continue;
        { EB *bb = &ebl[ebi[yy][xx] - 1]; u8 dmg = (u8)(1 + a_size(a) / (bb->t == B_NEST ? 80 : 40));
          if (bb->t == B_NEST && (gw + gs + gm)) break;   // the nest has guards: fight them below
          fx_add((u8)xx, (u8)yy, FX_PUFF);
          if (bb->hp <= dmg) {
            if (bb->t == B_NEST) { if (cht[1]) { bb->hp = 250; goto moved; } end_game(0, 0); return; }
            del_b((u8)(ebi[yy][xx] - 1)); terr_calc(); eredraw((u8)xx, (u8)yy, 1); say("A BUILDING WAS DESTROYED!");
          } else bb->hp -= dmg;
          alt = 120; alx = (u8)xx; aly = (u8)yy;
          goto moved;
        }
      }
      if (dnp[0][a->y][a->x] <= 1 && (gw + gs + gm)) {   // at the nest: the garrison fights
        u16 sa = (u16)(a->w + 2 * a->s + 4 * a->m), sb = (u16)(gw + 2 * gs + 4 * gm + towers_near(a->x, a->y, 4) * 20);
        loss(&a->w, &a->s, &a->m, (u16)(sb / 14 + 1)); loss(&gw, &gs, &gm, (u16)(sa / 14 + 1));
        fx_add(a->x, a->y, FX_PUFF); if (!(gt & 3)) sfx_fight();
        alt = 120; alx = a->x; aly = a->y; ezoom = (u8)(i + 1);
        goto moved;
      }
      if (step_down(&a->x, &a->y, dnp[0])) {
        if (ebi[a->y][a->x] && ebl[ebi[a->y][a->x] - 1].t == B_WALL) { EB *bb = &ebl[ebi[a->y][a->x] - 1]; a->x = x; a->y = y;   // a wall: smash it
          if (bb->hp <= 2) { u8 wx_ = bb->x, wy_ = bb->y; del_b((u8)(ebi[wy_][wx_] - 1)); terr_calc(); eredraw(wx_, wy_, 1); } else bb->hp -= 2; fx_add(x, y, FX_PUFF); }
        else if (ebi[a->y][a->x] && ebl[ebi[a->y][a->x] - 1].t == B_SPIKE) { loss(&a->w, &a->s, &a->m, (u16)(a_size(a) / 20 + 2)); fx_add(a->x, a->y, FX_PUFF); }
      }
    } else {                                       // your army
      if (a->ord == 1 || a->ord == 3) {
        if (efld[a->y][a->x] == 0 || !step_down(&a->x, &a->y, efld)) a->ord = 0;
      }
      if (a->ord != 2) {
        for (j = 0; j < enriv; j++) if (ralive[j] && dist(a->x, rx[j]) + dist(a->y, ry[j]) <= 1) {
          if (rpeace[j]) { rpeace[j] = 0; say("WE BROKE THE PEACE!"); approval_all(-5); }   // at a rival nest: fight the guards, then the nest
          if (rgar[j]) { u16 sa = (u16)(a->w + 2 * a->s + 4 * a->m), sb = rgar[j]; u16 w2 = (u16)(rgar[j] * 7 / 10), s2 = (u16)(rgar[j] / 5), m2 = (u16)(rgar[j] - w2 - s2);
            loss(&a->w, &a->s, &a->m, (u16)(sb / 14 + 1)); loss(&w2, &s2, &m2, (u16)(sa / 14 + 1)); rgar[j] = (u16)(w2 + s2 + m2); fx_add(rx[j], ry[j], FX_PUFF); if (!(gt & 3)) sfx_fight(); ezoom = (u8)(i + 1); }
          else { u8 dmg = (u8)(1 + a_size(a) / 30); fx_add(rx[j], ry[j], FX_HEART);
            if (rhp[j] <= dmg) { ralive[j] = 0; say("A RIVAL NEST HAS FALLEN!"); jingle(WIN_TUNE, 6); terr_calc(); eredraw(rx[j], ry[j], 2);
              for (k = 0; k < EAN; k++) if (ea[k].alive && ea[k].team == j + 1) ea[k].alive = 0; }
            else rhp[j] -= dmg; }
        }
      } else if (a->ord == 2) {
        if (dnp[0][a->y][a->x] <= 1) { gw += a->w; gs += a->s; gm += a->m; a->alive = 0; say("THE ARMY IS HOME"); continue; }
        step_down(&a->x, &a->y, dnp[0]);
      }
    }
moved:
    if (a->x != a->ox || a->y != a->oy) { a->age = 0; if (a->x != a->ox) a->fl = a->x < a->ox; }
    for (j = 0; j < EAN; j++) {                    // meet an enemy army: battle starts
      b = &ea[j]; if (j == i || !b->alive || b->team == a->team || (a->team && b->team)) continue;
      if (dist(a->x, b->x) + dist(a->y, b->y) <= 1 && a->foe == 255) {
        a->foe = j; b->foe = i;
        alt = 200; alx = a->x; aly = a->y; ezoom = (u8)(i + 1);
        say("BATTLE!  \3 ZOOM IN"); sfx_hit();
      }
    }
  }
  // towers and spitter nests shoot nearby enemy armies; spikes hurt
  for (i = 0; i < nbl; i++) {
    EB *b2 = &ebl[i]; u8 r;
    if ((b2->t != B_TOWER && b2->t != B_SPIT) || !b2->staff) continue;
    r = b2->t == B_TOWER ? 3 : 5;
    if ((gt + i) % (b2->t == B_TOWER ? 2 : 3)) continue;
    for (j = 0; j < EAN; j++) { EA *e = &ea[j]; if (e->alive && e->team && dist(e->x, b2->x) + dist(e->y, b2->y) <= r) { loss(&e->w, &e->s, &e->m, (u16)(1 + eshell + (b2->t == B_SPIT))); fx_add(e->x, e->y, FX_SPLASH); break; } }
  }
}
static void eslice_run(void) {
  u8 i;
  for (i = 0; i < EWN; i++) if (ew[i].alive && ew[i].age < 8) ew[i].age++;
  for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].age < 8 && (eslice & 1)) ea[i].age++;
  for (i = eslice; i < EWN; i += 8) if (ew[i].alive) worker_step(&ew[i]);
  if (++eslice < 8) return;
  eslice = 0; gt++;
  armies_tick();
  if (!(gt % 30)) ecycle();
}

// ---------- drawing ----------
static void p3(u8 x, u8 y, u16 v, u8 p) { pc(x, y, v >= 100 ? (char)('0' + v / 100 % 10) : ' ', p); pc((u8)(x + 1), y, v >= 10 ? (char)('0' + v / 10 % 10) : ' ', p); pc((u8)(x + 2), y, (char)('0' + v % 10), p); }
static void ehud(void) {
  u8 i, x; u16 c = tcost();
  ov_fill(1, 1, 17, 19);
  pc(0, 17, '%', 13); p3(1, 17, rf, 13); pc(5, 17, '[', 2); p3(6, 17, rd, 2); pc(10, 17, ']', 9); p3(11, 17, ra, 9);
  pc(15, 17, '{', 1); p3(16, 17, pop, 1); pc(19, 17, '/', 15); p3(20, 17, popcap, 1);
  pc(24, 17, '^', 2); pn(25, 17, fac_avg(), fac_avg() < 30 ? 8 : 2);
  if (spd != 4) ps(28, 17, SPTAG[spd], 2);
  pc(0, 18, '=', 8); { u16 t = my_troops(); pc(1, 18, t >= 1000 ? '9' : ' ', 1); p3(1, 18, t > 999 ? 999 : t, 1); }
  pc(5, 18, '}', 14);
  if (eres != 255) { ps(6, 18, TNAME[eres], 14); for (i = 0; i < 4; i++) BGMAP1[18 * 32 + 20 + i] = (u16)((erp * 4 / c > i ? BAR_F : BAR_E) | (14 << 12)); }
  else ps(6, 18, "NO RESEARCH", 15);
  x = 25; pc(x, 18, 'H', pop + 1 >= popcap ? 13 : 15); pc((u8)(x + 1), 18, 'F', food_net < 3 ? 13 : 15); pc((u8)(x + 2), 18, 'W', unemp == 0 ? 13 : 15);
  if (msgt) { psx(0, 19, msg, 2); msgt--; return; }
  if (eplace) {
    const char *why = can_build(eplace, cx, cy, 1);
    psx(0, 19, why ? why : eplace >= 100 ? LTN[eplace - 100] : BD[eplace].nm, why ? 8 : 13); ps(19, 19, "\1 PUT \2 STOP", 1); return;
  }
  if (etgt) { ps(0, 19, "MARCH WHERE?  \1 GO  \2 CANCEL", 2); return; }
  if (ebi[cy][cx]) { EB *b = &ebl[ebi[cy][cx] - 1]; x = psx(0, 19, BD[b->t].nm, 2);
    if (BD[b->t].jobs) { pc((u8)(x + 1), 19, (char)('0' + b->staff), 1); pc((u8)(x + 2), 19, '/', 15); pc((u8)(x + 3), 19, (char)('0' + BD[b->t].jobs), 1); }
    ps(20, 19, "\1 OPEN", 1); return; }
  for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].x == cx && ea[i].y == cy) { psx(0, 19, ea[i].team ? RNAME[ea[i].team - 1] : "YOUR ARMY", ea[i].team ? 8 : 13); p3(10, 19, a_size(&ea[i]), 1); if (!ea[i].team) ps(20, 19, "\1 ORDERS", 1); return; }
  for (i = 0; i < enriv; i++) if (ralive[i] && cx == rx[i] && cy == ry[i]) { psx(0, 19, RNAME[i], 8); ps(7, 19, "NEST  GUARDS", 1); p3(20, 19, rgar[i], 8); return; }
  if (ETER(cy, cx) == 1) ps(0, 19, "\1 BUILD  \3 ZOOM  \5 MENU", 1); else ps(0, 19, ETER(cy, cx) ? "ENEMY LAND" : "WILD LAND", 15);
}
static void edraw_sprites(void) {
  u8 i, s = 0, k; EA *a; EW *w;
  for (i = 0; i < EWN; i++, s++) { w = &ew[i]; if (!w->alive) { spr_hide(s); continue; }
    place_px(s, (u8)((w->ph == 0 ? 5 : 3) + (((frc >> 2) + i) & 1)), 0, w->ox * 8 + (w->x - w->ox) * w->age, w->oy * 8 + (w->y - w->oy) * w->age, w->fl); }
  for (i = 0; i < EAN; i++, s++) { a = &ea[i]; if (!a->alive) { spr_hide(s); continue; }
    k = a->team ? RPAL[a->team - 1] : 0;
    place16(s, ARMT_T, a->foe != 255 && (frc & 4) ? 4 : k, a->ox * 8 + (a->x - a->ox) * a->age, a->oy * 8 + (a->y - a->oy) * a->age - ((frc >> 4) & 1), a->fl); }
  place16(s++, 16, 0, nestx[0] * 8, nesty[0] * 8 - ((frc >> 4) & 1), 0);
  for (i = 0; i < 3; i++, s++) { if (i < enriv && ralive[i]) place16(s, 16, RPAL[i], rx[i] * 8, ry[i] * 8, 1); else spr_hide(s); }
  for (; s < MAXA + 2; s++) spr_hide(s);
  place16(MAXA + 2, 20, (eplace && can_build(eplace, cx, cy, 1)) ? 5 : 2, cx * 8, cy * 8, 0);
  for (i = 0; i < 8; i++) { Fx *f = &fx[i]; if (!f->t) { spr_hide(MAXA + 3 + i); continue; } f->t--; if (f->tile != FX_PUFF && (f->t & 1)) f->y--; place_px(MAXA + 3 + i, f->tile, 3, f->x, f->y, 0); }
}

// ---------- menus ----------
static u16 emk, emp_, emprev;
static u8 ekey(void) { vsync(); emk = joy(); emp_ = emk & ~emprev; emprev = emk; return emp_ != 0; }
static void ewait_clear(void) { while (joy()) vsync(); emprev = 0; }
static void cost_txt(u8 x, u8 y, u8 t) {
  if (cht[14] == 2) { ps(x, y, "FREE", 13); return; }
  if (BD[t].f) { pc(x, y, '%', 13); p3((u8)(x + 1), y, bc(BD[t].f), 13); x += 5; }
  if (BD[t].d) { pc(x, y, '[', 2); p3((u8)(x + 1), y, bc(BD[t].d), 2); x += 5; }
  if (BD[t].a) { pc(x, y, ']', 9); p3((u8)(x + 1), y, bc(BD[t].a), 9); }
}
static u8 build_menu(void) {                       // tabs (L / R) of things to build; returns what to place (0 = nothing)
  u8 sel = epsel, tab = eptab, n, i, t, r = 0;
  hide_all(); vsync(); oam_flush(); ewait_clear();
  for (;;) {
    for (n = 0; TABS[tab][n]; n++) {}
    if (sel >= n) sel = (u8)(n - 1);
    ov_fill(1, 3, 0, 19); ui_full();
    ps(1, 0, "\3", 1); ps((u8)(15 - (blen2(TABN[tab]) + 1) / 2), 0, TABN[tab], 9); ps(28, 0, "\4", 1);
    for (i = 0; i < NTAB; i++) BGMAP1[1 * 32 + 12 + i] = (u16)((ORN + (i == tab ? 3 : 4)) | (9 << 12));
    pm_rule(2);
    for (i = 0; i < n; i++) {
      t = TABS[tab][i];
      if (i == sel) BGMAP1[(3 + i * 2) * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      if (t >= 100) { ps(3, (u8)(3 + i * 2), LTN[t - 100], i == sel ? 9 : 3); ps(17, (u8)(3 + i * 2), LTD[t - 100], 10); }
      else {
        u8 ok = has_t(BD[t].tech);
        ps(3, (u8)(3 + i * 2), BD[t].nm, !ok ? 15 : i == sel ? 9 : 3);
        if (ok) cost_txt(16, (u8)(3 + i * 2), t); else ps(16, (u8)(3 + i * 2), "LOCKED", 15);
      }
    }
    pm_rule(17);
    t = TABS[tab][sel];
    if (t >= 100) ps(1, 18, LTD[t - 100], 3);
    else if (!has_t(BD[t].tech)) { ps(1, 18, "NEEDS:", 8); ps(8, 18, TNAME[BD[t].tech], 3); }
    else { ps(1, 18, BD[t].d1, 3); ps(1, 19, BD[t].d2, 10); }
    if (t < 100 && has_t(BD[t].tech) && BD[t].d2[0] == 0) ps(1, 19, "\1 PICK  \2 BACK  \3\4 TABS", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_L) { tab = (u8)((tab + NTAB - 1) % NTAB); sel = 0; sfx_food(); }
    if (emp_ & J_R) { tab = (u8)((tab + 1) % NTAB); sel = 0; sfx_food(); }
    if (emp_ & J_UP) { sel = sel ? sel - 1 : n - 1; sfx_food(); }
    if (emp_ & J_DOWN) { sel = sel + 1 >= n ? 0 : sel + 1; sfx_food(); }
    if (emp_ & J_A) { if (t < 100 && !has_t(BD[t].tech)) sfx_deny(); else { r = t; break; } }
  }
  eptab = tab; epsel = sel;
  ewait_clear(); ov_clear();
  return r;
}
static void panel_open(void);
static void research_menu(void) {
  u8 sel = eres == 255 ? 0 : eres, i, top = 0, k;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "RESEARCH");
    ps(1, 2, ERAN[era_now()], 9); ps(14, 2, "\5 BACK", 10);
    if (sel < top) top = sel; if (sel >= top + 11) top = (u8)(sel - 10);
    for (i = 0; i < 11 && top + i < T_N; i++) {
      k = (u8)(top + i); { u8 y = (u8)(4 + i), can = (TREQ[k] & etech) == TREQ[k];
      if (k == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, TNAME[k], has_t(k) ? 13 : !can ? 15 : k == sel ? 9 : 3);
      ps(20, y, has_t(k) ? "DONE" : k == eres ? "NOW" : can ? "" : "LOCKED", has_t(k) ? 13 : k == eres ? 9 : 15); }
    }
    pm_rule(16); ps(1, 17, TDESC[sel], 3);
    if (eres != 255) { ps(1, 18, "PROGRESS", 10); pn(11, 18, (u8)(erp * 99 / tcost()), 9); ps(14, 18, "PERCENT", 10); }
    ps(1, 19, "\1 STUDY THIS", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = sel ? sel - 1 : T_N - 1;
    if (emp_ & J_DOWN) sel = sel + 1 >= T_N ? 0 : sel + 1;
    if (emp_ & J_A) { if (!has_t(sel) && (TREQ[sel] & etech) == TREQ[sel]) { if (eres != sel) erp = 0; eres = sel; sfx_mana(); } else sfx_deny(); }
  }
  ewait_clear();
}
static void edict_menu(void) {
  u8 sel = 0, i;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "EDICTS");
    if (edict != 255) { ps(1, 2, "NOW:", 10); ps(6, 2, EDN[edict], 9); } else ps(1, 2, "ONE EDICT AT A TIME", 10);
    for (i = 0; i < NED; i++) { u8 y = (u8)(4 + i * 1);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, EDN[i], i == sel ? 9 : 3); ps(19, y, EDC[i], 10); }
    pm_rule(13); ps(1, 14, EDD[sel], 3);
    ps(1, 16, "FARM", 13); pn(6, 16, fac[0], 3); ps(9, 16, "BUILD", 2); pn(15, 16, fac[1], 3); ps(18, 16, "WAR", 8); pn(22, 16, fac[2], 3); ps(25, 16, "MYS", 9); pn(28, 16, fac[3], 3);
    ps(1, 19, "\1 DECREE  \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = sel ? sel - 1 : NED - 1;
    if (emp_ & J_DOWN) sel = sel + 1 >= NED ? 0 : sel + 1;
    if (emp_ & J_A) {
      if (edict != 255 && sel != 0 && sel != 4 && sel != 7) { say("AN EDICT IS ALREADY ACTIVE"); sfx_deny(); continue; }
      if (rf < EDF[sel] || ra < EDA[sel] || rd < EDDI[sel]) { sfx_deny(); continue; }
      rf -= EDF[sel]; ra -= EDA[sel]; rd -= EDDI[sel]; sfx_mana();
      if (sel == 0) { apr_f(0, 15); approval_all(5); }
      else if (sel == 3) { apr_f(3, 20); approval_all(5); edict = 3; edt = 6; }
      else if (sel == 4) { gw += 30; apr_f(0, -10); }
      else if (sel == 5) { erig = 1; approval_all(-5); }
      else if (sel == 7) approval_all(8);
      else { edict = sel; edt = sel == 1 ? 8 : 10; if (sel == 2) ach_get(22); }
      say("THE ANTPEROR HAS SPOKEN!");
      break;
    }
  }
  ewait_clear();
}
static void groups_screen(void) {
  u8 i, j;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "THE GROUPS");
  for (i = 0; i < 4; i++) {
    u8 y = (u8)(3 + i * 3);
    ps(2, y, FACN[i], 9); pn(12, y, fac[i], fac[i] < 30 ? 8 : 3); ps(15, y, FACW[i] + 6, 10);
    for (j = 0; j < 10; j++) BGMAP1[(y + 1) * 32 + 2 + j] = (u16)((fac[i] > j * 10 ? BAR_F : BAR_E) | (14 << 12));
  }
  ps(2, 16, "NEXT VOTE IN", 3); pn(15, 16, (u8)(elect_t * 4 / 60), 9); ps(18, 16, "MINUTES", 3);
  ps(2, 17, "AVERAGE", 3); pn(15, 17, fac_avg(), 9); ps(18, 17, fac_avg() >= 50 ? "YOU WIN IT" : "YOU LOSE IT", fac_avg() >= 50 ? 13 : 8);
  ps(1, 19, "\5 BACK", 10);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_B | J_START))) {}
  ewait_clear();
}
static void advisor_screen(void) {
  u8 y = 4;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "COUNSELOR");
  ps(1, 2, "CHITIN SAYS:", 9);
  if (echap != 255) { ps(1, y++, "GOAL:", 9); ps(7, y - 1, CH[echap].goal_txt, 3); y++; }
  else { ps(1, y++, "WIN: BEAT ALL RIVALS, OR", 3); ps(1, y++, "BUILD THE GREAT MOUND, OR", 3); ps(1, y++, "50 ANTS + 70 HAPPY AT A VOTE", 3); y++; }
  if (food_net < 0) ps(1, y++, "WE ARE STARVING! MORE FARMS", 8);
  else if (food_net < 3) ps(1, y++, "FOOD IS TIGHT. BUILD FARMS", 2);
  if (pop + 1 >= popcap) ps(1, y++, "NO ROOM! BUILD CHAMBERS", 2);
  if (unemp > 3) ps(1, y++, "IDLE ANTS: BUILD WORKPLACES", 2);
  if (eres == 255) ps(1, y++, "PICK SOMETHING TO RESEARCH", 2);
  if (fac_avg() < 40) ps(1, y++, "THE COLONY IS UNHAPPY: EDICTS", 8);
  if (enriv && my_troops() < 20) ps(1, y++, "WE NEED SOLDIERS: BARRACKS", 2);
  if (y == (echap != 255 ? 6 : 8)) ps(1, y++, "ALL IS WELL, ANTPEROR!", 13);
  ps(1, 15, "TIP: PAINT ZONES IN THE", 10); ps(1, 16, "LAND TAB. THEY GROW ALONE!", 10);
  ps(1, 19, "\5 BACK", 10);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_B | J_START))) {}
  ewait_clear();
}
// ---------- random events (Tropico style): Chitin tells you what happened ----------
static void story_card(const char *title, const char *const *lines, const char *foot);
static void emp_event(u8 e) {
  static const char *const EVT[10] = {"BEETLE TRADERS", "SPRING RAIN", "DROUGHT", "LOCUST SWARM", "REFUGEES", "GOLDEN DAY", "CAVE IN", "AMBER VEIN", "FUNGUS BLIGHT", "WANDERING WARRIORS"};
  static const char *const EV1[10] = {"BEETLES CAME TO TRADE.", "THE RAINS ARE HERE!", "NO RAIN FOR WEEKS.", "LOCUSTS ATE OUR STORES!", "ANTS FROM A FLOODED NEST", "THE SUN SHINES, THE FOOD", "A TUNNEL COLLAPSED UNDER", "OUR DIGGERS STRUCK AMBER!", "A GREY ROT IS ON THE", "A BAND OF LOST SOLDIERS"};
  static const char *const EV2[10] = {"WE SOLD FOOD FOR AMBER.", "FARMS GROW DOUBLE A WHILE.", "FARMS GROW HALF A WHILE.", "WE LOST A THIRD OF FOOD.", "JOINED US: +5 ANTS!", "IS SWEET: EVERYONE +10.", "ONE OF OUR BUILDINGS.", "+30 AMBER.", "FARMS: FOOD AND FARMERS -.", "JOINED OUR GUARD: +25!"};
  const char *L[6] = {EV1[e], EV2[e], 0, 0, 0, 0};
  if (cht[2] == 1 && (e == 2 || e == 3 || e == 6 || e == 8)) e = 7;   // WEATHER CALM: only good news
  if (cht[2] == 2 && (e == 1 || e == 5 || e == 7)) e = (u8)(e == 1 ? 2 : e == 5 ? 3 : 8);   // WEATHER CHAOS: only bad news
  L[0] = EV1[e]; L[1] = EV2[e];
  switch (e) {
  case 0: if (rf >= 30) { rf -= 30; radd(&ra, 25); } else radd(&ra, 10); break;
  case 1: efarm_mod = 1; efarm_t = 8; break;
  case 2: efarm_mod = -1; efarm_t = 8; break;
  case 3: rf = (u16)(rf * 2 / 3); break;
  case 4: pop = (u8)(pop + 5 > 250 ? 250 : pop + 5); break;
  case 5: approval_all(10); break;
  case 6: { u8 i; for (i = 0; i < nbl; i++) if (ebl[(u8)((i + rnd()) % nbl)].t != B_NEST) { EB *b = &ebl[(u8)((i + rnd()) % nbl)]; if (b->t != B_NEST) b->hp = (u8)(b->hp / 2 + 1); break; } } break;
  case 7: radd(&ra, 30); break;
  case 8: rf = rf > 15 ? rf - 15 : 0; apr_f(0, -10); break;
  default: gw += 25; break;
  }
  music_stop(); fade(0, 16); DISPCNT = 0x80;
  story_card(EVT[e], L, "\1 CONTINUE");
}
// ---------- diplomacy (Civ): peace, tribute, war ----------
static void rivals_screen(void) {
  u8 sel = 0, act = 0, i;
  static const char *const ACT[3] = {"OFFER PEACE  ]20", "DEMAND TRIBUTE", "DECLARE WAR"};
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "RIVALS");
    if (!enriv) { ps(3, 4, "NO RIVALS ON THIS MAP", 3); }
    for (i = 0; i < enriv; i++) { u8 y = (u8)(3 + i * 4);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, RNAME[i], ralive[i] ? (i == sel ? 9 : 3) : 15);
      ps(11, y, !ralive[i] ? "GONE" : rpeace[i] ? "AT PEACE" : "AT WAR", !ralive[i] ? 15 : rpeace[i] ? 13 : 8);
      if (ralive[i]) { ps(3, (u8)(y + 1), "GUARDS", 10); p3(10, (u8)(y + 1), rgar[i], 3); ps(15, (u8)(y + 1), "NEST", 10); p3(20, (u8)(y + 1), rhp[i], 3);
        if (rpeace[i]) { ps(3, (u8)(y + 2), "PEACE FOR", 10); p3(13, (u8)(y + 2), (u16)(rpeace[i] * 4 / 60 + 1), 3); ps(17, (u8)(y + 2), "MIN", 10); } }
    }
    pm_rule(15);
    ps(1, 16, "YOUR TROOPS", 10); p3(13, 16, my_troops() > 999 ? 999 : my_troops(), 9);
    ps(1, 17, "\3\4", 1); ps(4, 17, ACT[act], 9);
    ps(1, 19, "\1 DO IT  UP DOWN WHO  \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (enriv && (emp_ & J_UP)) sel = sel ? sel - 1 : enriv - 1;
    if (enriv && (emp_ & J_DOWN)) sel = sel + 1 >= enriv ? 0 : sel + 1;
    if (emp_ & (J_L | J_LEFT)) act = (u8)((act + 2) % 3);
    if (emp_ & (J_R | J_RIGHT)) act = (u8)((act + 1) % 3);
    if ((emp_ & J_A) && enriv && ralive[sel]) {
      if (act == 0) {
        if (rpeace[sel]) { say("ALREADY AT PEACE"); continue; }
        if (ra < 20) { sfx_deny(); continue; }
        ra -= 20;
        if ((u32)(my_troops() + 20) * 100 / (rgar[sel] + 20) >= 50 || (rnd() & 3) == 0) {
          u8 k; rpeace[sel] = 45; for (k = 0; k < EAN; k++) if (ea[k].alive && ea[k].team == sel + 1) ea[k].alive = 0;
          say("PEACE! THEIR ARMIES GO HOME"); sfx_mana(); ach_get(26);
        } else { say("THEY REFUSED. AMBER LOST"); sfx_deny(); }
      } else if (act == 1) {
        if (my_troops() > rgar[sel] * 2 + 20) { radd(&rf, 30); radd(&ra, 20); rwt[sel] = 3; say("THEY PAID: +30 FOOD +20 AMBER"); sfx_mana(); }
        else { if (rwt[sel] > 3) rwt[sel] -= 2; say("THEY LAUGHED AT US"); sfx_deny(); }
      } else { rpeace[sel] = 0; apr_f(2, 10); say("WAR! THE WARRIORS CHEER"); sfx_alarm(); }
    }
  }
  ewait_clear();
}
// ---------- world map: the whole land on one screen (video mode 4) ----------
static void world_map(void) {
  volatile u16 *pg = (volatile u16 *)0x06000000; u8 x, y, sc, i, c; int ox, oy, a, b;
  static const u16 WC[16] = {0, RGB(6,13,26), RGB(28,24,14), RGB(9,20,7), RGB(18,13,8), RGB(31,26,6), RGB(31,31,31), RGB(31,4,4), RGB(31,28,6), RGB(24,8,31),
    RGB(2,1,0), RGB(20,24,31), RGB(14,30,8), 0, 0, 0};
  sc = (u8)(240 / W < 136 / H ? 240 / W : 136 / H); if (!sc) sc = 1;
  ox = (240 - W * sc) / 2; oy = 16 + (136 - H * sc) / 2;
  music_stop(); fade(0, 16); hide_all(); oam_flush();
  DISPCNT = 0x0404 | 0x80;
  for (i = 0; i < 16; i++) BGPAL[i] = WC[i];
  for (a = 0; a < 240 * 160 / 2; a++) pg[a] = 0x0A0A;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    c = (u8)(hgt[y][x] + 1);
    if (ETER(y, x) == 1 && ter_edge(x, y, 1)) c = 11;
    if (ebi[y][x]) c = 5;
    for (b = 0; b < sc; b++) for (a = 0; a < sc; a++) vplot(pg, ox + x * sc + a, oy + y * sc + b, c);
  }
  for (i = 0; i < enriv; i++) if (ralive[i]) for (b = -1; b < sc + 1; b++) for (a = -1; a < sc + 1; a++) vplot(pg, ox + rx[i] * sc + a, oy + ry[i] * sc + b, (u8)(7 + i));
  for (b = -1; b < sc + 1; b++) for (a = -1; a < sc + 1; a++) vplot(pg, ox + nestx[0] * sc + a, oy + nesty[0] * sc + b, 6);
  for (i = 0; i < EAN; i++) if (ea[i].alive) for (b = 0; b < sc; b++) for (a = 0; a < sc; a++) vplot(pg, ox + ea[i].x * sc + a, oy + ea[i].y * sc + b, ea[i].team ? (u8)(6 + ea[i].team) : 12);
  for (b = 0; b < sc; b++) for (a = 0; a < sc; a++) if (a == 0 || b == 0 || a == sc - 1 || b == sc - 1) vplot(pg, ox + cx * sc + a, oy + cy * sc + b, 6);
  { const char *t = "WORLD MAP"; u8 gx, gy, g; int tx = 84; for (; *t; t++, tx += 8) { g = gi(*t); for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) vplot(pg, tx + gx, 4 + gy, 5); } }
  { const char *t = "WHITE: YOU  GREEN: ARMY  A: BACK"; u8 gx, gy, g; int tx = 0; for (; *t; t++, tx += 7) { g = gi(*t); for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) vplot(pg, tx + gx + 8, 152 + gy, 6); } }
  DISPCNT = 0x0404; fade(16, 0);
  ewait_clear(); while (!(joy() & (J_A | J_B | J_START))) vsync(); ewait_clear();
  fade(0, 16); DISPCNT = 0x80;
  vram_reload();
}
static void emp_save(void);
static u8 emp_menu(void) {                         // START: 0 resume, 1 quit
  static const char *const IT[10] = {"RESUME", "RESEARCH", "EDICTS", "GROUPS AND VOTES", "COUNSELOR", "SPEED", "SAVE AND QUIT", "QUIT", "RIVALS", "WORLD MAP"};
  static const u8 ORD[10] = {0, 9, 1, 2, 8, 3, 4, 5, 6, 7};
  u8 sel = 0, i, r = 255;
  hide_all(); vsync(); oam_flush(); ewait_clear();
  while (r == 255) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "EMPIRE");
    ps(1, 2, ERAN[era_now()], 9); ps(16, 2, echap == 255 ? "FREE PLAY" : "STORY", 10);
    for (i = 0; i < 10; i++) { u8 y = (u8)(4 + i * 1), it = ORD[i];
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, (it == 6 && echap != 255) ? "QUIT (STORY: NO SAVE)" : IT[it], i == sel ? 9 : 3);
      if (it == 5) ps(20, y, SPNAME[spd], 9); }
    pm_rule(15);
    if (echap != 255) { ps(1, 16, CH[echap].name, 9); ps(1, 17, CH[echap].goal_txt, 3); }
    ps(1, 19, "\1 OK   \5 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) r = 0;
    if (emp_ & J_UP) sel = sel ? sel - 1 : 9;
    if (emp_ & J_DOWN) sel = sel >= 9 ? 0 : sel + 1;
    if (ORD[sel] == 5 && (emp_ & (J_LEFT | J_RIGHT))) spd_next((emp_ & J_LEFT) ? -1 : 1);
    if (emp_ & J_A) {
      u8 it = ORD[sel];
      if (it == 0) r = 0; else if (it == 1) research_menu(); else if (it == 2) edict_menu(); else if (it == 3) groups_screen();
      else if (it == 4) advisor_screen(); else if (it == 5) spd_next(1);
      else if (it == 6) { if (echap == 255) emp_save(); r = 1; }
      else if (it == 8) rivals_screen();
      else if (it == 9) { world_map(); r = 0; }
      else r = 1;
    }
  }
  ewait_clear(); ov_clear();
  return r;
}
static void panel_open(void) {                     // \1 on a building or an army
  u8 i, sel = 0, n = 0, act[6]; const char *lab[6]; EB *b = 0; EA *a = 0;
  if (ebi[cy][cx]) b = &ebl[ebi[cy][cx] - 1];
  for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && ea[i].x == cx && ea[i].y == cy) a = &ea[i];
  if (a && !b) {
    lab[n] = "MARCH: PICK A PLACE"; act[n++] = 1; lab[n] = "ATTACK NEAREST RIVAL"; act[n++] = 2;
    lab[n] = "GO HOME"; act[n++] = 3; lab[n] = "HOLD HERE"; act[n++] = 4;
  } else if (b) {
    if (b->t == B_NEST) { lab[n] = "FORM AN ARMY"; act[n++] = 10; lab[n] = "OPEN THE BUILD MENU"; act[n++] = 11; }
    if (b->t == B_BARR) { lab[n] = "TRAIN 10 WARRIORS"; act[n++] = 20; if (nb_of[B_SPIT]) { lab[n] = "TRAIN 10 SPITTERS"; act[n++] = 21; } if (has_t(T_SHELL)) { lab[n] = "TRAIN 5 MAJORS"; act[n++] = 22; } }
    if (b->t != B_NEST) { lab[n] = "DEMOLISH (HALF BACK)"; act[n++] = 30; }
  } else return;
  hide_all(); vsync(); oam_flush(); ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full();
    if (b) { big(0, b->t == B_NEST ? "NEST" : "BUILDING"); ps(1, 2, BD[b->t].nm, 9); ps(1, 3, BD[b->t].d1, 3); ps(1, 4, BD[b->t].d2, 3);
      ps(1, 5, "HEALTH", 10); pn(8, 5, b->hp, 3); if (BD[b->t].jobs) { ps(12, 5, "WORKERS", 10); pc(20, 5, (char)('0' + b->staff), 3); pc(21, 5, '/', 10); pc(22, 5, (char)('0' + BD[b->t].jobs), 3); }
      if (b->t == B_NEST || b->t == B_BARR) { ps(1, 6, "GUARDS AT HOME", 10); p3(16, 6, (u16)(gw > 999 ? 999 : gw), 3); p3(20, 6, gs, 13); p3(24, 6, gm, 9); }
      if (b->t == B_WONDER) { ps(1, 6, "BUILT", 10); pn(8, 6, (u8)(wprog * 99 / 40), 9); ps(11, 6, "PERCENT", 10); } }
    else { big(0, "YOUR ARMY"); ps(1, 2, "WARRIORS", 3); p3(12, 2, a->w, 9); ps(1, 3, "SPITTERS", 3); p3(12, 3, a->s, 13); ps(1, 4, "MAJORS", 3); p3(12, 4, a->m, 8); }
    pm_rule(7);
    for (i = 0; i < n; i++) { u8 y = (u8)(9 + i * 2); if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, y, lab[i], i == sel ? 9 : 3); }
    if (act[sel] == 20) ps(1, 18, "COST 10 FOOD 5 DIRT", 10); else if (act[sel] == 21) ps(1, 18, "COST 5 FOOD 5 DIRT 5 AMBER", 10);
    else if (act[sel] == 22) ps(1, 18, "COST 10 FOOD 10 DIRT 10 AMBER", 10); else if (act[sel] == 10) ps(1, 18, "ALL GUARDS LEAVE AS AN ARMY", 10);
    ps(1, 19, "\1 DO IT   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = sel ? sel - 1 : n - 1;
    if (emp_ & J_DOWN) sel = sel + 1 >= n ? 0 : sel + 1;
    if (emp_ & J_A) {
      u8 c = act[sel];
      if (c == 1) { etgt = 1; etgt_a = (u8)(a - ea); break; }
      if (c == 2) { u8 j, bj = 255, bd = 255; for (j = 0; j < enriv; j++) if (ralive[j]) { u8 dd = (u8)(dist(a->x, rx[j]) + dist(a->y, ry[j])); if (dd < bd) { bd = dd; bj = j; } }
        if (bj == 255) { say("NO RIVALS LEFT"); break; } efield(rx[bj], ry[bj]); a->ord = 3; for (j = 0; j < EAN; j++) if (ea[j].alive && !ea[j].team && ea[j].ord == 1) ea[j].ord = 3; say("TO WAR!"); sfx_alarm(); break; }
      if (c == 3) { a->ord = 2; say("GOING HOME"); break; }
      if (c == 4) { a->ord = 0; break; }
      if (c == 10) { if (!(gw + gs + gm)) { say("NO GUARDS TO SEND"); sfx_deny(); break; }
        if (new_army(0, nestx[0], nesty[0], gw, gs, gm)) { gw = gs = gm = 0; say("ARMY FORMED! \1 ON IT: ORDERS"); sfx_spawn(); } else say("TOO MANY ARMIES"); break; }
      if (c == 11) { u8 t = build_menu(); if (t) eplace = t; break; }
      if (c == 20) { if (rf >= 10 && rd >= 5) { rf -= 10; rd -= 5; gw += 10; sfx_spawn(); } else sfx_deny(); }
      if (c == 21) { if (rf >= 5 && rd >= 5 && ra >= 5) { rf -= 5; rd -= 5; ra -= 5; gs += 10; sfx_spawn(); } else sfx_deny(); }
      if (c == 22) { if (rf >= 10 && rd >= 10 && ra >= 10) { rf -= 10; rd -= 10; ra -= 10; gm += 5; sfx_spawn(); } else sfx_deny(); }
      if (c == 30) { radd(&rf, BD[b->t].f / 2); radd(&rd, BD[b->t].d / 2); radd(&ra, BD[b->t].a / 2); del_b((u8)(b - ebl)); terr_calc(); eredraw(cx, cy, 1); break; }
    }
  }
  ewait_clear(); ov_clear();
}

// ---------- story cards ----------
static void story_card(const char *title, const char *const *lines, const char *foot) {
  u8 i, k; u16 fr = 0;
  hide_all(); vsync(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  big(1, title[0] ? title : "CHITIN");
  pm_rule(3);
  for (i = 0; i < 6 && lines[i]; i++) ps(2, (u8)(5 + i * 2), lines[i], 3);
  ps(1, 18, foot, 10);
  // Counselor Chitin (a big black ant) watches from the corner
  spr(0, 196, 118, BIGT, 0, 0, 1); oam[1] |= 0x8000;
  DISPCNT = 0x1F40; BLDY = 0;
  ewait_clear();
  for (;;) { vsync(); oam_flush(); fr++; k = (u8)((fr >> 3) & 1); oam[0] = (u16)((118 + k) & 0xFF); if (joy() & (J_A | J_START)) break; }
  ewait_clear(); hide_all(); oam_flush();
}

// ---------- start a game ----------
static void place_rivals(void) {
  static const u8 CX[3] = {1, 1, 0}, CY[3] = {0, 1, 0};
  u8 i, k, x, y;
  for (i = 0; i < enriv; i++) {
    if (i == 0) { x = nestx[1]; y = nesty[1]; }
    else { x = CX[i] ? (u8)(W - 6) : 5; y = CY[i] ? (u8)(H - 6) : 5; if (i == 2) { x = 5; y = 5; } }
    for (k = 0; k < 30 && !hgt[y][x]; k++) { x = (u8)(x + (rnd() % 3) - 1); y = (u8)(y + (rnd() % 3) - 1); if (x >= W) x = (u8)(W - 2); if (y >= H) y = (u8)(H - 2); }
    if (!hgt[y][x]) hgt[y][x] = 2;
    rx[i] = x; ry[i] = y; rhp[i] = (u8)(40 + 15 * eaggr); ralive[i] = 1; rgar[i] = (u16)(10 + 10 * eaggr); rwave[i] = 0;
    rwt[i] = (u8)(32 + 10 * (2 - eaggr) + 5 * i);  // a few minutes of peace to build first
    path(nestx[0], nesty[0], x, y);
  }
}
static void emp_setup_common(void) {
  u8 i, x, y;
  emode = 1; nbl = 0; eend = 0; rpeace[0] = rpeace[1] = rpeace[2] = 0; efarm_t = 0; efarm_mod = 0; ezbuilt = 0; ebeat = 0; evpend = 255; eplace = 0; etgt = 0; ezoom = 0; evote_ok = 0; erig = 0; edict = 255; lowhap = 0; ewv = 0; wprog = 0;
  for (i = 0; i < EAN; i++) ea[i].alive = 0;
  for (i = 0; i < EWN; i++) ew[i].alive = 0;
  for (i = 0; i < MAXA; i++) ant[i].alive = 0;
  for (i = 0; i < 8; i++) fx[i].t = 0;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { ebi[y][x] = 0; ezt[y][x] = 0; food[y][x] = 0; ph[y][x] = 0; }
  for (i = 0; i < 4; i++) fac[i] = 55;
  elect_t = 45; gt = 0; ecyc = 0; eslice = 0; spd = 4; sf = 0; msgt = 0; alt = 0; shk = 0; shx = 0;
  for (i = 0; i < 16; i++) { OBJPAL[7 * 16 + i] = PAL_ANT1[i]; OBJPAL[8 * 16 + i] = PAL_ANT1[i]; }
  OBJPAL[7 * 16 + 1] = RGB(10, 7, 0); OBJPAL[7 * 16 + 2] = RGB(27, 22, 3); OBJPAL[7 * 16 + 3] = RGB(31, 30, 14);   // gold rivals
  OBJPAL[8 * 16 + 1] = RGB(7, 1, 9); OBJPAL[8 * 16 + 2] = RGB(19, 7, 26); OBJPAL[8 * 16 + 3] = RGB(28, 18, 31);   // violet rivals
}
static void emp_show(void) {
  cx = nestx[0]; cy = (u8)(nesty[0] > 2 ? nesty[0] - 2 : nesty[0]);
  camx = 0; camy = (u8)(H - VH); follow(); scx = (s16)camx * 8; scy = (s16)camy * 8;
  win_all(); terr_calc(); ov_clear(); ehud();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; BG2CNT = 1 | (2 << 2) | (30 << 8);
  hide_all(); edraw_sprites(); oam_flush();
  DISPCNT = 0x1F40; BLDY = 16; music_start(); fade(16, 0);
}
static void emp_newgame(void) {
  u8 i;
  { u8 sm = mapt, sw = tw, sh = th;                // story chapters pick their own land
    if (echap != 255) { mapt = CH[echap].map; tw = CH[echap].w; th = CH[echap].h; }
    DISPCNT = 0x80; BLDY = 0; ov_fill(1, 1, 0, 19); ui_full(); ps(5, 8, "BUILDING THE WORLD...", 2); vsync(); DISPCNT = 0x0A00;
    gen_world();
    mapt = sm; tw = sw; th = sh; }
  emp_setup_common();
  if (echap != 255) { enriv = CH[echap].nriv; eaggr = CH[echap].aggr; etech = CH[echap].techs; rf = CH[echap].f; rd = CH[echap].d; ra = CH[echap].a; edry = echap == 1; }
  else { etech = (1 << T_FUNGUS) | (1 << T_DIG) | (enriv ? (1 << T_SOLD) : 0); rf = 50; rd = 70; ra = 0; edry = 0; }
  pop = 6; popcap = 8; gw = enriv ? 20 : 0; gs = gm = 0; eres = 255; erp = 0;
  if (cht[4]) etech = 0xFFF;
  if (cht[5]) { u16 v = MEGAV[cht[5]]; gw = (u16)(v * 4 / 5); gs = (u16)(v * 3 / 25); gm = (u16)(v - gw - gs); }
  if (cht[3]) rf = rd = ra = RICHV[cht[3]];
  era = era_now();
  place_rivals();
  bfs(0); bfs(1); pdirty = 0;
  add_b(nestx[0], nesty[0], B_NEST);
  for (i = 0; i < enriv; i++) path(nestx[0], nesty[0], rx[i], ry[i]);
  bfs(0); pdirty = 0;
  efield(nestx[0], nesty[0]);
  terr_calc();
}

// ---------- save / continue (free play) ----------
#define ESO 9600
static void sb(u16 *o, u8 v) { SRAM[(*o)++] = v; }
static void sw16(u16 *o, u16 v) { sb(o, (u8)v); sb(o, (u8)(v >> 8)); }
static u8 lb(u16 *o) { return SRAM[(*o)++]; }
static u16 lw16(u16 *o) { u16 v = lb(o); return (u16)(v | (lb(o) << 8)); }
static void emp_save(void) {
  u16 o = ESO + 4; u8 x, y, i;
  sb(&o, mw); sb(&o, mh); sb(&o, nestx[0]); sb(&o, nesty[0]); sb(&o, enriv); sb(&o, eaggr); sb(&o, diff);
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) sb(&o, (u8)(hgt[y][x] | (EZN(y, x) << 2)));
  sb(&o, nbl); for (i = 0; i < nbl; i++) { sb(&o, ebl[i].x); sb(&o, ebl[i].y); sb(&o, ebl[i].t); sb(&o, ebl[i].hp); }
  for (i = 0; i < 3; i++) { sb(&o, rx[i]); sb(&o, ry[i]); sb(&o, rhp[i]); sb(&o, ralive[i]); sb(&o, rwt[i]); sb(&o, rwave[i]); sw16(&o, rgar[i]); }
  for (i = 0; i < EAN; i++) { EA *a = &ea[i]; sb(&o, a->alive); sb(&o, a->x); sb(&o, a->y); sb(&o, a->team); sb(&o, a->ord); sw16(&o, a->w); sw16(&o, a->s); sw16(&o, a->m); }
  sw16(&o, rf); sw16(&o, rd); sw16(&o, ra); sb(&o, pop); sw16(&o, etech); sb(&o, eres); sw16(&o, erp);
  for (i = 0; i < 4; i++) sb(&o, fac[i]);
  sb(&o, edict); sb(&o, edt); sw16(&o, ecyc); sb(&o, elect_t); sw16(&o, wprog); sw16(&o, gw); sw16(&o, gs); sw16(&o, gm); sw16(&o, ewv);
  for (i = 0; i < 3; i++) sb(&o, rpeace[i]);
  SRAM[ESO] = 'E'; SRAM[ESO + 1] = 'S'; SRAM[ESO + 2] = 'A'; SRAM[ESO + 3] = 'V';
  say("SAVED");
}
static u8 emp_has_save(void) { return SRAM[ESO] == 'E' && SRAM[ESO + 1] == 'S' && SRAM[ESO + 2] == 'A' && SRAM[ESO + 3] == 'V'; }
static void emp_load(void) {
  u16 o = ESO + 4; u8 x, y, i, n, v;
  mw = lb(&o); mh = lb(&o); nestx[0] = lb(&o); nesty[0] = lb(&o); enriv = lb(&o); eaggr = lb(&o); diff = lb(&o);
  emp_setup_common(); echap = 255; edry = 0;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { v = lb(&o); hgt[y][x] = v & 3; set_zn(y, x, (u8)((v >> 2) & 3)); }
  n = lb(&o); nbl = 0;
  for (i = 0; i < n; i++) { u8 bx = lb(&o), by = lb(&o), bt = lb(&o), bh = lb(&o); add_b(bx, by, bt); ebl[nbl - 1].hp = bh; }
  for (i = 0; i < 3; i++) { rx[i] = lb(&o); ry[i] = lb(&o); rhp[i] = lb(&o); ralive[i] = lb(&o); rwt[i] = lb(&o); rwave[i] = lb(&o); rgar[i] = lw16(&o); }
  nestx[1] = rx[0]; nesty[1] = ry[0];
  for (i = 0; i < EAN; i++) { EA *a = &ea[i]; a->alive = lb(&o); a->x = a->ox = lb(&o); a->y = a->oy = lb(&o); a->team = lb(&o); a->ord = lb(&o); a->w = lw16(&o); a->s = lw16(&o); a->m = lw16(&o); a->peak = a_size(a); a->age = 8; a->foe = 255; }
  rf = lw16(&o); rd = lw16(&o); ra = lw16(&o); pop = lb(&o); etech = lw16(&o); eres = lb(&o); erp = lw16(&o);
  for (i = 0; i < 4; i++) fac[i] = lb(&o);
  edict = lb(&o); edt = lb(&o); ecyc = lw16(&o); elect_t = lb(&o); wprog = lw16(&o); gw = lw16(&o); gs = lw16(&o); gm = lw16(&o); ewv = lw16(&o);
  for (i = 0; i < 3; i++) rpeace[i] = lb(&o);
  era = era_now(); popcap = (u8)(8 + 5 * nb_of[B_HOUSE]);
  bfs(0); bfs(1); pdirty = 0; terr_calc();
  for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && (ea[i].ord == 1 || ea[i].ord == 3)) ea[i].ord = 0;
}

// ---------- the empire game loop ----------
static void emp_play(void) {
  u16 k, p, prev = 0, last = 0; u8 dirs, rep = 0, fire, t = 0, n, mx, my, selused = 0, selprev = 0;
  emp_show();
  if (echap != 255) say(CH[echap].goal_txt); else say("\1 BUILD  \5 MENU  \3 ZOOM");
  while (!eend) {
    vsync(); frc++;
    shx = (shk && !O(O_SHAKE)) ? ((shk & 2) ? 2 : -2) : 0; if (shk) shk--;
    BG0HOFS = BG2HOFS = (u16)(scx + shx); BG0VOFS = BG2VOFS = (u16)scy; oam_flush();
    BGPAL[176 + 3] = (frc & 32) ? RGB(14,22,31) : RGB(10,18,30); BGPAL[176 + 14] = (frc & 32) ? RGB(10,18,30) : RGB(14,22,31);
    OBJPAL[32 + 3] = RGB(31, 26 + ((frc >> 2) & 3), (frc & 16) ? 4 : 12);
    party_tick();
    if (alt) alt--;
    if (evpend != 255) { emp_event(evpend); evpend = 255; emp_show(); prev = joy(); continue; }
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
    if (k & J_SEL) { if (p & J_SEL) selused = 0; if (k & ~J_SEL) selused = 1; }
    else if (selprev && !selused) {                // SELECT: jump to alert, home, your army, rival nest
      if (alt) { cx = alx; cy = aly; alt = 0; }
      else { static u8 jj; u8 i; jj = (u8)((jj + 1) % 3);
        if (jj == 0) { cx = nestx[0]; cy = nesty[0]; say("YOUR NEST"); }
        else if (jj == 1) { for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team) { cx = ea[i].x; cy = ea[i].y; say("YOUR ARMY"); break; } if (i == EAN) jj = 2; }
        if (jj == 2) { for (i = 0; i < enriv; i++) if (ralive[i]) { cx = rx[i]; cy = ry[i]; say("RIVAL NEST"); break; } } }
      sfx_spawn();
    }
    selprev = (u8)(k & J_SEL);
    if (p & J_START) { if (eplace || etgt) { eplace = 0; etgt = 0; } else if (emp_menu()) { eend = 3; break; } emp_show(); prev = joy(); continue; }
    if (etgt) {
      if (p & J_A) { EA *a = &ea[etgt_a]; u8 j; efield(cx, cy); if (a->alive) a->ord = 1; for (j = 0; j < EAN; j++) if (ea[j].alive && !ea[j].team && ea[j].ord == 3) ea[j].ord = 1; etgt = 0; say("MARCHING!"); sfx_alarm(); }
      if (p & J_B) etgt = 0;
    } else if (eplace) {
      if ((p & J_A) || ((k & J_A) && (mx != cx || my != cy) && eplace >= L_ZH)) do_build(eplace, cx, cy);
      if (p & J_B) eplace = 0;
    } else {
      if (p & J_A) {
        u8 i, on = ebi[cy][cx] != 0;
        for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && ea[i].x == cx && ea[i].y == cy) on = 1;
        if (on) { panel_open(); emp_show(); prev = joy(); continue; }
        if (ETER(cy, cx) == 1) { u8 b = build_menu(); if (b) eplace = b; emp_show(); prev = joy(); continue; }
        say("NOT YOUR LAND"); sfx_deny();
      }
      if (p & J_B) { say("\1 BUILD  \5 MENU  \6 JUMP"); }
    }
    if (p & J_L) {                                 // zoom into the nearest fight: the pixel battle
      u8 i, bi = 255, bd = 255;
      for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && ea[i].foe != 255) { u8 dd = (u8)(dist(ea[i].x, cx) + dist(ea[i].y, cy)); if (dd < bd) { bd = dd; bi = i; } }
      if (bi != 255 && ea[ea[bi].foe].alive) { EA *e = &ea[ea[bi].foe]; zoom_battle(&ea[bi], &e->w, &e->s, &e->m, (u8)(e->team - 1), e->x, e->y);
        if (!a_size(e)) e->alive = 0; if (!a_size(&ea[bi])) ea[bi].alive = 0; ea[bi].foe = 255; e->foe = 255; prev = joy(); continue; }
      { u8 j; for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].team && dnp[0][ea[i].y][ea[i].x] <= 1 && (gw + gs + gm)) {   // a siege at your nest
          EA *e = &ea[i]; zoom_battle(0, &e->w, &e->s, &e->m, (u8)(e->team - 1), nestx[0], nesty[0]); if (!a_size(e)) e->alive = 0; prev = joy(); goto cont; }
        for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team) for (j = 0; j < enriv; j++) if (ralive[j] && rgar[j] && dist(ea[i].x, rx[j]) + dist(ea[i].y, ry[j]) <= 1) {   // storming a rival nest
          u16 w2 = (u16)(rgar[j] * 7 / 10), s2 = (u16)(rgar[j] / 5), m2 = (u16)(rgar[j] - w2 - s2);
          zoom_battle(&ea[i], &w2, &s2, &m2, j, rx[j], ry[j]); rgar[j] = (u16)(w2 + s2 + m2); if (!a_size(&ea[i])) ea[i].alive = 0; prev = joy(); goto cont; } }
#ifdef BATTLETEST
      { u16 w2 = 420, s2 = 50, m2 = 30; gw = 400; gs = 60; gm = 40; zoom_battle(0, &w2, &s2, &m2, 0, cx, cy); prev = joy(); goto cont; }
#endif
      say("NO BATTLE NOW");
    }
cont:
    if (p & J_R) spd_toggle();
    follow(); scroll_step(); win_sync();
    bfs_tick(450);
    n = SPN[spd]; if (n && (sf++ & SPM[spd])) n = 0;
    for (; n; n--) eslice_run();
    if (++t >= 8 || p || mx != cx || my != cy) { t = 0; ehud(); }
    edraw_sprites();
  }
  music_stop();
}

// ---------- end screens ----------
static void emp_end(void) {
  static const char *const WHY_L[3] = {"YOUR NEST WAS DESTROYED", "THE COLONY ROSE AGAINST YOU", ""};
  static const char *const WHY_W[5] = {"", "", "THE GREAT MOUND IS COMPLETE", "ALL RIVALS ARE BEATEN", "A PROSPEROUS, HAPPY COLONY"};
  if (eend == 3) { emode = 0; fade(0, 16); return; }
  hide_all(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  if (eend == 1) {
    big(2, "VICTORY!");
    if (echap != 255) { ps(3, 5, CH[echap].outro, 3); if (SRAM[63] < echap + 1 && echap + 1 <= NCH) SRAM[63] = (u8)(echap + 1); }
    else ps(3, 5, WHY_W[ewhy < 5 ? ewhy : 2], 3);
    jingle(WIN_TUNE, 6);
  } else { big(2, "DEFEAT"); ps(3, 5, WHY_L[ewhy < 3 ? ewhy : 0], 3); jingle(LOSE_TUNE, 4); }
  pm_rule(7);
  ps(3, 9, "AGE", 10); ps(12, 9, ERAN[era_now()], 3);
  ps(3, 10, "ANTS", 10); p3(12, 10, pop, 3);
  ps(3, 11, "TROOPS", 10); p3(12, 11, my_troops() > 999 ? 999 : my_troops(), 3);
  ps(3, 12, "BUILDINGS", 10); p3(12, 12, nbl, 3);
  ps(3, 13, "MINUTES", 10); p3(12, 13, (u16)(ecyc * 4 / 60), 3);
  pm_rule(15); ps(7, 17, "PRESS \5 START", 9);
  ewait_clear(); while (!(joy() & (J_START | J_A))) vsync(); ewait_clear();
  emode = 0; fade(0, 16);
}

// ---------- EMPIRE MODE main menu (title: L) ----------
static u8 emp_unlocked(void) { return SRAM[62] == 'U' || (u16)wins[0] + wins[1] + wins[2] >= 1; }
static void empire_main(void) {
  u8 sel = 0, i, nopt, nriv_sel = 1, agg_sel = 1;
  static const char *const AGN[3] = {"CALM", "NORMAL", "BRUTAL"};
  if (SRAM[63] > NCH) SRAM[63] = 0;
  for (;;) {
    DISPCNT = 0x80;
    ov_fill(1, 3, 0, 19); ui_full();
    big(1, "EMPIRE MODE");
    ps(2, 3, "BUILD  RESEARCH  RULE  WAR", 10);
    pm_rule(4);
    nopt = emp_has_save() ? 6 : 5;
    {
      const char *it[6] = {"STORY", "FREE PLAY", "RIVALS", "RIVALS ARE", "BACK", "CONTINUE SAVED GAME"};
      u8 ord[6] = {0, 1, 2, 3, 5, 4}, k;
      if (nopt == 5) { ord[4] = 4; }
      for (i = 0; i < nopt; i++) { u8 y = (u8)(6 + i * 2); k = ord[i];
        if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        ps(3, y, it[k], i == sel ? 9 : 3);
        if (k == 0) { ps(18, y, "CHAPTER", 10); pc(26, y, (char)('1' + (SRAM[63] < NCH ? SRAM[63] : NCH - 1)), 9); }
        if (k == 2) { pc(18, y, '<', 10); pc(20, y, (char)('0' + nriv_sel), 9); pc(22, y, '>', 10); }
        if (k == 3) { pc(18, y, '<', 10); ps(20, y, AGN[agg_sel], 9); pc(27, y, '>', 10); }
      }
      ps(1, 18, "MAP AND SIZE: TITLE SCREEN", 10);
      ps(1, 19, "\1 OK  LEFT RIGHT SET  \2 BACK", 10);
      DISPCNT = 0x1B40; BLDY = 0;
      while (!ekey()) {}
      if (emp_ & J_B) break;
      if (emp_ & J_UP) sel = sel ? sel - 1 : nopt - 1;
      if (emp_ & J_DOWN) sel = sel + 1 >= nopt ? 0 : sel + 1;
      k = ord[sel];
      if (k == 2 && (emp_ & (J_LEFT | J_RIGHT))) nriv_sel = (u8)((nriv_sel + ((emp_ & J_RIGHT) ? 1 : 3)) % 4);
      if (k == 3 && (emp_ & (J_LEFT | J_RIGHT))) agg_sel = (u8)((agg_sel + ((emp_ & J_RIGHT) ? 1 : 2)) % 3);
      if (emp_ & J_A) {
        if (k == 4) break;
        if (k == 0) {                              // STORY: pick an unlocked chapter
          u8 c = (u8)(SRAM[63] < NCH ? SRAM[63] : NCH - 1), cs = c;
          ewait_clear();
          for (;;) {
            ov_fill(1, 3, 0, 19); ui_full(); big(0, "STORY");
            for (i = 0; i < NCH; i++) { u8 y = (u8)(3 + i * 2);
              if (i == cs) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
              pc(3, y, (char)('1' + i), i <= c ? 9 : 15); ps(5, y, i <= c ? CH[i].name : "LOCKED", i <= c ? (i == cs ? 9 : 3) : 15); }
            ps(1, 19, "\1 PLAY  \2 BACK", 10);
            while (!ekey()) {}
            if (emp_ & J_B) { cs = 255; break; }
            if (emp_ & J_UP) cs = cs ? cs - 1 : c;
            if (emp_ & J_DOWN) cs = cs >= c ? 0 : cs + 1;
            if (emp_ & J_A) break;
          }
          ewait_clear();
          if (cs == 255) continue;
          echap = cs;
          { char t1[12] = "CHAPTER "; t1[8] = (char)('1' + cs); t1[9] = 0; story_card(t1, CH[cs].s1, CH[cs].name); }
          story_card("CHITIN", CH[cs].s2, "\1 BEGIN");
          emp_newgame(); emp_play(); emp_end();
          continue;
        }
        if (k == 1) { echap = 255; enriv = nriv_sel; eaggr = agg_sel; emp_newgame(); emp_play(); emp_end(); continue; }
        if (k == 5) { emp_load(); emp_play(); emp_end(); continue; }
      }
    }
  }
  ewait_clear();
  emode = 0;
}
static void vram_reload(void) {
  u16 k;
  gfx_init(); opts_apply();
  for (k = 0; k < 36 * 8; k++) TILE2[EMT * 8 + k] = EMPT[k];
  for (k = 0; k < 4 * 8; k++) OBJT32[ARMT_T * 8 + k] = ARMT[k];
  for (k = 0; k < 16; k++) { OBJPAL[7 * 16 + k] = PAL_ANT1[k]; OBJPAL[8 * 16 + k] = PAL_ANT1[k]; }
  OBJPAL[7 * 16 + 1] = RGB(10, 7, 0); OBJPAL[7 * 16 + 2] = RGB(27, 22, 3); OBJPAL[7 * 16 + 3] = RGB(31, 30, 14);
  OBJPAL[8 * 16 + 1] = RGB(7, 1, 9); OBJPAL[8 * 16 + 2] = RGB(19, 7, 26); OBJPAL[8 * 16 + 3] = RGB(28, 18, 31);
  eshell = has_t(T_SHELL); earm = nb_of[B_ARMORY] ? 1 : 0;
}
