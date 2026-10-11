// ============================================================================================================================
//  ANTVERSE MISSIONS (included from city.c): 50 missions picked from the ANTVERSE menu. Some use one mode only (classic,
//  ANT EYE, EMPIRE, ANT CITY, EL PRESIDANTE), the rest mix everything. Each has a goal, a map and (mostly) a time limit.
//  Finished missions are marked on the cartridge (SRAM 9350: 'M' 'I' + 7 bytes of bits).
// ============================================================================================================================
enum { MK_CLASSIC, MK_EYE, MK_EMPIRE, MK_CITY, MK_PALACE, MK_VERSE };
enum { MG_CWIN, MG_CFLAW, MG_CFLOOD, MG_EFOOD, MG_EBITE, MG_EQUEEN, MG_ESCENT, MG_EPOP, MG_EBLD, MG_TROOPS, MG_CONQ, MG_WONDER, MG_ERA,
       MG_AMBER, MG_TECHS, MG_HAPPY4, MG_CPOP, MG_CASH, MG_CSMOG, MG_CCRIME, MG_ARCO, MG_HOMES3, MG_FOODST, MG_SURVIVE, MG_VOTES, MG_RESP,
       MG_STASH, MG_ALLSUP, MG_LIB, MG_REL, MG_ALLY, MG_NOREB, MG_MART, MG_QUEEN, MG_VPOP, MG_ALL, MG_FINAL, MG_MEGA };
