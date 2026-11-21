// ============================================================================
// DUAL VIEW EXPEDITION: linked asymmetric cooperative multiplayer.
// A map GUIDE and a first-person ant SCOUT share one colony simulation.
// Two 90-second expeditions swap viewpoints, and both players contribute to
// one team score. Linked inputs are always applied in the same order.
// ============================================================================
#include "chase_logic.h"
#define CHASE_TICKS 5400u                // 90 seconds at the game's 60 Hz VBlank
#define CHASE_GOAL 3
#define CHASE_BERRIES 8             // reserve a small accessible food supply
#define CHASE_FOOD_TICK 127u        // top up every 128 linked frames (~2 seconds)
// One 4bpp tile, outside the MAPT 832..975 generated-map atlas.
// Pixel index 1 in palette 1 is black. No image asset/framebuffer needed.
#define CHASE_PANEL_TILE 976
static u8 chase_last_food;   // the scout's completed deliveries for team scoring
// Both peers generate identical supplies from map data, never RNG. Workers
// still consume food normally, but cannot permanently starve the scout's
// three-delivery win condition. Keep at least eight nearby reachable berries.
// The guide's BG map must refresh newly grown berries on its own cartridge;
// the first-person client has BG2 in first-person mode and must NOT write the map.
static void chase_food(u8 map_visible) {
  s16 x, y;
  u8 supply = 0, phase;
  for (y = (s16)nesty[0] - 8; y <= (s16)nesty[0] + 8; y++)
    for (x = (s16)nestx[0] - 8; x <= (s16)nestx[0] + 8; x++) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      if (dnp[0][y][x] == NOPATH || dnp[0][y][x] > 15) continue;
      if (food[y][x] && supply < CHASE_BERRIES) supply++;
    }
  if (supply >= CHASE_BERRIES) return;
  // Sweep all eight stable hash buckets, covering all eligible squares even
  // when a sparse arena has fewer than eight squares in bucket zero.
  for (phase = 0; phase < 8 && supply < CHASE_BERRIES; phase++)
    for (y = (s16)nesty[0] - 8; y <= (s16)nesty[0] + 8 && supply < CHASE_BERRIES; y++)
      for (x = (s16)nestx[0] - 8; x <= (s16)nestx[0] + 8 && supply < CHASE_BERRIES; x++) {
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        if (!hgt[y][x] || near_nest((u8)x, (u8)y) || food[y][x]) continue;
        if (dnp[0][y][x] == NOPATH || dnp[0][y][x] > 15) continue;
        if (chase_berry_bucket((u8)x, (u8)y) != phase) continue;
        food[y][x] = 1;
        supply++;
        if (map_visible) draw_dec((u8)x, (u8)y);
      }
}
// Cheap, bounded objective search: the hunt guarantees reachable food around
// the black nest. Recheck at HUD cadence, not per camera frame. The compass
// then guides the scout even when the food is behind the camera.
static u8 chase_goal(u8 rx, u8 ry, u8 *gx, u8 *gy) {
  s16 x, y;
  u16 best = 0xFFFFu, d;
  u8 found = 0;
  for (y = (s16)nesty[0] - 8; y <= (s16)nesty[0] + 8; y++)
    for (x = (s16)nestx[0] - 8; x <= (s16)nestx[0] + 8; x++) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      if (!food[y][x] || dnp[0][y][x] == NOPATH || dnp[0][y][x] > 15) continue;
      d = (u16)(dist(rx, (u8)x) + dist(ry, (u8)y));
      if (d < best) { best = d; *gx = (u8)x; *gy = (u8)y; found = 1; }
    }
  return found;
}
static void chase_rules(u8 round) {
  u16 k;
  u8 guide = round, scout = (u8)(round ^ 1), ready = 0;
  hide_all(); vsync(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "DUAL VIEW");
  ps(2, 3, "TWO RUNS - SWAP VIEWS", 9);
  ps(2, 5, "GUIDE: OVERHEAD ANT MAP", 8);
  ps(2, 6, "D-PAD MOVE A MARK B FIND", 3);
  ps(2, 8, "SCOUT: 3D ANT EYE", 13);
  ps(2, 9, "FIND % FOOD - BRING TO $", 3);
  ps(2, 10, "TEAM: DELIVER THREE LOADS", 13);
  ps(2, 11, "A SPRINT B SCENT L/R STRAFE", 3);
  ps(2, 12, "FOLLOW ? GUIDE WAYPOINTS", 13);
  ps(2, 13, "REACH MARK: SPRINT BOOST", 8);
  ps(2, 14, "TIME LIMIT: 90 SECONDS", 8);
  ps(2, 16, "EXPEDITION", 10); pn(14, 16, (u8)(round + 1), 9);
  ps(2, 17, lk_me == guide ? "YOUR JOB: GUIDE FROM MAP" : "YOUR JOB: EXPLORE AS AN ANT", lk_me == guide ? 8 : 13);
  ps(2, 18, "P1 WAIT        P2 WAIT", 10);
  ps(2, 19, "BOTH PRESS A / START TO READY", 9);
  DISPCNT = 0x1B40; BLDY = 0;
  while (ready != 3 && !lk_err) {
    vsync(); mp_hash(); (void)joy();
    if ((lk_k[0] & (J_A | J_START)) && !(ready & 1)) {
      ready |= 1; ps(2, 18, "P1 READY", 13); sfx_food();
    }
    if ((lk_k[1] & (J_A | J_START)) && !(ready & 2)) {
      ready |= 2; ps(17, 18, "P2 READY", 13); sfx_food();
    }
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
static void chase_map_sprites(u8 hx, u8 hy, s32 px, s32 py,
                              u8 marked, u8 mx, u8 my) {
  u8 i, tl;
  Ant *a;
  for (i = 0; i < MAXA; i++) {
    a = &ant[i];
    if (!a->alive) { spr_hide(i); continue; }
    tl = a->carry ? 5 : a->sol ? 7 : 3;
    if (a->age < 8 && (((frc >> 2) + i) & 1)) tl++;
    // The scout is the very same ant seen from the world map. Preserve the
    // fractional ANT EYE coordinate for smooth movement, not tile snapping.
    if (i == eye_ctl)
      place_px(i, tl, a->team, chase_view_px(px), chase_view_px(py), a->fl);
    else
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
  if (marked) place16(MAXA + 11, 20, 8, mx * 8, my * 8, 0);
  else spr_hide(MAXA + 11);
}
static void chase_hud_map(u8 guide, u8 delivered, u8 marked, u8 secs,
                          u8 can_mark) {
  ov_fill(CHASE_PANEL_TILE, 1, 17, 19);
  ps(0, 17, guide ? "P2 GUIDE: COLONY MAP" : "P1 GUIDE: COLONY MAP", 8);
  ps(24, 17, "TIME", 10); pn(28, 17, secs, 9);
  ps(0, 18, "FOOD DELIVERED", 13); pn(16, 18, delivered, 9);
  ps(18, 18, "/3", 3);
  ps(22, 18, marked ? "MARK ON" : "NO MARK", marked ? 13 : 10);
  // Make the action available at this cursor explicit. B always recenters.
  ps(0, 19, can_mark ? "D-PAD MAP A MARK B FIND SCOUT" :
     "A: NEEDS LAND 3 FROM SCOUT", 10);
}
static void chase_hud_eye(u8 scout, u8 delivered, u8 energy, u8 secs,
                          u8 carry, u8 marked) {
  ov_fill(CHASE_PANEL_TILE, 1, 16, 19);
  ps(0, 16, scout ? "P2 SCOUT: ANT EYE" : "P1 SCOUT: ANT EYE", 13);
  ps(24, 16, "TIME", 10); pn(28, 16, secs, 9);
  ps(0, 17, carry ? "FOOD HELD: RETURN HOME $" :
     marked ? "FOLLOW GUIDE ? COMPASS MARK" :
     "FIND FOOD % AND BRING HOME $", carry ? 13 : marked ? 14 : 3);
  ps(0, 18, "DELIVERIES", 10); pn(12, 18, delivered, 13);
  ps(14, 18, "/3", 3); ps(19, 18, "STAMINA", 10); pn(28, 18, energy, 9);
  ps(0, 19, "\1 SPRINT \2 SCENT L/R STRAFE", 10);
}
// Returns 1 when the team delivered 3 loads, 0 at time-out/loss, 2 on abort.
static u8 chase_round(u8 round) {
  u8 guide = round, scout = (u8)(round ^ 1), vi, rx, ry, hx, hy;
  u8 deliveries = 0, energy = 99, marked = 0, mx = 0, my = 0;
  u8 prevh = 0, prevr = 0, roundout = 0, cur, fa, is_eye;
  u16 sky = BGPAL[0], pic[16], angle = 0, k[2], frame;
  s32 px, py;
  u8 i;
  chase_last_food = 0;
  // Round setup is deterministic and the generated world is identical on
  // both GBAs, independently of which perspective either GBA renders.
  chase_food(0); // win_all() in chase_map_start() draws the initial berries
  vi = mp_pilot_near(nestx[0], nesty[0], MAXA);
  if (vi >= MAXA) return 0;  // no workers left to control
  eye_ctl = vi; eye_ctl2 = MAXA;
  px = (s32)ant[vi].x * 256 + 128;
  py = (s32)ant[vi].y * 256 + 128;
  rx = ant[vi].x; ry = ant[vi].y;
  angle = (u16)((ant[vi].fl ? 128u : 0u) << 8);
  hx = nestx[1]; hy = nesty[1];
  is_eye = (u8)(lk_me == scout);
  for (i = 0; i < 16; i++) pic[i] = BGPAL[112 + i];
  if (is_eye) {
    // First-person entry is recorded in the same save-RAM milestone used by
    // the tested two-pilot mode; only the local scout actually enters 3D.
    tev |= 16; ach_get(7);
    chase_eye_start(px, py, (u8)(angle >> 8), vi);
  } else chase_map_start(hx, hy);
  prevh = lk_k[guide]; prevr = lk_k[scout];
  for (frame = 0; frame < CHASE_TICKS && !lk_err; frame++) {
    u16 hp, rp;
    u8 secs = (u8)((CHASE_TICKS - frame + 59) / 60);
    u8 moved = 0;
    vsync(); frc++;
    // Keep a single shared world checksum despite different local cameras.
    mp_hash();
    lk_hash ^= ((u32)rx << 24) ^ ((u32)ry << 16) ^ ((u32)hx << 8) ^
               (u32)hy ^ ((u32)deliveries << 12) ^
               ((u32)mx << 4) ^ ((u32)my << 20) ^ ((u32)marked << 30);
    (void)joy();
    if (lk_err) { roundout = 2; break; }
    k[0] = lk_k[guide]; k[1] = lk_k[scout];
    hp = (u16)(k[0] & ~prevh); rp = (u16)(k[1] & ~prevr);
    prevh = k[0]; prevr = k[1];
    if ((hp | rp) & J_START) { roundout = 2; break; }
    // The GUIDE marks a reachable destination; B refocuses the overhead map
    // on the real scout position. This is guidance, not targeting.
    if (hp & J_B) { hx = rx; hy = ry; sfx_food(); }
    if (hp & J_A) {
      if (chase_marker_legal(hgt[hy][hx], dnp[0][hy][hx],
                             (u16)(dist(hx, rx) + dist(hy, ry)))) {
        mx = hx; my = hy; marked = 1; sfx_mana();
      } else sfx_deny();
    }
    // Cursor covers one map cell per eight frames; accurate tagging takes
    // deliberate pursuit rather than near-instant map-wide teleporting.
    if (!(frame & 7)) {
      if ((k[0] & J_LEFT) && hx) hx--;
      if ((k[0] & J_RIGHT) && hx + 1 < W) hx++;
      if ((k[0] & J_UP) && hy) hy--;
      if ((k[0] & J_DOWN) && hy + 1 < H) hy++;
    }
    // Scout: normal ANT EYE movement + stamina-based sprint.
    if (k[1] & J_RIGHT) angle += 410;
    if (k[1] & J_LEFT) angle -= 410;
    fa = (u8)(angle >> 8);
    {
      s32 cs = SINT[(fa + 64) & 255], sn = SINT[fa];
      s32 vx = 0, vy = 0, speed = ESPD[O(O_EYESPD)];
      if ((k[1] & J_A) && (k[1] & (J_UP | J_DOWN | J_L | J_R)) && energy) {
        // A useful expedition-length sprint (~3.3 s from full stamina), rather
        // than draining the entire meter in only 99 video frames.
        speed = speed * 2; if (!(frame & 1)) energy--;
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
    // Pheromone trails and movements both affect the same ant colony data.
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
    // A reached guide waypoint restores some sprint energy and disappears.
    // Rejecting markers placed next to the scout prevents free energy spam.
    if (marked && dist(rx, mx) + dist(ry, my) <= 1) {
      marked = 0;
      energy = (u8)(energy > 79 ? 99 : energy + 20);
      sfx_food();
    }
    if (!(frame & 7)) { tick_slice(); bfs_tick(200); }
    // Replenish before drawing either perspective. Only the map client
    // touches map VRAM, while both peers change identical world-state food.
    if (!(frame & CHASE_FOOD_TICK)) chase_food((u8)!is_eye);
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
    // Local cameras differ, but linked game state and guide markers match.
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
      // ANT EYE world objects use shadow OAM, so upload their changes.
      // The previous asymmetric loop never flushed OAM after eye_scene, leaving
      // living ants and other props visually frozen in the scout's view.
      // Scene/OAM at 30Hz with 60Hz camera matrix and lockstep world ticks.
      if (!(frame & 1)) {
        eye_scene(fa, px, py, vi, 0, (u8)((frc >> 3) & 1));
        oam_flush();
      }
      BG0HOFS = (u16)(fa * 5 + (fa >> 2));
      if (!(frame & 7)) {
        u8 gx, gy;
        eye_compass(fa, rx, ry);
        if (!ant[vi].carry && chase_goal(rx, ry, &gx, &gy))
          eye_mark(fa, (s16)gx - rx, (s16)gy - ry, '%', 13);
        if (marked)
          eye_mark(fa, (s16)mx - rx, (s16)my - ry, '?', 14);
        chase_hud_eye(scout, deliveries, energy, secs, ant[vi].carry,
                       marked);
      }
    } else {
      cx = hx; cy = hy;
      follow(); scroll_step(); win_sync();
      BG0HOFS = BG2HOFS = (u16)scx;
      BG0VOFS = BG2VOFS = (u16)scy;
      chase_map_sprites(hx, hy, px, py, marked, mx, my);
      oam_flush();
      if (!(frame & 7))
        chase_hud_map(guide, deliveries, marked, secs,
          chase_marker_legal(hgt[hy][hx], dnp[0][hy][hx],
            (u16)(dist(hx, rx) + dist(hy, ry))));
    }
  }
  if (is_eye) chase_eye_stop(sky, pic);
  eye_ctl = eye_ctl2 = MAXA;
  if (lk_err) return 2;
  return roundout;
}
static void chase_result(u8 round, u8 outcome, const u8 *score) {
  u16 k;
  hide_all(); vsync(); oam_flush(); DISPCNT = 0x80;
  ov_fill(1, 3, 0, 19); ui_full();
  big(1, "DUAL VIEW");
  ps(3, 5, outcome ? "SUPPLY RUN COMPLETE!" : "RUN ENDED: TIME OR DANGER", outcome ? 13 : 8);
  ps(3, 8, "TEAM SUPPLY REPORT", 10);
  ps(3, 11, "RUN 1 SCORE", 13); pn(20, 11, score[0], 13);
  ps(3, 13, "RUN 2 SCORE", 8); pn(20, 13, score[1], 8);
  ps(3, 15, "DELIVERY +1  COMPLETION +3", 10);
  ps(2, 17, round ? "BOTH VIEWS COMPLETED" : "NEXT: SWAP VIEWPOINTS", 3);
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
  ov_fill(1, 3, 0, 19); ui_full(); big(1, "DUAL VIEW");
  ps(3, 5, "TWO VIEWPOINTS - ONE COLONY", 9);
  ps(3, 8, "EXPEDITION 1", 13); pn(20, 8, score[0], 13);
  ps(3, 10, "EXPEDITION 2", 8); pn(20, 10, score[1], 8);
  ps(3, 13, "TOTAL TEAM SCORE", 3);
  pn(20, 13, (u8)(score[0] + score[1]), 9);
  ps(3, 15, "SWAP AND PLAY AGAIN!", 3);
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
    // Rebuild the world each run so each scout gets a fair food supply.
    newgame();
    chase_rules(round);
    if (lk_err) break;
    result = chase_round(round);
    if (result == 2) { over = 3; break; }
    // Team scoring rewards food gathered in either view plus completed runs.
    score[round] = chase_team_points(chase_last_food, result);
    chase_result(round, result, score);
    if (lk_err) break;
    if (round == 0) { vram_reload(); opts_apply(); }
  }
  if (!lk_err && over != 3) chase_final(score);
  over = 3;  // leave safely through mp_run's usual link/option cleanup
}
