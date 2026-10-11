// ===================== EXTRAS (included from main.c): achievements, unlockables, cheats, records =====================
// 64 achievements: bits 0-31 in SRAM 56..59, bits 32-63 in SRAM 9300..9303 ('X' 'A' at 9304 marks them valid).
// Many unlock rewards: ant colours, menu styles and cheats. Cheats are switched on in EXTRAS > CHEATS and last until you
// switch them off (not saved). While any cheat is on, achievements are not earned (except CHEATER, of course).
#define NACH 64
static const char *const ACN[NACH] = {
  "FIRST BLOOD", "SPEED QUEEN", "HARD SHELL", "FLOOD MASTER", "BIG LAND", "TO INFINITY", "ARCHITECT", "TOURIST",
  "FLAWLESS", "POPULAR", "FULL COLONY", "DEMOCRAT", "SETTLER", "CITY BUILDER", "METROPOLIS", "SCHOLAR",
  "AMBER AGE", "GENERAL", "WARLORD", "CONQUEROR", "WONDER", "BELOVED", "TYRANT", "STORYTELLER",
  "CHAPTER ONE", "ZONED OUT", "DIPLOMAT", "SURVIVOR", "RICH", "CHEATER", "COMPLETIONIST", "OLD SCHOOL",
  "ANT HERO", "JAWS", "QUEENSLAYER", "TRAILBLAZER", "MAYOR", "TYCOON", "MEGA MAYOR", "CLEAN CITY",
  "SAFE STREETS", "FIREFIGHTER", "BEETLE BUSTER", "ALL TEN", "ANTVERSE HERO", "GOD OF ANTS", "BEST FRIENDS", "RIVALS",
  "LINKED UP", "PASS THE GBA", "SKYSCRAPERS", "BIG BUDGET", "BUSY EMPIRE", "SECRET HUNTER", "BIG MAP MAKER", "LEGEND",
  "EL PRESIDANTE", "LANDSLIDE", "HONEY POT", "IRON FIST", "PEOPLES HERO", "FACT FAN", "AGENT ANT", "MISSION MASTER"};
static const char *const ACD[NACH] = {
  "WIN A CLASSIC GAME", "WIN IN UNDER 3 MINUTES", "WIN ON HARD", "DROWN 20 RED ANTS IN A GAME", "WIN ON A 96 X 96 MAP",
  "WIN AN INFINITE MAP", "SAVE A MAP IN THE EDITOR", "LOOK THROUGH THE ANT EYE", "WIN WITHOUT LOSING A HEART",
  "REACH POPULARITY 99", "HAVE 24 ANTS AT ONCE", "WIN 3 ELECTIONS IN A GAME", "EMPIRE: BUILD 10 BUILDINGS",
  "EMPIRE: BUILD 40 BUILDINGS", "EMPIRE: REACH 60 ANTS", "EMPIRE: LEARN EVERY TECH", "EMPIRE: REACH THE AMBER AGE",
  "WIN A ZOOMED IN BATTLE", "HAVE 500 TROOPS", "WIN BY CONQUEST", "FINISH THE GREAT MOUND", "ALL 4 GROUPS AT 80",
  "PASS MARTIAL LAW", "FINISH THE STORY", "FINISH CHAPTER 1", "ZONES BUILD 10 THINGS", "MAKE PEACE WITH A RIVAL",
  "STOP 10 ENEMY ARMIES", "999 OF ANYTHING", "TURN ON A CHEAT", "GET 31 ACHIEVEMENTS", "ENTER THE KONAMI CODE",
  "ANT EYE: CARRY 10 FOOD HOME", "ANT EYE: BITE 10 RED ANTS", "ANT EYE: BEAT THE RED QUEEN", "ANT EYE: LAY 50 SCENT",
  "FINISH A CITY SCENARIO", "CITY: HAVE 20000 MONEY", "CITY: REACH MEGALOPOLIS", "CITY: 2000 ANTS, SMOG UNDER 10",
  "CITY: 2000 ANTS, CRIME UNDER 10", "DEW PUMPS SAVE 10 FIRES", "GUARDS BEAT A GIANT BEETLE", "FINISH ALL 10 SCENARIOS",
  "WIN AN ANTVERSE GAME", "USE ALL 5 GOD POWERS", "WIN A 2 PLAYER CO-OP GAME", "WIN A VERSUS GAME",
  "PLAY ON A LINK CABLE", "PASS THE GBA TO A FRIEND", "CITY: 50 LEVEL 3 HOMES", "CITY: EARN 500 A MONTH",
  "EMPIRE: 80 BUILDINGS", "FIND ALL 3 SECRET CHEATS", "SAVE A 96 X 96 MAP", "GET 50 ACHIEVEMENTS",
  "CITY: WIN 3 VOTES IN A GAME", "WIN A VOTE WITH 80 PERCENT", "HIDE *10000 IN THE HONEY POT", "STOP A COUP OR AN UPRISING",
  "REACH 90 RESPECT", "READ 50 LOADING FACTS", "FINISH 10 ANTVERSE MISSIONS", "FINISH ALL 50 MISSIONS"};