enum { TW_DEBT = 1, TW_FIRES = 2, TW_REBELS = 4, TW_ANGRY = 8, TW_HOSTILE = 16, TW_STARTER = 32, TW_RICH = 64, TW_HARD = 128 };
typedef struct { const char *name; u8 kind, map, w, h; u16 lim; u8 goal; u16 v1; u8 v2, money, dis, riv, aggr, pzm, tw; const char *s[3]; } Mis;
#define NMIS 50
static const Mis MIS[NMIS] = {
  // ---- classic ----
  {"FIRST STEPS", MK_CLASSIC, 0, 40, 32, 0, MG_CWIN, 0, 0, 1, 0, 0, 0, 0, 0, {"THE OLD GAME. RAISE AND", "LOWER THE LAND SO YOUR ANTS", "REACH THE RED QUEEN. WIN!"}},
  {"SPEED RUN", MK_CLASSIC, 0, 32, 32, 5, MG_CWIN, 0, 0, 1, 0, 0, 0, 0, 0, {"A TINY MEADOW AND A CLOCK.", "THE RED QUEEN MUST FALL", "IN UNDER 5 MINUTES."}},
  {"NOT A SCRATCH", MK_CLASSIC, 1, 48, 40, 0, MG_CFLAW, 0, 0, 1, 0, 0, 0, 0, 0, {"WIN ACROSS THE RIVER", "WITHOUT YOUR QUEEN LOSING", "A SINGLE HEART."}},
  {"THE DELUGE", MK_CLASSIC, 2, 48, 48, 0, MG_CFLOOD, 15, 0, 1, 0, 0, 0, 0, 0, {"THE LAKES ARE YOUR WEAPON.", "DROWN 15 RED ANTS WITH", "FLOODS, THEN WIN."}},
  {"HARD MODE", MK_CLASSIC, 3, 48, 48, 0, MG_CWIN, 0, 0, 1, 0, 0, 0, 0, TW_HARD, {"THE CANYON ON HARD. THE", "RED ANTS HATCH FAST AND", "ATTACK EARLY. WIN ANYWAY."}},
  // ---- ant eye ----
  {"THROUGH THE EYE", MK_EYE, 0, 40, 32, 0, MG_EFOOD, 5, 0, 1, 0, 0, 0, 0, 0, {"OPEN THE ANT EYE (\5 MENU)", "AND CARRY 5 FOOD HOME", "WITH YOUR OWN JAWS."}},
  {"JAWS OF STEEL", MK_EYE, 0, 40, 32, 0, MG_EBITE, 8, 0, 1, 0, 0, 0, 0, 0, {"IN THE ANT EYE, BITE 8", "RED ANTS. WATCH YOUR", "HEARTS: THEY BITE BACK."}},
  {"REGICIDE", MK_EYE, 1, 40, 40, 0, MG_EQUEEN, 0, 0, 1, 0, 0, 0, 0, 0, {"WALK TO THE RED NEST AS", "AN ANT AND BITE THE RED", "QUEEN UNTIL SHE FALLS."}},
  {"SCENT MASTER", MK_EYE, 0, 48, 48, 0, MG_ESCENT, 60, 0, 1, 0, 0, 0, 0, 0, {"HOLD \2 IN THE ANT EYE TO", "LAY SCENT. MARK 60 SQUARES", "AND YOUR WORKERS FOLLOW."}},
  // ---- empire ----
  {"HOMESTEAD", MK_EMPIRE, 0, 48, 40, 20, MG_EPOP, 30, 0, 1, 0, 0, 0, 0, 0, {"A QUIET MEADOW, NO RIVALS.", "FARMS, CHAMBERS, NURSERIES:", "REACH 30 ANTS IN 20 MINUTES."}},
  {"THE BUILDER", MK_EMPIRE, 0, 48, 48, 25, MG_EBLD, 30, 0, 1, 0, 1, 0, 0, 0, {"ONE CALM RIVAL WATCHES.", "PUT UP 30 BUILDINGS", "BEFORE 25 MINUTES PASS."}},
  {"THE AMBER ROAD", MK_EMPIRE, 3, 56, 56, 30, MG_AMBER, 100, 0, 1, 0, 1, 1, 0, 0, {"THE CANYON HILLS HOLD AMBER.", "RESEARCH IT, MINE IT, STORE", "100 AMBER. A RIVAL WANTS IT."}},
  {"ALL KNOWING", MK_EMPIRE, 0, 48, 48, 40, MG_TECHS, 0, 0, 1, 0, 0, 0, 0, 0, {"ARCHIVES AND ACADEMIES.", "LEARN EVERY ONE OF THE", "12 TECHS IN 40 MINUTES."}},
  {"BIG ARMY", MK_EMPIRE, 1, 56, 48, 30, MG_TROOPS, 300, 0, 1, 0, 1, 1, 0, 0, {"BARRACKS AND ARMORIES.", "GATHER 300 TROOPS WHILE", "THE RED ANTS RAID YOU."}},
  {"TWO FRONTS", MK_EMPIRE, 2, 64, 64, 0, MG_CONQ, 0, 0, 1, 0, 2, 1, 0, 0, {"RED AND GOLD NESTS ON", "BOTH SIDES OF THE LAKES.", "DESTROY THEM BOTH."}},
  {"THE MOUND", MK_EMPIRE, 0, 64, 64, 0, MG_WONDER, 0, 0, 1, 0, 1, 2, 0, 0, {"BUILD THE GREAT MOUND", "WHILE A BRUTAL RIVAL", "TRIES TO STOP YOU."}},
  {"BELOVED", MK_EMPIRE, 0, 48, 48, 30, MG_HAPPY4, 70, 0, 1, 0, 0, 0, 0, 0, {"FARMERS BUILDERS WARRIORS", "AND MYSTICS: GET ALL FOUR", "GROUPS TO 70 HAPPINESS."}},
  {"CRYSTAL AGE", MK_EMPIRE, 3, 56, 56, 45, MG_ERA, 0, 3, 1, 0, 1, 0, 0, 0, {"FROM DIRT TO CRYSTAL:", "REACH THE LAST AGE IN", "45 MINUTES."}},
  // ---- city ----
  {"TOWN PLANNER", MK_CITY, 0, 48, 48, 96, MG_CPOP, 1500, 0, 1, 0, 0, 0, 0, 0, {"TUNNELS, ZONES, SERVICES.", "GROW TO 1500 ANTS IN", "8 YEARS. NO PALACE HERE."}},
  {"RICH MAYOR", MK_CITY, 0, 48, 48, 120, MG_CASH, 2500, 0, 1, 0, 0, 0, 0, 0, {"TAXES, SALES, FARMERS", "MARKET. HAVE *25000 IN", "THE BANK WITHIN 10 YEARS."}},
  {"FRESH AIR", MK_CITY, 3, 48, 48, 96, MG_CSMOG, 1500, 15, 1, 0, 0, 0, 0, TW_STARTER, {"A FACTORY TOWN IN A CANYON.", "1500 ANTS WITH SMOG UNDER", "15. PARKS AND LAWS HELP."}},
  {"SKYSCRAPERS", MK_CITY, 0, 56, 56, 120, MG_HOMES3, 40, 0, 1, 0, 0, 0, 0, 0, {"LEVEL 3 HOMES NEED GUARDS", "CLINICS SCHOOLS AND VALUE.", "GROW 40 OF THEM."}},
  {"GRANARY CITY", MK_CITY, 1, 48, 48, 96, MG_FOODST, 350, 120, 1, 0, 0, 0, 0, 0, {"THE RIVER FEEDS THE FARMS.", "1200 ANTS AND 350 FOOD", "IN THE STORES AT ONCE."}},
  {"OUT OF THE RED", MK_CITY, 0, 48, 48, 120, MG_CASH, 300, 0, 1, 0, 0, 0, 0, TW_DEBT, {"YOU START *1500 IN DEBT.", "GET TO *3000 IN THE BANK", "WITHOUT GOING BANKRUPT."}},
  {"FIRESTORM", MK_CITY, 1, 56, 56, 120, MG_CPOP, 2000, 0, 1, 1, 0, 0, 0, TW_FIRES | TW_STARTER, {"THE TOWN IS BURNING AND", "DISASTERS ARE ON. REBUILD", "TO 2000 ANTS."}},
  {"CLEAN HANDS", MK_CITY, 2, 48, 48, 96, MG_CCRIME, 1500, 10, 1, 0, 0, 0, 0, TW_STARTER, {"A PACKED LAKE TOWN FULL", "OF THIEVES. 1500 ANTS", "WITH CRIME UNDER 10."}},
  {"SKY MOUND", MK_CITY, 7, 96, 96, 300, MG_ARCO, 6000, 0, 1, 0, 0, 0, 0, 0, {"THE BIGGEST LAND. BECOME", "A METROPOLIS AND BUILD", "THE SKY MOUND. 25 YEARS."}},
  // ---- el presidante ----
  {"EL PRESIDANTE", MK_PALACE, 0, 48, 48, 120, MG_VOTES, 800, 2, 1, 0, 0, 0, 1, 0, {"THE PALACE IS YOURS. KEEP", "THE 8 GROUPS HAPPY AND", "WIN 2 VOTES WITH 800 ANTS."}},
  {"RESPECT", MK_PALACE, 0, 48, 48, 120, MG_RESP, 1000, 75, 1, 0, 0, 0, 1, 0, {"STATUES, SPEECHES, WINS.", "REACH 75 RESPECT WITH", "1000 ANTS IN THE CITY."}},
  {"THE HONEY POT", MK_PALACE, 1, 48, 48, 120, MG_STASH, 500, 0, 1, 0, 0, 0, 1, 0, {"SKIM THE TAXES QUIETLY.", "HIDE *5000 IN THE HONEY", "POT WITHOUT BEING OUSTED."}},
  {"LOVED BY ALL", MK_PALACE, 0, 56, 56, 144, MG_ALLSUP, 1200, 65, 1, 0, 0, 0, 1, 0, {"1200 ANTS, AND EVERY ONE", "OF THE 8 GROUPS AT 65", "SUPPORT OR MORE."}},
  {"FREEDOM!", MK_PALACE, 2, 48, 48, 120, MG_LIB, 1000, 85, 1, 0, 0, 0, 1, 0, {"NO SECRET POLICE, NO CULT.", "FREEDOM 85 WITH 1000 ANTS.", "AND STILL WIN THE VOTES."}},
  {"TWO GIANTS", MK_PALACE, 3, 48, 48, 120, MG_REL, 0, 60, 1, 0, 0, 0, 1, TW_HOSTILE, {"THE RED EMPIRE AND THE", "GOLDEN HIVE BOTH HATE YOU.", "GET BOTH TO 60 RELATION."}},
  {"THE ALLIANCE", MK_PALACE, 0, 48, 48, 120, MG_ALLY, 1500, 0, 1, 0, 0, 0, 1, 0, {"COURT A SUPERPOWER UNTIL", "THEY SIGN AN ALLIANCE.", "AND HAVE 1500 ANTS."}},
  {"REBEL HILLS", MK_PALACE, 3, 56, 56, 72, MG_NOREB, 0, 30, 1, 0, 0, 0, 1, TW_REBELS, {"120 REBELS WAIT IN THE", "HILLS. AFTER 6 YEARS", "FEWER THAN 30 MUST REMAIN."}},
  {"THE ANGRY CITY", MK_PALACE, 0, 48, 48, 96, MG_ALLSUP, 800, 55, 1, 0, 0, 0, 1, TW_ANGRY | TW_STARTER, {"EVERY GROUP HATES YOU", "(30 SUPPORT). TURN IT", "AROUND: ALL 8 AT 55."}},
  {"IRON FIST", MK_PALACE, 1, 48, 48, 60, MG_MART, 600, 0, 1, 0, 0, 0, 1, TW_ANGRY, {"RULE BY MARTIAL LAW FOR", "5 YEARS WITH 600 ANTS AND", "DO NOT GET OVERTHROWN."}},
  {"QUEEN FOR LIFE", MK_PALACE, 2, 48, 48, 120, MG_QUEEN, 800, 0, 1, 0, 0, 0, 1, 0, {"CHANGE THE CHARTER: RULER", "QUEEN. NO VOTES, LESS", "FREEDOM. LAST 10 YEARS."}},
  {"TEN YEARS", MK_PALACE, 0, 56, 56, 120, MG_SURVIVE, 0, 0, 2, 1, 0, 0, 1, TW_REBELS | TW_ANGRY, {"ANGRY GROUPS, REBELS AND", "DISASTERS. JUST STAY IN", "THE PALACE FOR 10 YEARS."}},
  {"POOR PALACE", MK_PALACE, 3, 48, 48, 96, MG_VOTES, 500, 1, 2, 0, 0, 0, 1, TW_DEBT, {"IN DEBT AND HARD MONEY.", "WIN ONE VOTE WITH 500 ANTS", "BEFORE 8 YEARS PASS."}},
  // ---- antverse (everything) ----
  {"HOLD THE LINE", MK_VERSE, 1, 48, 48, 120, MG_SURVIVE, 0, 0, 1, 0, 1, 2, 2, 0, {"CITY, EMPIRE AND A WILD", "RIVAL. KEEP YOUR NEST", "FOR 10 YEARS."}},
  {"THE CONQUEROR", MK_VERSE, 0, 64, 64, 0, MG_CONQ, 0, 0, 1, 0, 2, 1, 2, 0, {"TWO RIVAL NESTS. GROW A", "CITY, TRAIN AN ARMY AND", "DESTROY THEM BOTH."}},
  {"VERSE SKY MOUND", MK_VERSE, 2, 64, 64, 240, MG_ARCO, 4000, 0, 1, 0, 1, 0, 2, 0, {"4000 ANTS AND THE SKY", "MOUND IN 20 YEARS WHILE", "A RIVAL NIBBLES AT YOU."}},
  {"MIGHTY", MK_VERSE, 3, 56, 56, 180, MG_VPOP, 2000, 20, 1, 0, 1, 1, 2, 0, {"2000 CITY ANTS AND 200", "TROOPS AT THE SAME TIME.", "15 YEARS."}},
  {"THREE RIVALS", MK_VERSE, 7, 96, 96, 0, MG_CONQ, 0, 0, 1, 0, 3, 2, 2, 0, {"THE INFINITE LAND, 96 BY", "96. THREE WILD RIVALS.", "BEAT THEM ALL."}},
  {"PALACE AT WAR", MK_VERSE, 1, 56, 56, 120, MG_VOTES, 1000, 1, 1, 0, 1, 1, 1, 0, {"A RIVAL RAIDS YOUR LAND", "WHILE THE GROUPS GRUMBLE.", "WIN A VOTE WITH 1000 ANTS."}},
  {"GOD OF ALL", MK_VERSE, 0, 56, 56, 144, MG_RESP, 1500, 80, 1, 0, 1, 0, 1, 0, {"GOD POWERS, A PALACE AND", "A CITY. 80 RESPECT WITH", "1500 ANTS."}},
  {"EMPIRE OF SMOG", MK_VERSE, 3, 56, 56, 120, MG_CSMOG, 1500, 15, 1, 0, 1, 0, 2, TW_STARTER, {"PITS AND MINES EVERYWHERE.", "1500 ANTS, SMOG UNDER 15,", "AND A RIVAL AT THE GATE."}},
  {"THE LONG PEACE", MK_VERSE, 2, 64, 64, 180, MG_SURVIVE, 0, 0, 1, 1, 2, 0, 2, 0, {"TWO CALM RIVALS. MAKE PEACE", "OR WAR AS YOU LIKE, BUT", "SURVIVE 15 YEARS."}},
  {"FOREIGN FRIENDS", MK_VERSE, 0, 56, 56, 144, MG_REL, 0, 70, 1, 0, 1, 0, 1, TW_HOSTILE, {"ONE RIVAL AT HOME, TWO", "ANGRY SUPERPOWERS ABROAD.", "GET BOTH POWERS TO 70."}},
  {"EVERYTHING", MK_VERSE, 1, 64, 64, 240, MG_ALL, 2000, 10, 1, 1, 2, 1, 1, 0, {"2000 ANTS, 100 TROOPS, A", "VOTE WON AND THE SKY MOUND.", "ALL OF IT. 20 YEARS."}},
  {"GRAND FINALE", MK_VERSE, 7, 96, 96, 0, MG_FINAL, 0, 0, 2, 1, 3, 2, 1, TW_HOSTILE, {"THREE WILD RIVALS, HOSTILE", "SUPERPOWERS, HARD MONEY.", "CONQUER ALL + THE SKY MOUND."}},
};
static const char *const MKN[6] = {"CLASSIC", "ANT EYE", "EMPIRE", "CITY", "PALACE", "VERSE"};
static const u8 MKC[6] = {13, 14, 9, 2, 8, 1};
#define MSO 9350
static u8 mis_on, mis_i, mis_sav_pzw;
static u8 mis_done(u8 i) { return (u8)(SRAM[MSO] == 'M' && SRAM[MSO + 1] == 'I' ? (SRAM[MSO + 2 + i / 8] >> (i & 7)) & 1 : 0); }
static u8 mis_count(void) { u8 i, n = 0; for (i = 0; i < NMIS; i++) n += mis_done(i); return n; }
static void mis_mark(u8 i) { u8 k; if (SRAM[MSO] != 'M' || SRAM[MSO + 1] != 'I') { for (k = 0; k < 7; k++) SRAM[MSO + 2 + k] = 0; SRAM[MSO] = 'M'; SRAM[MSO + 1] = 'I'; } SRAM[MSO + 2 + i / 8] |= (u8)(1 << (i & 7)); }
static u8 mis_end_goal(u8 g) { return g == MG_SURVIVE || g == MG_NOREB || g == MG_MART || g == MG_QUEEN; }
static u8 mis_ok(void) {                           // is the goal met right now?
  const Mis *m = &MIS[mis_i]; u8 i;
  switch (m->goal) {
  case MG_CWIN: return over == 1 && (!m->lim || (u32)gt * 2 / 15 <= (u32)m->lim * 60);
  case MG_CFLAW: return over == 1 && !qlost;
  case MG_CFLOOD: return over == 1 && rdrown >= m->v1;
  case MG_EFOOD: return e_del >= m->v1;
  case MG_EBITE: return e_bit >= m->v1;
  case MG_EQUEEN: return e_qk;
  case MG_ESCENT: return e_sc >= m->v1;
  case MG_EPOP: return pop >= m->v1;
  case MG_EBLD: return nbl >= m->v1;
  case MG_TROOPS: return my_troops() >= m->v1;
  case MG_CONQ: for (i = 0; i < enriv; i++) if (ralive[i]) return 0; return 1;
  case MG_WONDER: return wprog >= 40;
  case MG_ERA: return era_now() >= m->v2;
  case MG_AMBER: return ra >= m->v1;
  case MG_TECHS: return (etech & 0xFFF) == 0xFFF;
  case MG_HAPPY4: return fac[0] >= m->v1 && fac[1] >= m->v1 && fac[2] >= m->v1 && fac[3] >= m->v1;
  case MG_CPOP: return cpop >= m->v1;
  case MG_CASH: return cash >= (s32)m->v1 * 10;
  case MG_CSMOG: return cpop >= m->v1 && csmog < m->v2;
  case MG_CCRIME: return cpop >= m->v1 && ccrime < m->v2;
  case MG_ARCO: return cpop >= m->v1 && nb_of[B_ARCO];
  case MG_HOMES3: return czn[1][3] >= m->v1;
  case MG_FOODST: return rf >= m->v1 && cpop >= (u32)m->v2 * 10;
  case MG_SURVIVE: return 1;
  case MG_VOTES: return cpop >= m->v1 && PZ_.wins >= m->v2;
  case MG_RESP: return cpop >= m->v1 && PZ_.resp >= m->v2;
  case MG_STASH: return PZ_.stash >= (u32)m->v1 * 10;
  case MG_ALLSUP: if (cpop < m->v1) return 0; for (i = 0; i < NFAC; i++) if (PZ_.sup[i] < m->v2) return 0; return 1;
  case MG_LIB: return cpop >= m->v1 && PZ_.lib >= m->v2;
  case MG_REL: return PZ_.rel[0] >= m->v2 && PZ_.rel[1] >= m->v2;
  case MG_ALLY: return cpop >= m->v1 && (PZ_.ally[0] || PZ_.ally[1]);
  case MG_NOREB: return PZ_.reb < m->v2;
  case MG_MART: return pz_mart() && cpop >= m->v1;
  case MG_QUEEN: return PZ_.con[5] == 1 && cpop >= m->v1;
  case MG_VPOP: return cpop >= m->v1 && my_troops() >= (u16)m->v2 * 10;
  case MG_ALL: return cpop >= m->v1 && my_troops() >= (u16)m->v2 * 10 && PZ_.wins >= 1 && nb_of[B_ARCO];
  case MG_FINAL: for (i = 0; i < enriv; i++) if (ralive[i]) return 0; return nb_of[B_ARCO] != 0;
  default: return cmile >= 5;
  }
}
static void mis_tick(void) {                       // once a month (city) or cycle (empire): win, lose, or go on
  const Mis *m; u8 to;
  if (!mis_on || cquiet) return;
  m = &MIS[mis_i];
  to = (u8)(m->lim && (ecity ? cmon >= m->lim : (u16)(ecyc * 4 / 60) >= m->lim));
  if (mis_end_goal(m->goal)) { if (to) { if (mis_ok()) { if (ecity) city_end(1, 23); else end_game(1, 11); } else { if (ecity) city_end(0, 21); else end_game(0, 12); } } return; }
  if (mis_ok()) { if (ecity) city_end(1, 23); else end_game(1, 11); }
  else if (to) { if (ecity) city_end(0, 21); else end_game(0, 12); }
}
static void mis_result(u8 win) {
  const Mis *m = &MIS[mis_i];
  hide_all(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full();
  big(2, win ? "MISSION DONE!" : "MISSION FAILED");
  ps((u8)(15 - (blen2(m->name) + 1) / 2), 5, m->name, 9);
  pm_rule(7);
  ps(2, 8, win ? "WELL DONE, ANTPEROR!" : "NOT THIS TIME. TRY AGAIN!", win ? 13 : 8);
  ps(2, 10, "MODE", 10); ps(9, 10, MKN[m->kind], MKC[m->kind]);
  if (m->kind >= MK_CITY) { ps(2, 11, "ANTS", 10); pnum(9, 11, cpop, 3); ps(2, 12, "YEARS", 10); pnum(9, 12, (u32)(cmon / 12), 3); }
  else if (m->kind == MK_EMPIRE) { ps(2, 11, "ANTS", 10); pnum(9, 11, pop, 3); ps(2, 12, "MINUTES", 10); pnum(10, 12, (u32)(ecyc * 4 / 60), 3); }
  else { ps(2, 11, "TIME", 10); pnum(9, 11, (u32)gt * 2 / 15 / 60, 3); ps(12, 11, "MIN", 10); }
  if (win) { u8 had = mis_done(mis_i); mis_mark(mis_i); if (!had) { ps(2, 14, "MISSIONS DONE:", 10); pn(17, 14, mis_count(), 9); ps(20, 14, "OF 50", 10); }
    if (mis_count() >= 10) ach_get(62); if (mis_count() >= NMIS) ach_get(63); }
  pm_rule(16); ps(7, 18, "PRESS \5 START", 9);
  DISPCNT = 0x1B40; BLDY = 0;
  if (win) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  ewait_clear(); while (!(joy() & (J_START | J_A))) vsync(); ewait_clear();
  fade(0, 16);
}
static void mission_run(u8 i) {
  const Mis *m = &MIS[i]; u8 sm = mapt, sw = tw, sh = th, sdiff = diff, win = 0, quit = 0, k;
  const char *L[6] = {m->s[0], m->s[1], m->s[2], 0, 0, 0};
  mis_on = 1; mis_i = i; mis_sav_pzw = pzw;
  story_card(m->name, L, "\1 BEGIN");
  mapt = m->map; tw = m->w; th = m->h;
  if (m->pzm == 0) pzw = 2; else if (m->pzm == 1) pzw = 1;
  switch (m->kind) {
  case MK_CLASSIC: case MK_EYE:
    diff = (m->tw & TW_HARD) ? 2 : 1; sandbox = 0; tutor = 0; emode = 0; ecity = 0; PZ_.on = 0;
    newgame(); play();
    win = (u8)(over == 4 || (over == 1 && mis_ok())); quit = over == 3;
    break;
  case MK_EMPIRE:
    emode = 1; ecity = 0; echap = 255; enriv = m->riv; eaggr = m->aggr; PZ_.on = 0;
    emp_newgame(); emp_play();
    win = eend == 1; quit = eend == 3; emode = 0;
    break;
  default:
    ecity = (u8)(m->kind == MK_VERSE ? 2 : 1); cscen = 255; cmoney = m->money; cdis_on = m->dis; enriv = m->riv; eaggr = m->aggr; echap = 255;
    city_newgame();
    if (m->tw & TW_DEBT) cash = -1500;
    if (m->tw & TW_RICH) cash = 20000;
    if (m->tw & TW_STARTER) { starter_town((u8)(m->goal == MG_CSMOG ? 1 : m->goal == MG_CCRIME ? 2 : 3)); { u8 r; for (r = 0; r < H; r++) scan_rows(1); } cquiet = 1; city_cycle(); cquiet = 0; cmon = 0; nh2 = 0; }
    if (m->tw & TW_FIRES) for (k = 0; k < 4; k++) fire_any();
    if (PZ_.on) {
      if (m->tw & TW_REBELS) PZ_.reb = 120;
      if (m->tw & TW_ANGRY) for (k = 0; k < NFAC; k++) PZ_.sup[k] = 30;
      if (m->tw & TW_HOSTILE) { PZ_.rel[0] = PZ_.rel[1] = 15; }
    }
    msgt = 0;
    if (m->kind == MK_VERSE) city_goal_card();
    city_play();
    win = eend == 1; quit = eend == 3;
    break;
  }
  music_stop();
  if (!quit) mis_result(win); else fade(0, 16);
  mis_on = 0; ecity = 0; emode = 0; PZ_.on = 0; pzw = mis_sav_pzw;
  mapt = sm; tw = sw; th = sh; diff = sdiff; eend = 0; over = 0;
}
static void missions_menu(void) {
  static u8 s = 0; u8 top = 0, i; u16 fr = 0;
  static const char *const PW[3] = {"OPTIONS", "ON", "OFF"};
  ewait_clear();
  for (;;) {
    const Mis *m = &MIS[s];
    DISPCNT = 0x80; hide_all(); oam_flush();
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "MISSIONS");
    ps(1, 2, "DONE", 10); pn(6, 2, mis_count(), 9); ps(9, 2, "OF 50", 10); ps(19, 2, "\3\4 JUMP 5", 10);
    if (s < top) top = s; if (s >= top + 5) top = (u8)(s - 4);
    if (top) pc(29, 3, '-', 10); if (top + 5 < NMIS) pc(29, 11, '+', 10);
    for (i = top; i < NMIS && i < top + 5; i++) { u8 y = (u8)(3 + (i - top) * 2); const Mis *q = &MIS[i];
      if (i == s) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      pn(2, y, (u8)(i + 1), 10); ps(5, y, q->name, i == s ? 9 : 3); ps(21, y, MKN[q->kind], MKC[q->kind]); if (mis_done(i)) pc(29, y, '$', 9); }
    pm_rule(13);
    ps(1, 14, m->s[0], 3); ps(1, 15, m->s[1], 3); ps(1, 16, m->s[2], 3);
    if (m->lim) { ps(1, 17, "LIMIT", 10); pnum(7, 17, m->kind >= MK_CITY ? (u32)(m->lim / 12) : m->lim, 9); ps(10, 17, m->kind >= MK_CITY ? "YEARS" : "MIN", 10); }
    else ps(1, 17, "NO LIMIT", 10);
    ps(17, 17, "PALACE:", 10); ps(25, 17, m->pzm == 0 ? "OFF" : m->pzm == 1 ? "ON" : PW[pzw], m->pzm == 0 ? 15 : 13);
    if (m->pzm == 2) ps(1, 18, "< > PALACE ON OR OFF", 10);
    ps(1, 19, "\1 PLAY   \2 BACK", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    while (!ekey()) fr++;
    if (emp_ & J_B) break;
    if (emp_ & J_UP) s = (u8)(s ? s - 1 : NMIS - 1);
    if (emp_ & J_DOWN) s = (u8)(s + 1 >= NMIS ? 0 : s + 1);
    if (emp_ & J_L) s = (u8)(s >= 5 ? s - 5 : 0);
    if (emp_ & J_R) s = (u8)(s + 5 < NMIS ? s + 5 : NMIS - 1);
    if ((emp_ & (J_LEFT | J_RIGHT)) && m->pzm == 2) { pzw = (u8)((pzw + ((emp_ & J_RIGHT) ? 1 : 2)) % 3); sfx_food(); }
    if (emp_ & J_A) { ewait_clear(); mission_run(s); ecity = 2; ewait_clear(); }
  }
  ewait_clear();
}
