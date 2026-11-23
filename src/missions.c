// ============================================================================================================================
//  ANTVERSE MISSIONS (included from city.c): 56 missions picked from the ANTVERSE menu. Some use one mode only (classic,
//  ANT EYE, EMPIRE, ANT CITY, EL PRESIDANTE), the rest mix everything. Each has a goal, a map and (mostly) a time limit.
//  Finished missions are marked on the cartridge (SRAM 9350: 'M' 'I' + 7 bytes of bits).
// ============================================================================================================================
#include "mission_catalog.h"
static const char *const MKN[6] = {"CLASSIC", "ANT EYE", "EMPIRE", "CITY", "PALACE", "VERSE"};
static const u8 MKC[6] = {13, 14, 9, 2, 8, 1};
#define MSO 9350
static u8 mis_on, mis_i, mis_sav_pzw;
static u8 mis_total(void) { return (u8)NMIS; }
static u8 mis_done(u8 i) { return mission_progress_done(SRAM + MSO, i); }
static u8 mis_count(void) { return mission_progress_count(SRAM + MSO); }
static void mis_mark(u8 i) { mission_progress_mark(SRAM + MSO, i); }
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
  if (win) { u8 had = mis_done(mis_i); mis_mark(mis_i); if (!had) { ps(2, 14, "MISSIONS DONE:", 10); pn(17, 14, mis_count(), 9); ps(20, 14, "OF", 10); pn(23, 14, mis_total(), 10); }
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
    ps(1, 2, "DONE", 10); pn(6, 2, mis_count(), 9); ps(9, 2, "OF", 10); pn(12, 2, mis_total(), 10); ps(19, 2, "\3\4 JUMP 5", 10);
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
    /* The tiny '
    ps(1, 19, "\1 PLAY   \2 BACK", 10);
    ps(17, 19, "SEL NEXT OPEN", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    while (!ekey()) fr++;
    if (emp_ & J_B) break;
    if (emp_ & J_UP) s = (u8)(s ? s - 1 : NMIS - 1);
    if (emp_ & J_DOWN) s = (u8)(s + 1 >= NMIS ? 0 : s + 1);
    if (emp_ & J_L) s = (u8)(s >= 5 ? s - 5 : 0);
    if (emp_ & J_R) s = (u8)(s + 5 < NMIS ? s + 5 : NMIS - 1);
    /* Go to the next unbeaten goal using existing SRAM completion bits.
       SELECT never edits records or blocks manual UP/DOWN browsing. */
    if (emp_ & J_SEL) {
      u8 next = mission_progress_next_open(SRAM + MSO, s);
      if (next != s) { s = next; sfx_food(); }
    }
    if ((emp_ & (J_LEFT | J_RIGHT)) && m->pzm == 2) { pzw = (u8)((pzw + ((emp_ & J_RIGHT) ? 1 : 2)) % 3); sfx_food(); }
    if (emp_ & J_A) { ewait_clear(); mission_run(s); ecity = 2; ewait_clear(); }
  }
  ewait_clear();
}
 tick in the list is easily missed. Confirm the
       selected mission's state in words, including old SRAM records. */
    ps(1, 18, mis_done(s) ? "COMPLETED" : "NOT COMPLETED", mis_done(s) ? 13 : 9);
    if (m->pzm == 2) ps(17, 18, "< > PALACE", 10);
    ps(1, 19, "\1 PLAY   \2 BACK", 10);
    ps(17, 19, "SEL NEXT OPEN", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    while (!ekey()) fr++;
    if (emp_ & J_B) break;
    if (emp_ & J_UP) s = (u8)(s ? s - 1 : NMIS - 1);
    if (emp_ & J_DOWN) s = (u8)(s + 1 >= NMIS ? 0 : s + 1);
    if (emp_ & J_L) s = (u8)(s >= 5 ? s - 5 : 0);
    if (emp_ & J_R) s = (u8)(s + 5 < NMIS ? s + 5 : NMIS - 1);
    /* Go to the next unbeaten goal using existing SRAM completion bits.
       SELECT never edits records or blocks manual UP/DOWN browsing. */
    if (emp_ & J_SEL) {
      u8 next = mission_progress_next_open(SRAM + MSO, s);
      if (next != s) { s = next; sfx_food(); }
    }
    if ((emp_ & (J_LEFT | J_RIGHT)) && m->pzm == 2) { pzw = (u8)((pzw + ((emp_ & J_RIGHT) ? 1 : 2)) % 3); sfx_food(); }
    if (emp_ & J_A) { ewait_clear(); mission_run(s); ecity = 2; ewait_clear(); }
  }
  ewait_clear();
}