static const char *const ACR[NACH] = {     // what each one unlocks ("" = just the trophy)
  "GOLD ANTS", "CHEAT: WEATHER", "CHEAT: STRONG ANTS", "CHEAT: INFINITE AP", "CHEAT: FOOD RAIN", "", "", "",
  "CHEAT: GOD QUEEN", "PINK ANTS", "CHEAT: TURBO", "", "", "CHEAT: RICH COLONY", "", "CHEAT: ALL TECHS  WHITE ANTS",
  "AMBER MENUS", "", "CHEAT: MEGA ARMY", "", "", "CHEAT: PARTY ANTS", "SHADOW ANTS", "CRYSTAL MENUS",
  "", "CHEAT: INSTANT ZONES", "CHEAT: PEACEFUL RIVALS", "CHEAT: BIG BROOD", "", "", "", "",
  "CHEAT: SUPER ANT", "CHEAT: IRON JAWS", "", "", "", "CHEAT: MONEY TREE", "", "",
  "", "CHEAT: NO FIRES", "", "", "", "", "", "CHEAT: GIANT FLOODS",
  "", "", "", "", "", "", "", "",
  "CHEAT: LOYAL MASSES", "", "CHEAT: RIGGED VOTES", "", "", "", "", ""};
static u32 ach, ach2;
static u8 any_cheat(void) { u8 i; for (i = 0; i < NCHT; i++) if (cht[i]) return 1; return 0; }
static void ach_save(void) {
  SRAM[56] = (u8)ach; SRAM[57] = (u8)(ach >> 8); SRAM[58] = (u8)(ach >> 16); SRAM[59] = (u8)(ach >> 24);
  SRAM[9300] = (u8)ach2; SRAM[9301] = (u8)(ach2 >> 8); SRAM[9302] = (u8)(ach2 >> 16); SRAM[9303] = (u8)(ach2 >> 24); SRAM[9304] = 'X'; SRAM[9305] = 'A';
}
static void ach_load(void) {
  ach = (u32)SRAM[56] | ((u32)SRAM[57] << 8) | ((u32)SRAM[58] << 16) | ((u32)SRAM[59] << 24);
  ach2 = (SRAM[9304] == 'X' && SRAM[9305] == 'A') ? ((u32)SRAM[9300] | ((u32)SRAM[9301] << 8) | ((u32)SRAM[9302] << 16) | ((u32)SRAM[9303] << 24)) : 0;
  if (ach == 0xFFFFFFFFu) { ach = 0; ach2 = 0; ach_save(); }   // a fresh (blank) cartridge
}
static u8 ach_has(u8 i) { return (u8)(i < 32 ? (ach >> i) & 1 : (ach2 >> (i - 32)) & 1); }
static u8 ach_count(void) { u8 i, n = 0; for (i = 0; i < NACH; i++) n += ach_has(i); return n; }
static char tbuf[31];
static void ach_get(u8 i) {
  u8 k = 0; const char *s;
  if (i >= NACH || ach_has(i) || (any_cheat() && i != 29)) return;
  if (i < 32) ach |= (u32)1 << i; else ach2 |= (u32)1 << (i - 32);
  ach_save();
  s = "TROPHY: "; while (*s) tbuf[k++] = *s++;
  s = ACN[i]; while (*s && k < 30) tbuf[k++] = *s++;
  tbuf[k] = 0; say(tbuf); jingle(WIN_TUNE, 6);
  if (ach_count() >= 31) ach_get(30);
  if (ach_count() >= 50) ach_get(55);
}
// ---------- cheats ----------
// Every cheat has levels (A steps through them: OFF -> level 1 -> ... -> OFF). Cheats are unlocked by trophies
// (or all at once by the master code), except 13-15: SECRET, they only appear after you type their own button code on
// the CHEATS page. Found secrets are saved in SRAM 20 (21 = 'S' marks it as valid).
#define NSEC 13                                     // first secret cheat (13, 14, 15)
static const char *const CHN[NCHT] = {"INFINITE AP", "GOD QUEEN", "WEATHER", "RICH COLONY", "ALL TECHS", "MEGA ARMY", "PARTY ANTS", "TURBO",
  "PEACEFUL RIVALS", "INSTANT ZONES", "FOOD RAIN", "STRONG ANTS", "BIG BROOD", "FAST RESEARCH", "FREE BUILD", "SLEEPY REDS",
  "SUPER ANT", "IRON JAWS", "MONEY TREE", "NO FIRES", "GIANT FLOODS", "LOYAL MASSES", "RIGGED VOTES"};
