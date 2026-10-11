// ============================================================================================================================
//  2 PLAYERS (included from empire.c, after city.c): every mode can be played by two.
//   VERSUS        classic game: P1 shapes the land for the black ants, P2 for the red ants. First queen to fall loses.
//   CO-OP CLASSIC classic game: two gods share the black colony's AP against the red ants
//   CO-OP EMPIRE  one empire, two cursors (free play or a story chapter)
//   CO-OP CITY    one ANT CITY, two mayors (free city or a scenario)
//   CO-OP ANTVERSE  everything at once, together
//  PLAY ON:  SAME GBA  = take turns (a turn clock; "pass the GBA" between turns)
//            LINK CABLE = two GBAs at the same time. The host's settings, seed, options and cheats are sent to the guest,
//                         then both GBAs run in lockstep (see joy() in main.c). Menus are shared: both screens show the
//                         menu of whoever opened it.
// ============================================================================================================================
static u8 mp_how, mp_sub = 255, mp_size = 1, mp_riv = 1, mp_sel;
static const u8 MPSZ[4] = {32, 48, 64, 96};
static const char *const MPK[5] = {"VERSUS", "CO-OP EMPIRE", "CO-OP CITY", "CO-OP ANTVERSE", "CO-OP CLASSIC"};
static const char *const MPD[5] = {"BLACK ANTS AGAINST RED ANTS", "ONE EMPIRE, TWO RULERS", "ONE CITY, TWO MAYORS", "EVERYTHING, TOGETHER", "TWO GODS, ONE ANT COLONY"};
static const u8 MPORD[5] = {0, 4, 1, 2, 3};          // the order LEFT / RIGHT steps through the games
#define MP_BUILD ((u16)((__TIME__[0] * 7 + __TIME__[1] * 13 + __TIME__[3] * 17 + __TIME__[4] * 19 + __TIME__[6] * 23 + __TIME__[7] * 29 + __DATE__[4] * 31 + __DATE__[5] * 37) & 0xFFF))
static u8 mp_locked(u8 k) { return (k == 1 && !emp_unlocked()) || (k == 3 && !verse_unlocked()); }

