// ============================================================================
// CAT & MOUSE: asymmetric linked multiplayer.
// Every game action is run on BOTH connected GBAs, in the same order. The
// hunter's overhead screen and runner's ANT EYE never mutate world state.
// One round per role; the winner is the best player after the two-round match.
// ============================================================================
#define CHASE_TICKS 5400u                // 90 seconds at the game's 60 Hz VBlank
#define CHASE_GOAL 3
// One 4bpp tile, outside the MAPT 832..975 generated-map atlas.
// Pixel index 1 in palette 1 is black. No image asset/framebuffer needed.
#define CHASE_PANEL_TILE 976
static u8 chase_last_food;   // the mouse's completed deliveries count even if caught
static void chase_food(void) {           // accessible nearby berries, not random unreachable scenery
  s16 x, y; u8 added = 0;
  for (y = (s16)nesty[0] - 8; y <= (s16)nesty[0] + 8; y++)
    for (x = (s16)nestx[0] - 8; x <= (s16)nestx[0] + 8; x++) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      if (!hgt[y][x] || near_nest((u8)x, (u8)y) || food[y][x]) continue;
      if (dnp[0][y][x] == NOPATH || dnp[0][y][x] > 15) continue;
      if (((x * 17 + y * 29) & 7) != 1) continue;
      food[y][x] = 1; if (++added == 15) return;
    }
}
static void chase_rules(u8 round) {
  u16 k;
  u8 hunter = round, runner = (u8)(round ^ 1);
  hide_all(); vsync(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "CAT & MOUSE");
  ps(2, 3, "TWO ROUNDS - SWAP ROLES", 9);
  ps(2, 5, "CAT: OVERHEAD MAP VIEW", 8);
  ps(2, 6, "D-PAD SEEK  A TAG  B SCAN", 3);
  ps(2, 8, "MOUSE: FIRST PERSON VIEW", 13);
  ps(2, 9, "WALK ON % FOOD - BRING TO $", 3);
  ps(2, 10, "DELIVER THREE TO ESCAPE!", 13);
  ps(2, 11, "A SPRINT B SCENT L/R STRAFE", 3);
  ps(2, 13, "CAT TAGS MOUSE TO WIN.", 8);
  ps(2, 14, "CLOCK RUNS OUT: CAT WINS.", 8);
  ps(2, 16, "ROUND", 10); pn(9, 16, (u8)(round + 1), 9);
  if (lk_me == hunter) ps(2, 17, "YOUR JOB: HUNT FROM THE MAP", 8);
  else if (lk_me == runner) ps(2, 17, "YOUR JOB: RUN IN ANT EYE", 13);
  ps(2, 19, "A / START: BEGIN THE CHASE", 9);
  DISPCNT = 0x1B40; BLDY = 0;
  while (1) {
    vsync(); mp_hash(); k = joy();
    if (lk_err || (k & (J_A | J_START))) break;
  }
  while (!lk_err) {
    vsync(); mp_hash(); k = joy();
    if (!(k & (J_A | J_START))) break;
  }
}
static void chase_eye_start(s32 px, s32 py, u8 fa, u8 vi) {
  fill_tile(TILE32 + CHASE_PANEL_TILE * 8, 0x11111111u);
  hide_all(); vsync(); oam_flush();
  floor_pal(); sky_init(); pano_init(); ov_clear(); floor_tiles();
  fox = (s16)(px >> 8) - 32; foy = (s16)(py >> 8) - 32;
  floor_map((volatile u16 *)0x0600B000);
  m7_sky(m7t[0]); m7_sky(m7t[1]);
  m7_build(m7t[0], fa, (s16)px, (s16)py);
  eye_scene(fa, px, py, vi, 0, 0);
  eye_compass(fa, (u8)(px >> 8), (u8)(py >> 8));
  vsync(); eye_arm(m7t[0]); oam_flush(); eye_tab = m7t[0]; eye_irq_on(1);
  BG2CNT = 0x8000 | (22 << 8) | (2 << 2) | 3 | 0x80;
  BLDCNT = 0x2044;
  BG0CNT = 3 | (27 << 8); BG0HOFS = (u16)(fa * 5 + (fa >> 2)); BG0VOFS = 0;
  DISPCNT = 0x1741;
}
static void chase_eye_stop(u16 sky, const u16 *pic) {
  u8 i;
  eye_irq_on(0); vsync();
  DMAR(0, 8) = 0; DMAR(1, 8) = 0; DMAR(2, 8) = 0;
  hide_all(); oam_flush();
  BGPAL[0] = sky; for (i = 0; i < 16; i++) BGPAL[112 + i] = pic[i];
  DISPCNT = 0x80; vsync();
  BG0CNT = 2 | (2 << 2) | (28 << 8);
  BG2CNT = 1 | (2 << 2) | (30 << 8); BLDCNT = 0xFF;
  *(volatile u32 *)0x04000020 = 0x100;
  *(volatile u32 *)0x04000024 = 0x1000000;
  *(volatile u32 *)0x04000028 = 0;
  *(volatile u32 *)0x0400002C = 0;
}
static void chase_map_start(u8 hx, u8 hy) {
  fill_tile(TILE32 + CHASE_PANEL_TILE * 8, 0x11111111u);
  cx = hx; cy = hy;
  camx = 0; camy = H - VH; follow();
  scx = (s16)camx * 8; scy = (s16)camy * 8;
  hide_all(); vsync(); oam_flush();
  win_all(); ov_clear();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy;
  BG2CNT = 1 | (2 << 2) | (30 << 8);
  DISPCNT = 0x1F40; BLDY = 0;
}
// Unlike draw_sprites(), this render function does NOT decrement world
// animation counters. Every world mutation must happen on BOTH devices.
static void chase_map_sprites(u8 hx, u8 hy, u8 rx, u8 ry, u8 scan) {
  u8 i, tl;
  Ant *a;
  for (i = 0; i < MAXA; i++) {
    a = &ant[i];
    if (!a->alive) { spr_hide(i); continue; }
    tl = a->carry ? 5 : a->sol ? 7 : 3;
    if (a->age < 8 && (((frc >> 2) + i) & 1)) tl++;
    place_px(i, tl, a->team, a->ox * 8 + (a->x - a->ox) * a->age,
             a->oy * 8 + (a->y - a->oy) * a->age, a->fl);
  }
  for (i = 0; i < 2; i++) {
    if (qhp[i]) place16(MAXA + i, 16, qfl[i] ? 4 : i,
                         nestx[i] * 8, nesty[i] * 8 - ((frc >> 4) & 1), i);
    else spr_hide(MAXA + i);
  }
  place16(MAXA + 2, 20, 2, hx * 8, hy * 8, 0);
  for (i = 0; i < 8; i++) {
    if (fx[i].t) place_px(MAXA + 3 + i, fx[i].tile, 3, fx[i].x, fx[i].y, 0);
    else spr_hide(MAXA + 3 + i);
  }
  if (scan) place16(MAXA + 11, 20, 8, rx * 8, ry * 8, 0);
  else spr_hide(MAXA + 11);
}
static void chase_hud_map(u8 hunter, u8 delivered, u16 scan_cd, u8 secs) {
  ov_fill(CHASE_PANEL_TILE, 1, 17, 19); // solid dark HUD behind bright text
  ps(0, 17, hunter ? "P2 CAT: MAP HUNTER" : "P1 CAT: MAP HUNTER", 8);
  ps(24, 17, "TIME", 10); pn(28, 17, secs, 9);
  ps(0, 18, "MOUSE FOOD:", 13); pn(13, 18, delivered, 9);
  ps(16, 18, "OF 3", 3);
  if (scan_cd) { ps(22, 18, "SCAN", 8); pn(28, 18, (u8)(scan_cd / 60), 8); }
  else ps(21, 18, "SCAN OK", 13);
  ps(0, 19, "PAD AIM \1 TAG \2 SCAN \5 QUIT", 10);
}
static void chase_hud_eye(u8 runner, u8 delivered, u8 energy, u8 secs, u8 carry) {
  ov_fill(CHASE_PANEL_TILE, 1, 16, 19); // solid dark HUD behind first-person text
  ps(0, 16, runner ? "P2 MOUSE: ANT EYE" : "P1 MOUSE: ANT EYE", 13);
  ps(24, 16, "TIME", 10); pn(28, 16, secs, 9);
  ps(0, 17, carry ? "CARRY FOOD HOME TO $ !" : "FIND % FOOD, RETURN TO $", carry ? 13 : 3);
  ps(0, 18, "HOME DELIVERIES:", 10); pn(18, 18, delivered, 13);
  ps(21, 18, "OF 3", 3); ps(26, 18, "SP", 10); pn(28, 18, energy, 9);
  ps(0, 19, "\1 RUN \2 SCENT L/R SIDE \5 QUIT", 10);
}
// Returns 1 for runner victory, 0 for hunter victory, 2 for quit/link loss.
static u8 chase_round(u8 round) {
  u8 hunter = round, runner = (u8)(round ^ 1), vi, rx, ry, hx, hy;
  u8 deliveries = 0, energy = 99, scan = 0, tag_cd = 0;
  u16 scan_cd = 0;                 // 420 frame cooldown, never truncate to 8 bits
  u8 prevh = 0, prevr = 0, roundout = 0, cur, fa, is_eye;
  u16 sky = BGPAL[0], pic[16], angle = 0, k[2], frame;
  s32 px, py;
  u8 i;
  chase_last_food = 0;
  // Round setup is deterministic and the generated world is identical on
  // both GBAs, independently of which perspective either GBA renders.
  chase_food();
  vi = mp_pilot_near(nestx[0], nesty[0], MAXA);
  if (vi >= MAXA) return 0;  // no workers left to control
  eye_ctl = vi; eye_ctl2 = MAXA;
  px = (s32)ant[vi].x * 256 + 128;
  py = (s32)ant[vi].y * 256 + 128;
  rx = ant[vi].x; ry = ant[vi].y;
  angle = (u16)((ant[vi].fl ? 128u : 0u) << 8);
  hx = nestx[1]; hy = nesty[1];
  is_eye = (u8)(lk_me == runner);
  for (i = 0; i < 16; i++) pic[i] = BGPAL[112 + i];
  if (is_eye) {
    // First-person entry is recorded in the same save-RAM milestone used by
    // the tested two-pilot mode; only the local mouse actually enters 3D.
    tev |= 16; ach_get(7);
    chase_eye_start(px, py, (u8)(angle >> 8), vi);
  } else chase_map_start(hx, hy);
  prevh = lk_k[hunter]; prevr = lk_k[runner];
  for (frame = 0; frame < CHASE_TICKS && !lk_err; frame++) {
    u16 hp, rp;
    u8 secs = (u8)((CHASE_TICKS - frame + 59) / 60);
    u8 moved = 0;
    vsync(); frc++;
    // Keep a single shared world checksum despite different local cameras.
    mp_hash();
    lk_hash ^= ((u32)rx << 24) ^ ((u32)ry << 16) ^ ((u32)hx << 8) ^
               (u32)hy ^ ((u32)deliveries << 12) ^ (u32)scan_cd;
    (void)joy();
    if (lk_err) { roundout = 2; break; }
    k[0] = lk_k[hunter]; k[1] = lk_k[runner];
    hp = (u16)(k[0] & ~prevh); rp = (u16)(k[1] & ~prevr);
    prevh = k[0]; prevr = k[1];
    if ((hp | rp) & J_START) { roundout = 2; break; }
    if (scan) scan--;
    if (scan_cd) scan_cd--;
    if (tag_cd) tag_cd--;
    if ((hp & J_B) && !scan_cd) {
      scan = 110; scan_cd = 420; sfx_mana();
    }
    // Cursor covers one map cell per eight frames; accurate tagging takes
    // deliberate pursuit rather than near-instant map-wide teleporting.
    if (!(frame & 7)) {
      if ((k[0] & J_LEFT) && hx) hx--;
      if ((k[0] & J_RIGHT) && hx + 1 < W) hx++;
      if ((k[0] & J_UP) && hy) hy--;
      if ((k[0] & J_DOWN) && hy + 1 < H) hy++;
    }
    // Mouse: normal ANT EYE movement + stamina-based sprint.
    if (k[1] & J_RIGHT) angle += 410;
    if (k[1] & J_LEFT) angle -= 410;
    fa = (u8)(angle >> 8);
    {
      s32 cs = SINT[(fa + 64) & 255], sn = SINT[fa];
      s32 vx = 0, vy = 0, speed = ESPD[O(O_EYESPD)];
      if ((k[1] & J_A) && (k[1] & (J_UP | J_DOWN | J_L | J_R)) && energy) {
        speed = speed * 2; energy--;
      }
      else if (!(frame & 3) && energy < 99) energy++;
      if (k[1] & J_UP) { vx += (cs * speed) >> 8; vy += (sn * speed) >> 8; }
      if (k[1] & J_DOWN) { vx -= (cs * speed) >> 9; vy -= (sn * speed) >> 9; }
      if (k[1] & J_L) { vx += (sn * speed * 3) >> 10; vy -= (cs * speed * 3) >> 10; }
      if (k[1] & J_R) { vx -= (sn * speed * 3) >> 10; vy += (cs * speed * 3) >> 10; }
      if ((k[1] & (J_UP | J_DOWN)) && (k[1] & (J_L | J_R))) {
        vx = vx * 3 / 4; vy = vy * 3 / 4;
      }
      if (vx || vy) moved = eye_walk(&px, &py, vx, vy);
    }
    rx = (u8)(px >> 8); ry = (u8)(py >> 8);
    if (moved) {
      Ant *a = &ant[vi];
      a->ox = a->x; a->oy = a->y; a->x = rx; a->y = ry;
      a->age = 8;
      if (a->x != a->ox) a->fl = a->x < a->ox;
    }
    if ((k[1] & J_B) && !(frame & 3)) ph[ry][rx] = 255;
    if (!ant[vi].carry && food[ry][rx]) {
      food[ry][rx] = 0; ant[vi].carry = 1; sfx_food();
      if (!is_eye) draw_dec(rx, ry);  // VRAM-only update on map machine
    }
    if (ant[vi].carry && dist(rx, nestx[0]) <= 1 && dist(ry, nesty[0]) <= 1) {
      ant[vi].carry = 0; deliveries++; chase_last_food = deliveries; st_food++;
      sfx_mana();
      if (deliveries >= CHASE_GOAL) { roundout = 1; break; }
    }
    // The 2D cat cannot tag before the mouse gets a short head start.
    if ((hp & J_A) && !tag_cd && frame > 180) {
      tag_cd = 24;
      if (dist(hx, rx) + dist(hy, ry) <= 1) {
        roundout = 0; sfx_hit(); break;
      } else sfx_deny();
    }
    if (!(frame & 7)) { tick_slice(); bfs_tick(200); }
    // Advance presentation timers in the SHARED simulation, never from the
    // map-only drawing branch. Otherwise the 2D and 3D ROMs desynchronize.
    if (!(frame & 3)) {
      for (i = 0; i < 8; i++) if (fx[i].t) {
        fx[i].t--;
        if (fx[i].tile != FX_PUFF && (fx[i].t & 1)) fx[i].y--;
      }
      for (i = 0; i < 2; i++) if (qfl[i]) qfl[i]--;
    }
    if (!vok(vi) || !qhp[0]) { roundout = 0; break; }
    // The PRESENTATION alone diverges. No world-tick / FX mutations from
    // drawing; this prevents 2D/3D client disagreement and cable desync.
    if (is_eye) {
      if ((s16)rx - fox < 16 || (s16)rx - fox > 47 ||
          (s16)ry - foy < 16 || (s16)ry - foy > 47) {
        fox = (s16)rx - 32; foy = (s16)ry - 32;
        floor_map((volatile u16 *)0x0600B000);
      }
      cur = eye_tab == m7t[0] ? 1 : 0;
      m7_build(m7t[cur], fa, (s16)px, (s16)py);
      eye_tab = m7t[cur];
      eye_scene(fa, px, py, vi, 0, (u8)((frc >> 3) & 1));
      BG0HOFS = (u16)(fa * 5 + (fa >> 2));
      if (!(frame & 7)) {
        eye_compass(fa, rx, ry);
        chase_hud_eye(runner, deliveries, energy, secs, ant[vi].carry);
      }
    } else {
      cx = hx; cy = hy;
      follow(); scroll_step(); win_sync();
      BG0HOFS = BG2HOFS = (u16)scx;
      BG0VOFS = BG2VOFS = (u16)scy;
      chase_map_sprites(hx, hy, rx, ry, scan || ant[vi].carry);
      oam_flush();
      if (!(frame & 7)) chase_hud_map(hunter, deliveries, scan_cd, secs);
    }
  }
  if (is_eye) chase_eye_stop(sky, pic);
  eye_ctl = eye_ctl2 = MAXA;
  if (lk_err) return 2;
  return roundout;
}
static void chase_result(u8 round, u8 outcome, const u8 *score) {
  u16 k;
  u8 winner = outcome ? (u8)(round ^ 1) : round;
  hide_all(); vsync(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full();
  big(1, "CHASE RESULTS");
  ps(3, 5, outcome ? "MOUSE ESCAPED WITH FOOD!" : "CAT CAUGHT THE MOUSE!", outcome ? 13 : 8);
  ps(3, 8, "ROUND WINNER:", 10);
  ps(18, 8, winner ? "PLAYER 2" : "PLAYER 1", 9);
  ps(3, 11, "P1 SCORE", 13); pn(20, 11, score[0], 13);
  ps(3, 13, "P2 SCORE", 8); pn(20, 13, score[1], 8);
  ps(3, 15, "ROUND WIN +3  EACH FOOD +1", 10);
  ps(2, 17, round ? "MATCH DONE!" : "NEXT: SWAP CAT AND MOUSE", 3);
  ps(2, 19, "A / START: CONTINUE", 9);
  DISPCNT = 0x1B40; BLDY = 0;
  while (!lk_err) {
    vsync(); mp_hash(); k = joy();
    if (k & (J_A | J_START)) break;
  }
  while (!lk_err) {
    vsync(); mp_hash(); k = joy();
    if (!(k & (J_A | J_START))) break;
  }
}
static void chase_final(const u8 *score) {
  u16 k;
  hide_all(); vsync(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full(); big(1, "CAT & MOUSE");
  ps(3, 5, score[0] > score[1] ? "PLAYER 1 WINS!" :
           score[1] > score[0] ? "PLAYER 2 WINS!" : "DRAW - EVEN SCORES!", 9);
  ps(3, 8, "PLAYER 1", 13); pn(19, 8, score[0], 13);
  ps(3, 10, "PLAYER 2", 8); pn(19, 10, score[1], 8);
  ps(3, 13, "TWO ROUNDS COMPLETE.", 3);
  ps(3, 15, "NEXT: NEW HUNT OR GAME", 3);
  ps(3, 19, "A / START: BACK TO MENU", 9);
  DISPCNT = 0x1B40; BLDY = 0;
  while (!lk_err) {
    vsync(); mp_hash(); k = joy();
    if (k & (J_A | J_START)) break;
  }
  while (!lk_err) {
    vsync(); mp_hash(); k = joy();
    if (!(k & (J_A | J_START))) break;
  }
}
static void chase_play(void) {
  u8 round, result, score[2] = {0, 0};
  for (round = 0; round < 2 && !lk_err; round++) {
    // Rebuild the arena each round so both runners have fair food supplies,
    // rather than inheriting the other runner's picked-up items.
    newgame();
    chase_rules(round);
    if (lk_err) break;
    result = chase_round(round);
    if (result == 2) { over = 3; break; }
    // Reward actual foraging effort plus the round victory, so two matches
    // where both cats win need not end in an automatic, unrewarding draw.
    score[round ^ 1] += chase_last_food;
    score[result ? (round ^ 1) : round] += 3;
    chase_result(round, result, score);
    if (lk_err) break;
    if (round == 0) { vram_reload(); opts_apply(); }
  }
  if (!lk_err && over != 3) chase_final(score);
  over = 3;  // leave safely through mp_run's usual link/option cleanup
}