static const char *const CHD[NCHT] = {"CLASSIC: AP NEVER RUNS OUT", "YOUR QUEEN AND NEST CANT DIE", "CALM: NONE  CHAOS: LOTS",
  "EMPIRE: LOTS OF EVERYTHING", "EMPIRE: START KNOWING ALL", "EMPIRE: START WITH GUARDS", "YOUR ANTS CHANGE COLOR", "R SWITCHES TO X8 SPEED",
  "RED ANTS NEVER ATTACK", "EMPIRE: ZONES GROW FAST", "CLASSIC: FOOD FALLS FROM SKY", "YOUR ANTS WIN MORE FIGHTS", "MORE BABIES EACH HATCH",
  "EMPIRE: LEARN TECHS FASTER", "EMPIRE: BUILDINGS COST LESS", "CLASSIC: RED ANTS HATCH LESS",
  "ANT EYE: NO BITES, RUN FAST", "ANT EYE: EVERY BITE WINS", "CITY: TAXES BRING MORE", "CITY: NOTHING CATCHES FIRE", "CLASSIC: FLOODS ARE 5 X 5",
  "PALACE: EVERY GROUP LOVES YOU", "PALACE: YOU WIN EVERY VOTE"};
static const u8 CHMAX[NCHT] = {1, 1, 2, 3, 1, 3, 3, 1, 1, 3, 3, 3, 2, 3, 2, 2, 1, 1, 2, 1, 1, 1, 1};
static const char *const CHV[NCHT][4] = {
  {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "CALM", "CHAOS"}, {"OFF", "250", "500", "999"}, {"OFF", "ON"}, {"OFF", "100", "250", "500"},
  {"OFF", "SLOW", "FAST", "STROBE"}, {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "X2", "X5", "X10"}, {"OFF", "LIGHT", "HEAVY", "STORM"},
  {"OFF", "X2", "X4", "SUPER"}, {"OFF", "TWINS", "TRIPLE"}, {"OFF", "X2", "X5", "INSTANT"}, {"OFF", "HALF", "FREE"}, {"OFF", "FEWER", "NONE"},
  {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "X2", "X3"}, {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "ON"}};
