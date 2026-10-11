// ===================== EXTRAS (included from main.c): achievements, unlockables, cheats, records =====================
// 32 achievements are saved as bits in SRAM 56..59. Many unlock rewards: ant colours, menu styles and the cheats.
// Cheats are switched on in EXTRAS > CHEATS and last until you switch them off (not saved). While any cheat is on,
// achievements are not earned (except CHEATER, of course).
#define NACH 32
static const char *const ACN[NACH] = {
  "FIRST BLOOD", "SPEED QUEEN", "HARD SHELL", "FLOOD MASTER", "BIG LAND", "TO INFINITY", "ARCHITECT", "TOURIST",
  "FLAWLESS", "POPULAR", "FULL COLONY", "DEMOCRAT", "SETTLER", "CITY BUILDER", "METROPOLIS", "SCHOLAR",
  "AMBER AGE", "GENERAL", "WARLORD", "CONQUEROR", "WONDER", "BELOVED", "TYRANT", "STORYTELLER",
  "CHAPTER ONE", "ZONED OUT", "DIPLOMAT", "SURVIVOR", "RICH", "CHEATER", "COMPLETIONIST", "OLD SCHOOL"};
static const char *const ACD[NACH] = {
  "WIN A CLASSIC GAME", "WIN IN UNDER 3 MINUTES", "WIN ON HARD", "DROWN 20 RED ANTS IN A GAME", "WIN ON A 96 X 96 MAP",
  "WIN AN INFINITE MAP", "SAVE A MAP IN THE EDITOR", "LOOK THROUGH THE ANT EYE", "WIN WITHOUT LOSING A HEART",
  "REACH POPULARITY 99", "HAVE 24 ANTS AT ONCE", "WIN 3 ELECTIONS IN A GAME", "EMPIRE: BUILD 10 BUILDINGS",
  "EMPIRE: BUILD 40 BUILDINGS", "EMPIRE: REACH 60 ANTS", "EMPIRE: LEARN EVERY TECH", "EMPIRE: REACH THE AMBER AGE",
  "WIN A ZOOMED IN BATTLE", "HAVE 500 TROOPS", "WIN BY CONQUEST", "FINISH THE GREAT MOUND", "ALL 4 GROUPS AT 80",
  "PASS MARTIAL LAW", "FINISH THE STORY", "FINISH CHAPTER 1", "ZONES BUILD 10 THINGS", "MAKE PEACE WITH A RIVAL",
  "STOP 10 ENEMY ARMIES", "999 OF ANYTHING", "TURN ON A CHEAT", "GET 31 ACHIEVEMENTS", "ENTER THE KONAMI CODE"};
static const char *const ACR[NACH] = {     // what each one unlocks ("" = just the trophy)
  "GOLD ANTS", "CHEAT: NO DISASTERS", "", "CHEAT: INFINITE AP", "", "", "", "",
  "CHEAT: GOD QUEEN", "PINK ANTS", "CHEAT: TURBO", "", "", "CHEAT: RICH COLONY", "", "CHEAT: ALL TECHS  WHITE ANTS",
  "AMBER MENUS", "", "CHEAT: MEGA ARMY", "", "", "CHEAT: PARTY ANTS", "SHADOW ANTS", "CRYSTAL MENUS",
  "", "CHEAT: INSTANT ZONES", "CHEAT: PEACEFUL RIVALS", "", "", "", "", ""};
static u32 ach;
static u8 any_cheat(void) { u8 i; for (i = 0; i < 10; i++) if (cht[i]) return 1; return 0; }
static void ach_save(void) { SRAM[56] = (u8)ach; SRAM[57] = (u8)(ach >> 8); SRAM[58] = (u8)(ach >> 16); SRAM[59] = (u8)(ach >> 24); }
static void ach_load(void) {
  ach = (u32)SRAM[56] | ((u32)SRAM[57] << 8) | ((u32)SRAM[58] << 16) | ((u32)SRAM[59] << 24);
  if (ach == 0xFFFFFFFFu) { ach = 0; ach_save(); }           // a fresh (blank) cartridge
}
static u8 ach_has(u8 i) { return (u8)((ach >> i) & 1); }
static u8 ach_count(void) { u8 i, n = 0; for (i = 0; i < NACH; i++) n += ach_has(i); return n; }
static char tbuf[31];
static void ach_get(u8 i) {
  u8 k = 0; const char *s;
  if (ach_has(i) || (any_cheat() && i != 29)) return;
  ach |= (u32)1 << i; ach_save();
  s = "TROPHY: "; while (*s) tbuf[k++] = *s++;
  s = ACN[i]; while (*s && k < 30) tbuf[k++] = *s++;
  tbuf[k] = 0; say(tbuf); jingle(WIN_TUNE, 6);
  if (ach_count() >= 31) ach_get(30);
}
// ---------- cheats ----------
#define NCHT 10
static const char *const CHN[NCHT] = {"INFINITE AP", "GOD QUEEN", "NO DISASTERS", "RICH COLONY", "ALL TECHS", "MEGA ARMY", "PARTY ANTS", "TURBO", "PEACEFUL RIVALS", "INSTANT ZONES"};
static const char *const CHD[NCHT] = {"CLASSIC: AP NEVER RUNS OUT", "YOUR QUEEN AND NEST CANT DIE", "NO FLOODS OR EARTHQUAKES",
  "EMPIRE: 999 OF EVERYTHING", "EMPIRE: START KNOWING ALL", "EMPIRE: START WITH 500 GUARDS", "YOUR ANTS CHANGE COLOR", "R SWITCHES TO X8 SPEED",
  "RED ANTS NEVER ATTACK", "EMPIRE: ZONES BUILD FAST + FREE"};