// ---------- VERSUS (classic) ----------
static void vs_swap(void) { u8 t = mana; mana = mana2; mana2 = t; t = stock[0]; stock[0] = stock[1]; stock[1] = t; }   // P2 acts with the red AP and food
static void co_hud(void) {                          // CO-OP CLASSIC: the usual HUD + whose cursor (+ the turn clock in place of the game clock)
  u8 me = mp_local();
  hud();
  if (mp_on == 1) { u8 s = (u8)(mp_tleft / 60 > 99 ? 99 : mp_tleft / 60); ps(22, 18, "      ", 1); pc(22, 18, '~', 9); pn(23, 18, s, s < 10 ? 8 : 9); }
  pc(28, 18, 'P', me ? 8 : 13); pc(29, 18, (char)('1' + me), me ? 8 : 13);
}
static void vs_hud(void) {
  u8 i, me = mp_local(), m = me ? mana2 : mana;
  if (mp_kind) { co_hud(); return; }
  ov_fill(1, 1, 17, 19);
  ps(0, 17, "P1", 13); for (i = 0; i < qbase; i++) pc((u8)(3 + i), 17, '#', i < qhp[0] ? 8 : 15); pc(9, 17, '@', 1); pn(10, 17, ncnt[0], 1);
  ps(15, 17, "P2", 8); for (i = 0; i < qbase; i++) pc((u8)(18 + i), 17, '#', i < qhp[1] ? 8 : 15); pc(24, 17, '@', 8); pn(25, 17, ncnt[1], 8);
  pc(0, 18, '&', 14); pn(1, 18, m, 14);
  for (i = 0; i < 10; i++) BGMAP1[18 * 32 + 4 + i] = (u16)(((u16)m * 10 / MANA_MAX > i ? BAR_F : BAR_E) | (14 << 12));
  if (mp_on == 2) ps(16, 18, me ? "YOU: RED" : "YOU: BLACK", me ? 8 : 13);
  else { ps(16, 18, me ? "P2 TURN" : "P1 TURN", me ? 8 : 13); pn(26, 18, (u8)(mp_tleft / 60 > 99 ? 99 : mp_tleft / 60), mp_tleft < 600 ? 8 : 9); }
  if (msgt) { psx(0, 19, msg, 2); msgt--; } else cur_info();
}
static void vs_redraw(void) {                       // after a menu or a pass card
  hide_all(); oam_flush();
  win_all(); ov_clear(); vs_hud();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; BG2CNT = 1 | (2 << 2) | (30 << 8);
  DISPCNT = 0x1F40; BLDY = 0;
  if (!mus_on && !nomus) music_start();
}
static void vs_jump(u8 p) {                         // SELECT: home, your front line, the enemy nest
  PSt *s = &pst[p]; u8 tm = (u8)(mp_kind ? 0 : p), en = (u8)(tm ^ 1), i, bi = MAXA; u16 b = NOPATH, d;
  s->jj = (u8)((s->jj + 1) % 3);
  if (s->jj == 1) {
    for (i = 0; i < MAXA; i++) if (ant[i].alive && ant[i].team == tm) { d = dnp[en][ant[i].y][ant[i].x]; if (d < b) { b = d; bi = i; } }
    if (bi == MAXA) s->jj = 2; else { cx = ant[bi].x; cy = ant[bi].y; say(p ? "P2: YOUR FRONT LINE" : "P1: YOUR FRONT LINE"); }
  }
  if (s->jj == 2) { cx = nestx[en]; cy = nesty[en]; say("THE ENEMY NEST"); }
  if (s->jj == 0) { cx = nestx[tm]; cy = nesty[tm]; say("HOME"); }
  sfx_spawn();
}
static u8 vs_input(u8 p, u16 k) {                   // 0 go on, 1 quit, 2 a menu was used
  PSt *s = &pst[p]; u16 pr = k & ~s->prev; u8 dirs, fire, sw = (u8)(p && !mp_kind);   // sw: P2 of VERSUS acts with the red AP
  s->prev = k;
  if (pr & J_START) {
    lk_focus = p; if (sw) vs_swap(); pm_do = 0; pause_menu(); if (pm_do == 2) offering(); if (sw) vs_swap(); lk_focus = 2; s->prev = 0x3FF;
    if (over == 3) return 1;
    if (pm_do == 1) say("NO ANT EYE WITH 2 PLAYERS");
    vs_redraw(); return 2;
  }
  dirs = (u8)(k & 15);
  if (dirs != s->last) { s->rep = 0; s->last = dirs; }
  if (dirs) {
    fire = (s->rep == 0) || (s->rep >= CDLY[O(O_CURSOR)] && ((s->rep - CDLY[O(O_CURSOR)]) % CRAT[O(O_CURSOR)]) == 0);
    if (s->rep < 250) s->rep++;
    if (fire) {
      if ((dirs & J_LEFT) && cx > 0) cx--;
      if ((dirs & J_RIGHT) && cx < W - 1) cx++;
      if ((dirs & J_UP) && cy > 0) cy--;
      if ((dirs & J_DOWN) && cy < H - 1) cy++;
    }
  }
  if (sw) vs_swap();
  if (k & J_SEL) {                                 // SELECT + A: trade food for AP; SELECT alone: jump
    if (pr & J_SEL) s->selused = 0;
    if (pr & J_A) { offering(); s->selused = 1; }
    if (dirs) s->selused = 1;
  } else {
    if (s->selprev && !s->selused) { if (sw) vs_swap(); vs_jump(p); if (sw) vs_swap(); }
    if (pr & J_A) raise_land();
    if (pr & J_B) lower_land();
  }
  if (pr & J_L) flood();
  if (sw) vs_swap();
  s->selprev = (u8)(k & J_SEL);
  return 0;
}
static void vs_end(void) {
  u8 win = over == 1 ? 0 : 1, n; u32 s = (u32)gt * 2 / 15;
  hide_all(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  if (mp_on == 2) big(2, win == lk_me ? "YOU WIN!" : "YOU LOSE");
  else big(2, win ? "PLAYER 2 WINS!" : "PLAYER 1 WINS!");
  ps(3, 5, timeup ? "TIME IS UP: MORE ANTS WINS" : win ? "THE RED QUEEN RULES!" : "THE BLACK QUEEN RULES!", win ? 8 : 13);
  pm_rule(7);
  ps(3, 9, "TIME", 10); pn(14, 9, (u8)(s / 60 > 99 ? 99 : s / 60), 3); pc(16, 9, ':', 3); pz(17, 9, (u8)(s % 60), 3);
  ps(3, 11, "P1 BLACK ANTS", 13); pn(20, 11, ncnt[0], 3);
  ps(3, 13, "P2 RED ANTS", 8); pn(20, 13, ncnt[1], 3);
  ps(3, 15, "ANTS BEATEN", 10); pu(19, 15, st_kill, 3);
  ps(7, 18, "PRESS \5 START", 9);
  if (mp_on == 1 || win == lk_me) ach_get(47);
  if (win == mp_local() || mp_on == 1) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  for (n = 0; n < 2; n++) vsync();
  wait_keys(J_START);
  while (joy()) vsync();
  fade(0, 16);
}
static void versus_play(void) {                     // VERSUS, and CO-OP CLASSIC (mp_kind 4: both players help the black ants)
  u16 k; u8 i, n, t = 0, r = 0, co = (u8)(mp_kind != 0), h2 = (u8)(co ? 0 : 1);
  if (!co) hc1 = hc0;                              // versus: the same rules for both colonies
  ps_init((u8)(nestx[0] + 2 < W ? nestx[0] + 2 : nestx[0]), (u8)(nesty[0] > 2 ? nesty[0] - 2 : nesty[0]),
          (u8)(nestx[h2] > 2 ? nestx[h2] - 2 : nestx[h2]), (u8)(nesty[h2] + 2 < H ? nesty[h2] + 2 : nesty[h2]));
  mana2 = mana;
  ps_load(mp_local()); follow(); scx = (s16)camx * 8; scy = (s16)camy * 8; win_all();
  if (mp_on == 1) { mp_pass_card(0); }
  vs_redraw(); say(co ? "TWO GODS, ONE COLONY!" : "P1 BLACK  AGAINST  P2 RED!");
  while (joy()) vsync();
  while (!over) {
    vsync(); frc++;
    shx = (shk && !O(O_SHAKE)) ? ((shk & 2) ? 2 : -2) : 0; if (shk) shk--;
    BG0HOFS = BG2HOFS = (u16)(scx + shx); BG0VOFS = BG2VOFS = (u16)scy; oam_flush();
    BGPAL[176 + 3] = (frc & 32) ? RGB(14,22,31) : RGB(10,18,30); BGPAL[176 + 14] = (frc & 32) ? RGB(10,18,30) : RGB(14,22,31);
    OBJPAL[32 + 3] = RGB(31, 26 + ((frc >> 2) & 3), (frc & 16) ? 4 : 12);
    for (n = 0; n < 2; n++) { u8 mx; for (mx = wx0; mx < (u8)(wx0 + 31) && mx < W; mx++) draw_dec(mx, (u8)(wy0 + hrow)); hrow = (u8)((hrow + 1) % 19); }
    if (alt) alt--;
    if (pend) { lk_focus = 2; perk_card(); vs_redraw(); continue; }   // co-op: an antlection won, pick a perk (either player)
    mp_hash();
    k = joy();
    if (lk_err) { over = 3; break; }
    for (i = 0; i < (mp_on == 2 ? 2 : 1); i++) {
      u8 p = mp_on == 2 ? i : mp_turn;
      ps_load(p); r = vs_input(p, mp_on == 2 ? lk_k[p] : k); ps_save(p);
      if (r) break;
    }
    if (r == 1) { over = 3; break; }
    ps_load(mp_local());
    if (r == 2) continue;
    if (mp_turn_tick()) { vs_redraw(); continue; }
    if (cht[0]) mana = mana2 = MANA_MAX;
    if (co && cht[1] && qhp[0] < qmax0) qhp[0] = qmax0;
    if (co && !(frc & 31)) { if (appr >= 99) ach_get(9); if (ncnt[0] >= 24) ach_get(10); }
    party_tick();
    follow(); scroll_step(); win_sync();
    bfs_tick(450);
    n = SPN[spd]; if (n && (sf++ & SPM[spd])) n = 0;
    for (; n; n--) tick_slice();
    if (++t >= 8 || (mp_on == 2 ? lk_k[lk_me] : k)) { t = 0; count(ncnt); vs_hud(); }
    if (!qhp[1]) over = 1; else if (!qhp[0]) over = 2;
    if (O(O_TIME) && !over && (u32)gt * 2 / 15 >= (O(O_TIME) == 1 ? 300u : O(O_TIME) == 2 ? 600u : 1200u)) {
      timeup = 1; over = (u8)(ncnt[0] + qhp[0] * 3 >= ncnt[1] + qhp[1] * 3 ? 1 : 2);
    }
    draw_sprites(); mp_cursor2();
  }
  music_stop();
  if (over == 3) { fade(0, 16); return; }
  for (n = 0; n < 60; n++) { vsync(); frc++; draw_sprites(); oam_flush(); }
  if (!co) { vs_end(); return; }
  end_card();                                      // co-op: the classic end card (stats, graph, records)
  if (over == 1) { ach_get(46); jingle(WIN_TUNE, 6); } else jingle(LOSE_TUNE, 4);
  vsync(); oam_flush();
  wait_keys(J_START);
  while (joy()) vsync();
  fade(0, 16);
}

// ---------- the link lobby: find the other GBA, agree on the game ----------
static void mp_seed(u32 sd) {                       // the same random numbers on both GBAs
  u8 i; if (!sd) sd = 1;
  rs = sd; for (i = 0; i < 10; i++) mst[i] = sd * 2654435761u + i * 0x9E3779B9u + (sd >> (i + 3)); mpi = 0; genm = 0;
}
static void mp_msg(const char *a, const char *b, u8 col) {
  ov_fill(1, 3, 8, 13); ps((u8)(15 - (blen2(a) + 1) / 2), 9, a, col); if (b) ps((u8)(15 - (blen2(b) + 1) / 2), 11, b, 3);
}
static u8 mp_link_start(u32 *seed) {               // 1 = linked; the host's settings are now on both GBAs
  u16 o = 0, v[80]; u8 i, n = 0, dots = 0, ok;
  hide_all(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full(); big(1, "LINK CABLE");
  ps(2, 4, "PLUG IN A LINK CABLE.", 3);
  ps(2, 5, "ON THE OTHER GBA, PICK", 3);
  ps(2, 6, "2 PLAYERS: LINK CABLE: START", 3);
  ps(1, 19, "\2 CANCEL", 10);
  DISPCNT = 0x1B40; BLDY = 0;
  lk_open();
  for (i = 0; i < 8; i++) vsync();
  for (;;) {
    if (raw_keys() & J_B) { lk_close(); sfx_deny(); while (raw_keys()) vsync(); return 0; }
    lk_me = (SIOCNTR & 4) ? 1 : 0;
    mp_msg("LOOKING FOR THE OTHER GBA", (dots++ & 1) ? "... ..." : ". . . .", 9);
    lk_seq = 0;
    if (lk_step(0xA5A, &o, 15) && o == 0xA5A) {
      if (lk_step(MP_BUILD, &o, 90)) {
        if (o != MP_BUILD) { mp_msg("DIFFERENT GAME VERSIONS!", "UPDATE BOTH GBAS", 8); for (i = 0; i < 180; i++) vsync(); lk_close(); return 0; }
        break;
      }
    }
    vsync();
  }
  mp_msg(lk_me ? "LINKED! YOU ARE PLAYER 2" : "LINKED! YOU ARE PLAYER 1", lk_me ? "THE HOST PICKS THE GAME" : "SENDING THE GAME...", 13);
  sfx_mana();
  // the host's choices: what, map, seed, options, cheats
  if (!lk_me) {
    v[n++] = mp_locked(mp_kind) ? 0xFFF : mp_kind;   // (0xFFF: a game the host has not opened yet: call it off)
    v[n++] = mp_sub; v[n++] = mapt; v[n++] = tw; v[n++] = th; v[n++] = mp_riv; v[n++] = cmoney; v[n++] = cdis_on;
    v[n++] = (u16)(*seed & 0xFFF); v[n++] = (u16)((*seed >> 12) & 0xFFF); v[n++] = (u16)(*seed >> 24); v[n++] = (u16)(diff | (pzw << 8));
    for (i = 0; i < NOPT; i++) v[n++] = opt[i];
    for (i = 0; i < NCHT; i++) v[n++] = cht[i];
  } else n = (u8)(12 + NOPT + NCHT);
  ok = 1;
  for (i = 0; i < n && ok; i++) { if (!lk_step(lk_me ? 0 : v[i], &o, 240)) ok = 0; else if (lk_me) v[i] = o; }
  if (!ok) { mp_msg("THE LINK WAS LOST", "CHECK THE CABLE", 8); for (i = 0; i < 150; i++) vsync(); lk_close(); return 0; }
  if (v[0] == 0xFFF) { mp_msg(lk_me ? "HOST PICKED A LOCKED GAME" : "THAT GAME IS LOCKED HERE", "PICK ANOTHER GAME", 8); for (i = 0; i < 150; i++) vsync(); lk_close(); return 0; }
  if (lk_me) {
    u8 j = 0;
    mp_kind = (u8)v[j++]; mp_sub = (u8)v[j++]; mapt = (u8)v[j++]; tw = (u8)v[j++]; th = (u8)v[j++]; mp_riv = (u8)v[j++]; cmoney = (u8)v[j++]; cdis_on = (u8)v[j++];
    *seed = (u32)v[j] | ((u32)v[j + 1] << 12) | ((u32)v[j + 2] << 24); j += 3; diff = (u8)(v[j] & 0xFF); pzw = (u8)(v[j] >> 8); j++;
    for (i = 0; i < NOPT; i++) opt[i] = (u8)v[j++];
    for (i = 0; i < NCHT; i++) cht[i] = (u8)v[j++];
    opts_apply();
  }
  for (i = 0; i < 40; i++) vsync();
  ach_get(48);
  return 1;
}

// ---------- starting a 2 player game of any kind ----------
static void mp_reset_ui(void) {                     // menu memories that would make the two GBAs' menus differ
  eptab = epsel = 0; ctab = csel2 = 0; em_sel = cm_sel = 0; hsel = 0; tsel = 0; pm_do = 0; frc = 0; alt = 0; msgt = 0; spd = 4; sf = 0;
}
static void mp_run(u8 how) {                        // how: 1 same GBA, 2 link cable
  u8 sopt[NOPT], scht[NCHT], i; u32 seed = rs ^ ((u32)frc << 16) ^ 0x5EED1234u;
  for (i = 0; i < NOPT; i++) sopt[i] = opt[i];
  for (i = 0; i < NCHT; i++) scht[i] = cht[i];
  if (mp_sub == 255 || mp_kind == 0 || mp_kind >= 3) { tw = th = MPSZ[mp_size]; if (mapt == MMY && (how == 2 || !has_my)) mapt = MINF; }
  if (how == 2) { if (!mp_link_start(&seed)) { lk_close(); return; } }
  mp_on = how; mp_reset_ui(); mp_seed(seed);
  if (how == 2) { lk_on = 1; lk_focus = 2; }
  tutor = 0; sandbox = 0; tut = 0;
  switch (mp_kind) {
  case 0:
    opt[O_ELECT] = 1;                              // no elections or perks: the land is the whole game
    if (mp_on) for (i = 0; i < NCHT; i++) if (i != 6) cht[i] = 0;   // a fair fight (party colours are fine)
    newgame(); versus_play();
    break;
  case 4:                                          // CO-OP CLASSIC: the host's options and cheats, both gods on the black side
    newgame(); versus_play();
    break;
  case 1:
    emode = 1; ecity = 0; echap = mp_sub; enriv = (u8)(echap == 255 ? mp_riv : 0); eaggr = 1;
    if (echap != 255) { char t1[12] = "CHAPTER "; t1[8] = (char)('1' + echap); t1[9] = 0; story_card(t1, CH[echap].s1, CH[echap].name); story_card("CHITIN", CH[echap].s2, "\1 BEGIN"); }
    emp_newgame(); emp_play(); if (eend == 1) ach_get(46); emp_end();
    emode = 0;
    break;
  case 2: case 3:
    ecity = (u8)(mp_kind == 2 ? 1 : 2); cscen = mp_kind == 2 ? mp_sub : 255;
    if (mp_kind == 3) { enriv = mp_riv; eaggr = 1; }
    if (cscen != 255) { const char *L[6] = {SC[cscen].s[0], SC[cscen].s[1], SC[cscen].s[2], SC[cscen].s[3], 0, 0}; cmoney = 1; story_card(SC[cscen].name, L, "\1 BEGIN"); }
    city_newgame(); if (mp_kind == 3) city_goal_card(); city_play(); if (eend == 1) ach_get(46); city_end_screen();
    ecity = 0; emode = 0;
    break;
  }
  if (lk_err) {                                      // the cable came out (or the two worlds drifted apart)
    hide_all(); oam_flush(); DISPCNT = 0x80;
    ov_fill(1, 3, 0, 19); ui_full(); big(2, "LINK LOST");
    ps(4, 6, lk_err == 2 ? "THE GAMES WENT OUT OF SYNC" : "THE OTHER GBA STOPPED", 8);
    ps(4, 8, "CHECK THE CABLE AND TRY", 3); ps(4, 9, "AGAIN", 3);
    DISPCNT = 0x1B40; BLDY = 0;
    lk_err = 0; for (i = 0; i < 200; i++) vsync();
  }
  lk_close(); lk_err = 0; mp_on = 0;
  for (i = 0; i < NOPT; i++) opt[i] = sopt[i];
  for (i = 0; i < NCHT; i++) cht[i] = scht[i];
  opts_apply();
  while (raw_keys()) vsync();
}

// ---------- the 2 PLAYERS screen ----------
static void mp_draw(void) {
  u8 i, y; const char *w;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "2 PLAYERS"); pm_rule(2);
  for (i = 0; i < 7; i++) { y = (u8)(3 + i * 2); if (i == mp_sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); }
  ps(3, 3, "GAME", mp_sel == 0 ? 9 : 3); ps(12, 3, "<", 10); ps(14, 3, MPK[mp_kind], !mp_how && mp_locked(mp_kind) ? 15 : 9); ps(29, 3, ">", 10);
  ps(3, 5, "PLAY ON", mp_sel == 1 ? 9 : 3); ps(12, 5, "<", 10); ps(14, 5, mp_how ? "LINK CABLE" : "SAME GBA", 9); ps(29, 5, ">", 10);
  // what to play
  ps(3, 7, mp_kind == 0 || mp_kind == 3 || mp_sub == 255 ? "MAP" : mp_kind == 1 ? "STORY" : "SCENARIO", mp_sel == 2 ? 9 : 3); ps(12, 7, "<", 10); ps(29, 7, ">", 10);
  if (mp_kind == 1) { if (mp_sub == 255) ps(14, 7, "FREE PLAY", 9); else { ps(14, 7, "CHAPTER", 9); pc(22, 7, (char)('1' + mp_sub), 9); } }
  else if (mp_kind == 2) { if (mp_sub == 255) ps(14, 7, "FREE CITY", 9); else { ps(14, 7, "NUMBER", 9); pn(21, 7, (u8)(mp_sub + 1), 9); } }
  if (mp_kind == 0 || mp_kind == 3 || mp_sub == 255) { w = mname(mapt); if (mp_kind == 1 || mp_kind == 2) ps(14, 8, w, 10); else ps(14, 7, w, 9); }
  else ps(14, 8, mp_kind == 1 ? CH[mp_sub].name : SC[mp_sub].name, 10);
  ps(3, 9, "SIZE", mp_sel == 3 ? 9 : 3);
  if (mp_kind == 0 || mp_kind == 3 || mp_sub == 255) { static const char *const SZN[4] = {"SMALL", "MEDIUM", "LARGE", "HUGE"};
    ps(12, 9, "<", 10); ps(14, 9, SZN[mp_size], 9); pn(21, 9, MPSZ[mp_size], 10); ps(29, 9, ">", 10); }
  else ps(14, 9, "SET BY THE GAME", 15);
  ps(3, 11, mp_kind == 2 ? "MONEY" : "RIVALS", mp_sel == 4 ? 9 : 3);
  if (mp_kind == 0) ps(14, 11, "EACH OTHER!", 15);
  else if (mp_kind == 4) ps(14, 11, "THE RED ANTS", 8);
  else if (mp_kind == 2) { static const char *const MN[4] = {"EASY", "NORMAL", "HARD", "SANDBOX"}; ps(12, 11, "<", 10); ps(14, 11, mp_sub == 255 ? MN[cmoney] : "SET", 9); ps(29, 11, ">", 10); }
  else if (mp_sub == 255 || mp_kind == 3) { ps(12, 11, "<", 10); pc(14, 11, (char)('0' + mp_riv), 9); ps(29, 11, ">", 10); }
  else ps(14, 11, "SET BY THE GAME", 15);
  ps(3, 13, "TURNS", mp_sel == 5 ? 9 : 3);
  if (mp_how) ps(14, 13, "ALL AT ONCE!", 13);
  else { static const char *const TL[4] = {"30", "60", "90", "120"}; ps(12, 13, "<", 10); ps(14, 13, TL[mp_tlen], 9); ps(18, 13, "SECONDS", 10); ps(29, 13, ">", 10); }
  { u8 lk = (u8)(!mp_how && mp_locked(mp_kind));   // (on a link cable the host's unlocks count)
    const char *d = lk ? (mp_kind == 1 ? "WIN A GAME TO OPEN EMPIRE" : "WIN ALL 10 CITY SCENARIOS") : MPD[mp_kind];
    ps(3, 15, lk ? "LOCKED" : "START!", mp_sel == 6 ? 9 : lk ? 15 : 13);
    ps((u8)(15 - (blen2(d) + 1) / 2), 17, d, lk ? 8 : 2); }
  ps(1, 19, "< > CHANGE  \1 GO  \2 BACK", 10);
}
static void mp_menu(void) {
  u16 k, p, prev = raw_keys();
  hide_all(); oam_flush(); DISPCNT = 0x80;
  if (mapt == MMY) mapt = 0;
  mp_draw(); DISPCNT = 0x1B40; BLDY = 0;
  for (;;) {
    vsync(); k = raw_keys(); p = k & ~prev; prev = k;
    if (!p) continue;
    if (p & J_B) break;
    if (p & J_UP) { mp_sel = (u8)(mp_sel ? mp_sel - 1 : 6); sfx_food(); }
    if (p & J_DOWN) { mp_sel = (u8)(mp_sel >= 6 ? 0 : mp_sel + 1); sfx_food(); }
    if (p & (J_LEFT | J_RIGHT)) {
      s8 d = (p & J_RIGHT) ? 1 : -1;
      switch (mp_sel) {
      case 0: { u8 j = 0; while (j < 4 && MPORD[j] != mp_kind) j++; mp_kind = MPORD[(j + 5 + d) % 5]; mp_sub = 255; } break;
      case 1: mp_how ^= 1; break;
      case 2:
        if (mp_kind == 1) { u8 top = (u8)(SRAM[63] < NCH ? SRAM[63] : NCH - 1); mp_sub = (u8)(mp_sub == 255 ? (d > 0 ? 0 : top) : (mp_sub + d > top || (s8)(mp_sub + d) < 0) ? 255 : mp_sub + d); }
        else if (mp_kind == 2) { mp_sub = (u8)(mp_sub == 255 ? (d > 0 ? 0 : NSCEN - 1) : (mp_sub + d >= NSCEN || (s8)(mp_sub + d) < 0) ? 255 : mp_sub + d); }
        else { do mapt = (u8)((mapt + MAPN + d) % MAPN); while (mapt == MMY); }
        break;
      case 3: mp_size = (u8)((mp_size + 4 + d) & 3); break;
      case 4: if (mp_kind == 2) cmoney = (u8)((cmoney + 4 + d) & 3); else if (mp_kind && mp_kind != 4) mp_riv = (u8)(mp_riv + d < 1 ? 3 : mp_riv + d > 3 ? 1 : mp_riv + d); break;
      case 5: if (!mp_how) mp_tlen = (u8)((mp_tlen + 4 + d) & 3); break;
      }
      sfx_food();
    }
    if ((p & J_START) || ((p & J_A) && mp_sel == 6)) {
      if (!mp_how && mp_locked(mp_kind)) { sfx_deny(); continue; }   // (on a link cable the host's unlocks decide)
      sfx_mana();
      mp_run((u8)(mp_how ? 2 : 1));
      vram_reload(); hide_all(); oam_flush(); DISPCNT = 0x80;
      prev = raw_keys();
      mp_draw(); DISPCNT = 0x1B40; BLDY = 0;
      continue;
    }
    mp_draw();
  }
  while (raw_keys()) vsync();
}