static const u8 CHA[NCHT] = {3, 8, 1, 13, 15, 18, 21, 10, 26, 25, 4, 2, 27, 255, 255, 255, 32, 33, 37, 41, 47, 56, 58};   // trophy that unlocks each (255 = secret code)
static const u16 SECC[3][8] = {                    // the secret codes (no A, B, L or R: those do other things on the page)
  {J_UP, J_UP, J_UP, J_DOWN, J_DOWN, J_DOWN, J_START, J_SEL},
  {J_LEFT, J_LEFT, J_RIGHT, J_RIGHT, J_LEFT, J_RIGHT, J_START, J_START},
  {J_DOWN, J_DOWN, J_DOWN, J_DOWN, J_SEL, J_SEL, J_START, J_START}};
static const char *const SECH[3] = {"UUU DDD START SELECT", "LL RR L R START START", "DDDD SEL SEL START START"};
static u8 sec_bits(void) { return SRAM[21] == 'S' ? SRAM[20] : 0; }
static u8 cht_ok(u8 i) { return (i >= NSEC && i < NSEC + 3) ? (u8)((sec_bits() >> (i - NSEC)) & 1) : (SRAM[61] == 'C' || ach_has(CHA[i])); }
static u8 cht_on(void) { u8 i, n = 0; for (i = 0; i < NCHT; i++) n += cht[i] != 0; return n; }
static const u16 CODE[9] = {J_UP, J_DOWN, J_LEFT, J_RIGHT, J_UP, J_DOWN, J_LEFT, J_RIGHT, J_SEL};   // EXTRAS > CHEATS: unlocks cheats 0-12
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
      ps(2, 2, "EARNED", 10); pn(9, 2, ach_count(), 9); ps(12, 2, "OF", 10); pn(15, 2, NACH, 10);
      if (sel < top) top = sel; if (sel >= top + 5) top = (u8)(sel - 4);
      for (i = 0; i < 5 && top + i < NACH; i++) { u8 a = (u8)(top + i), y = (u8)(4 + i * 2);
        if (a == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        pc(3, y, ach_has(a) ? '$' : '-', ach_has(a) ? 9 : 15); ps(5, y, ACN[a], ach_has(a) ? (a == sel ? 9 : 3) : 15); }
      pm_rule(15); ps(1, 16, ACD[sel], 3);
      if (ACR[sel][0]) { ps(1, 17, "UNLOCKS:", 10); ps(10, 17, ACR[sel], ach_has(sel) ? 13 : 15); }
    } else if (tab == 1) {
      ps(2, 2, "CHEATS ON", 10); pn(12, 2, cht_on(), cht_on() ? 13 : 9); if (cht_on()) ps(16, 2, "NO TROPHIES", 8);
      if (sel < top) top = sel; if (sel >= top + 5) top = (u8)(sel - 4);
      for (i = 0; i < 5 && top + i < NCHT; i++) { u8 a = (u8)(top + i), y = (u8)(4 + i * 2), ok = cht_ok(a), lv;
        if (a == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        if (a >= NSEC && a < NSEC + 3 && !ok) { ps(3, y, "? ? ? SECRET", a == sel ? 8 : 15); continue; }
        ps(3, y, CHN[a], ok ? (a == sel ? 9 : 3) : 15);
        if (!ok) { ps(21, y, "LOCKED", 15); continue; }
        if (CHMAX[a] > 1) for (lv = 0; lv < CHMAX[a]; lv++)            // level pips: one per level, lit up to the current one
          BGMAP1[y * 32 + 19 + lv] = (u16)((cht[a] > lv ? BAR_F : BAR_E) | (14 << 12));
        ps(23, y, CHV[a][cht[a]], cht[a] ? 13 : 10); }
      pm_rule(15);
      if (sel >= NSEC && sel < NSEC + 3 && !cht_ok(sel)) { ps(1, 16, "SECRET! TYPE ITS CODE HERE:", 8); ps(1, 17, SECH[sel - NSEC], 9); }
      else { ps(1, 16, CHD[sel], 3);
        if (!cht_ok(sel)) { ps(1, 17, "EARN:", 10); ps(7, 17, ACN[CHA[sel]], 8); }
        else if (CHMAX[sel] > 1) { ps(1, 17, "LEVELS:", 10); { u8 x = 9, lv; for (lv = 1; lv <= CHMAX[sel]; lv++) { ps(x, 17, CHV[sel][lv], cht[sel] == lv ? 13 : 3); x = (u8)(x + blen2(CHV[sel][lv]) + 1); } } } }
    } else {
      static const char *const DN3[3] = {"EASY", "NORM", "HARD"};
      ps(2, 3, "LEVEL   WINS   BEST   SCORE", 10);
      for (i = 0; i < 3; i++) { u8 y = (u8)(5 + i * 2); u16 b = best[i];
        ps(2, y, DN3[i], 9); pn(10, y, wins[i], 3);
        if (b) { pn(16, y, (u8)(b / 60 > 99 ? 99 : b / 60), 3); pc(18, y, ':', 10); pz(19, y, (u8)(b % 60), 3); } else ps(17, y, "-", 15);
        pd(23, y, hiscore[i], 3); }
      ps(2, 12, "TROPHIES", 10); pn(14, 12, ach_count(), 9); ps(17, 12, "OF", 10); pn(20, 12, NACH, 10);
      ps(2, 13, "STORY", 10); pc(14, 13, (char)('0' + (SRAM[63] <= 7 ? SRAM[63] : 0)), 9); ps(17, 13, "OF 7 DONE", 10);
      ps(2, 14, "EMPIRE MODE", 10); ps(14, 14, emp_unlocked() ? "OPEN" : "LOCKED", emp_unlocked() ? 13 : 15);
      ps(2, 15, "CITY GOALS", 10); pn(14, 15, scen_count(), 9); ps(17, 15, "OF 10 DONE", 10);
      ps(2, 16, "ANTVERSE", 10); ps(14, 16, verse_unlocked() ? "OPEN" : "LOCKED", verse_unlocked() ? 13 : 15);
      ps(2, 17, "MISSIONS", 10); pn(14, 17, mis_count(), 9); ps(17, 17, "OF 50", 10); ps(22, 17, "FACTS", 10); { u16 f = fact_count(); pn(28, 17, (u8)(f > 99 ? 99 : f), 9); }
    }
    ps(1, 19, tab == 1 ? "\1 CHANGE \3\4 TABS \2 BACK" : "\3\4 TABS   \2 BACK", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    do { vsync(); k = joy(); p = k & ~prev; prev = k; } while (!p);
    if (tab == 1) {                                 // the master code, and the 3 secret cheat codes
      static u8 sk[3]; u8 c;
      if (p == CODE[ki]) { if (++ki == 9) { ki = 0; SRAM[61] = 'C'; sfx_mana(); jingle(WIN_TUNE, 6); continue; } } else ki = (u8)(p == CODE[0]);
      for (c = 0; c < 3; c++) {
        if (p == SECC[c][sk[c]]) { if (++sk[c] == 8) { sk[c] = 0;
            if (!cht_ok((u8)(NSEC + c))) { SRAM[20] = (u8)(sec_bits() | (1 << c)); SRAM[21] = 'S'; sel = (u8)(NSEC + c); sfx_mana(); jingle(WIN_TUNE, 6); if (sec_bits() == 7) ach_get(53); } } }
        else sk[c] = (u8)(p == SECC[c][0]);
      }
    }
    if (p & J_B) break;
    if (p & (J_L | J_R)) { tab = (u8)((tab + ((p & J_R) ? 1 : 2)) % 3); sel = 0; top = 0; sfx_food(); continue; }
    if (n && (p & J_UP)) { sel = sel ? sel - 1 : n - 1; sfx_food(); }
    if (n && (p & J_DOWN)) { sel = sel + 1 >= n ? 0 : sel + 1; sfx_food(); }
    if (tab == 1 && (p & J_A)) { if (cht_ok(sel)) { cht[sel] = (u8)(cht[sel] >= CHMAX[sel] ? 0 : cht[sel] + 1); sfx_mana(); if (cht[sel]) ach_get(29); } else sfx_deny(); }
  }
  while (joy()) vsync();
  ov_clear();
}