static const u8 CHA[NCHT] = {3, 8, 1, 13, 15, 18, 21, 10, 26, 25};   // the achievement that unlocks each cheat
static u8 cht_ok(u8 i) { return SRAM[61] == 'C' || ach_has(CHA[i]); }
static const u16 CODE[9] = {J_UP, J_DOWN, J_LEFT, J_RIGHT, J_UP, J_DOWN, J_LEFT, J_RIGHT, J_SEL};   // EXTRAS > CHEATS: unlocks every cheat
// ---------- unlockable colours / styles (checked by the options screen) ----------
static u8 opt_locked(u8 i, u8 v) {
  if (i == O_ANTCOL) { if (v == 4) return !ach_has(0); if (v == 5) return !ach_has(15); if (v == 6) return !ach_has(9); if (v == 7) return !ach_has(22); }
  if (i == O_PANEL) { if (v == 4) return !ach_has(16); if (v == 5) return !ach_has(23); }
  return 0;
}
// ---------- the EXTRAS screen (title: B) ----------
static void extras(void) {
  u8 tab = 0, sel = 0, top = 0, i, ki = 0; u16 k, p, prev = 0;
  static const char *const XTN[3] = {"ACHIEVEMENTS", "CHEATS", "RECORDS"};
  DISPCNT = 0x80;
  for (;;) {
    u8 n = tab == 0 ? NACH : tab == 1 ? NCHT : 0;
    ov_fill(1, 3, 0, 19); ui_full();
    ps(1, 0, "\3", 1); ps((u8)(15 - (blen2(XTN[tab]) + 1) / 2), 0, XTN[tab], 9); ps(28, 0, "\4", 1);
    pm_rule(1);
    if (tab == 0) {
      ps(2, 2, "EARNED", 10); pn(9, 2, ach_count(), 9); ps(12, 2, "OF 32", 10);
      if (sel < top) top = sel; if (sel >= top + 11) top = (u8)(sel - 10);
      for (i = 0; i < 11 && top + i < NACH; i++) { u8 a = (u8)(top + i), y = (u8)(4 + i);
        if (a == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        pc(3, y, ach_has(a) ? '$' : '-', ach_has(a) ? 9 : 15); ps(5, y, ACN[a], ach_has(a) ? (a == sel ? 9 : 3) : 15); }
      pm_rule(15); ps(1, 16, ACD[sel], 3);
      if (ACR[sel][0]) { ps(1, 17, "UNLOCKS:", 10); ps(10, 17, ACR[sel], ach_has(sel) ? 13 : 15); }
    } else if (tab == 1) {
      ps(2, 2, "ACHIEVEMENTS UNLOCK CHEATS", 10);
      for (i = 0; i < NCHT; i++) { u8 y = (u8)(4 + i);
        if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        ps(3, y, CHN[i], cht_ok(i) ? (i == sel ? 9 : 3) : 15);
        ps(21, y, !cht_ok(i) ? "LOCKED" : cht[i] ? "ON" : "OFF", !cht_ok(i) ? 15 : cht[i] ? 13 : 10); }
      pm_rule(15); ps(1, 16, CHD[sel], 3);
      if (!cht_ok(sel)) { ps(1, 17, "EARN:", 10); ps(7, 17, ACN[CHA[sel]], 8); }
    } else {
      static const char *const DN3[3] = {"EASY", "NORM", "HARD"};
      ps(2, 3, "LEVEL   WINS   BEST   SCORE", 10);
      for (i = 0; i < 3; i++) { u8 y = (u8)(5 + i * 2); u16 b = best[i];
        ps(2, y, DN3[i], 9); pn(10, y, wins[i], 3);
        if (b) { pn(16, y, (u8)(b / 60 > 99 ? 99 : b / 60), 3); pc(18, y, ':', 10); pz(19, y, (u8)(b % 60), 3); } else ps(17, y, "-", 15);
        pd(23, y, hiscore[i], 3); }
      ps(2, 12, "TROPHIES", 10); pn(14, 12, ach_count(), 9); ps(17, 12, "OF 32", 10);
      ps(2, 13, "STORY", 10); pc(14, 13, (char)('0' + (SRAM[63] <= 7 ? SRAM[63] : 0)), 9); ps(17, 13, "OF 7 DONE", 10);
      ps(2, 14, "EMPIRE MODE", 10); ps(14, 14, emp_unlocked() ? "OPEN" : "LOCKED", emp_unlocked() ? 13 : 15);
    }
    ps(1, 19, tab == 1 ? "\1 ON/OFF \3\4 TABS \2 BACK" : "\3\4 TABS   \2 BACK", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    do { vsync(); k = joy(); p = k & ~prev; prev = k; } while (!p);
    if (tab == 1) {                                 // the secret code: L R L R UP DOWN B A
      if (p == CODE[ki]) { if (++ki == 9) { ki = 0; SRAM[61] = 'C'; sfx_mana(); jingle(WIN_TUNE, 6); continue; } } else ki = (u8)(p == CODE[0]);
    }
    if (p & J_B) break;
    if (p & (J_L | J_R)) { tab = (u8)((tab + ((p & J_R) ? 1 : 2)) % 3); sel = 0; top = 0; sfx_food(); continue; }
    if (n && (p & J_UP)) { sel = sel ? sel - 1 : n - 1; sfx_food(); }
    if (n && (p & J_DOWN)) { sel = sel + 1 >= n ? 0 : sel + 1; sfx_food(); }
    if (tab == 1 && (p & J_A)) { if (cht_ok(sel)) { cht[sel] ^= 1; sfx_mana(); if (cht[sel]) ach_get(29); } else sfx_deny(); }
  }
  while (joy()) vsync();
  ov_clear();
}
