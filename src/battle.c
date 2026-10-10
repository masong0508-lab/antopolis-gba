// ===================== EMPIRE MODE: PIXEL BATTLES (included from main.c) =====================
// When two armies meet, the game switches to video mode 4 (a 240x160 bitmap, two pages) and every ant is drawn as 2 pixels
// (a major as 2x2). Up to 500 a side. Afterwards all tile graphics are reloaded (the bitmap covers that video memory).
#define BMAX 1000
#define BT_W 0                                     // warrior: fast, melee
#define BT_S 1                                     // spitter: ranged acid
#define BT_M 2                                     // major: big, slow, hits hard
typedef struct { s16 x, y; u16 tg; u8 hp, side, type, cd; } BAnt;
EWRAM static BAnt ba[BMAX];
EWRAM static u16 bal[2][BMAX / 2];                     // alive lists per side
#define bbg (gen_scratch.b)                          // the battlefield picture (shares memory with the map generator)
static u16 bn[2], bcount;
typedef struct { s16 x, y, tx, ty; u8 t; } BShot;
static BShot bsh[24]; static u8 bshn;
// palette indices used by the battle picture (BG palette, 8bpp)
#define BC_G0 1   // ground shades 1..4
#define BC_ME 5   // your ants: 5 body, 6 head
#define BC_EN 7   // enemy ants: 7 body, 8 head
#define BC_DEAD 9
#define BC_ACID 10
#define BC_TXT 11
#define BC_SH 12
#define BC_TOWER 13
#define BC_GOLD 14
static void bpx8(u8 *buf, int x, int y, u8 c) { if ((unsigned)x < 240 && (unsigned)y < 160) buf[y * 240 + x] = c; }
IWRAM static void vplot(volatile u16 *pg, int x, int y, u8 c) {   // one 8bpp pixel in VRAM (16-bit read-modify-write)
  volatile u16 *p; u16 v;
  if ((unsigned)x >= 240 || (unsigned)y >= 160) return;
  p = pg + ((y * 240 + x) >> 1); v = *p;
  *p = (x & 1) ? (u16)((v & 0x00FF) | (c << 8)) : (u16)((v & 0xFF00) | c);
}
static void bbtext(u8 *buf, int x, int y, const char *s, u8 c) {   // font8 text into the picture, with a shadow
  u8 gx, gy, g;
  for (; *s; s++, x += 8) {
    g = gi(*s);
    for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) { bpx8(buf, x + gx + 1, y + gy + 1, BC_SH); bpx8(buf, x + gx, y + gy, c); }
  }
}
static void bnum(char *b, u16 v) { u8 i = 0, j; char t[6]; do { t[i++] = (char)('0' + v % 10); v /= 10; } while (v && i < 5); for (j = 0; j < i; j++) b[j] = t[i - 1 - j]; b[i] = 0; }
static void bbground(u8 terr, u8 towers) {          // grass / sand / hill / rock look, with tufts and pebbles; towers on your side
  static const u16 GC[4][4] = {{RGB(5,10,20), RGB(6,12,23), RGB(9,15,25), RGB(4,8,16)}, {RGB(26,22,13), RGB(23,19,10), RGB(28,25,16), RGB(20,16,8)},
                               {RGB(8,19,6), RGB(6,15,5), RGB(11,23,8), RGB(5,12,4)}, {RGB(18,13,8), RGB(15,10,6), RGB(22,17,11), RGB(12,8,5)}};
  u16 i; int x, y; u32 h = 12345u;
  for (i = 0; i < 4; i++) BGPAL[BC_G0 + i] = GC[terr & 3][i];
  for (i = 0; i < 240 * 160; i++) { h = h * 1103515245u + 12345u; bbg[i] = (u8)(BC_G0 + ((h >> 16) % 23 == 0 ? 2 : (h >> 16) % 31 == 0 ? 3 : (h >> 20) & 1)); }
  for (i = 0; i < towers && i < 4; i++) {          // little watchtowers on the left edge
    x = 4; y = 30 + i * 30;
    for (int yy = 0; yy < 14; yy++) for (int xx = 0; xx < 6; xx++) bpx8(bbg, x + xx, y + yy, (yy < 3 && (xx & 1)) ? 0 : BC_TOWER);
  }
}
static void bspawn(u8 side, u16 nw, u16 ns, u16 nm) {   // lines up an army: majors in front, warriors, spitters behind
  u16 k, n = 0, tot = nw + ns + nm, rows; u8 t; s16 x0, dir = side ? -1 : 1; u32 r;
  rows = (u16)((tot + 39) / 40); if (!rows) rows = 1;
  for (k = 0; k < tot && bcount < BMAX; k++) {
    BAnt *a = &ba[bcount];
    t = k < nm ? BT_M : k < nm + nw ? BT_W : BT_S;
    r = rnd();
    x0 = side ? 168 : 72;                          // each army in ranks, the back ranks further from the middle
    a->x = (s16)((x0 + dir * -1 * (s16)((k / 40) * 4) + (s16)(r & 3) - 1) << 4);
    a->y = (s16)((22 + (k % 40) * 3 + (s16)((r >> 4) & 3)) << 4);
    a->side = side; a->type = t; a->tg = 0xFFFF; a->cd = (u8)(r >> 8 & 15);
    a->hp = t == BT_M ? (u8)(9 + eshell * (side == 0) * 3) : t == BT_S ? 2 : (u8)(3 + eshell * (side == 0));
    bal[side][bn[side]++] = bcount++; n++;
  }
  (void)n;
}
IWRAM static void bstep(u8 par) {                  // one simulation step; par = which half of the ants moves this frame
  u16 s, j, n, i, t; BAnt *a, *b; s16 dx, dy, adx, ady, rng, sp; u8 dmg;
  for (s = 0; s < 2; s++) {
    n = bn[s]; if (!bn[s ^ 1]) return;
    for (j = (u16)(par & 1); j < n; j += 2) {
      i = bal[s][j]; a = &ba[i]; if (!a->hp) continue;
      t = a->tg;
      if (t == 0xFFFF || !ba[t].hp) { t = bal[s ^ 1][rnd() % bn[s ^ 1]]; a->tg = t; }
      b = &ba[t];
      dx = (s16)(b->x - a->x); dy = (s16)(b->y - a->y); adx = dx < 0 ? -dx : dx; ady = dy < 0 ? -dy : dy;
      rng = a->type == BT_S ? (28 << 4) : (3 << 4);
      if (adx + ady > rng) {
        sp = a->type == BT_W ? 22 : a->type == BT_S ? 18 : 12;
        if (adx > ady * 2) { a->x += dx > 0 ? sp : -sp; a->y += (s16)((rnd() & 7) - 3); }
        else if (ady > adx * 2) { a->y += dy > 0 ? sp : -sp; a->x += (s16)((rnd() & 7) - 3); }
        else { a->x += dx > 0 ? sp * 3 / 4 : -sp * 3 / 4; a->y += dy > 0 ? sp * 3 / 4 : -sp * 3 / 4; }
      } else if (!a->cd) {
        dmg = a->type == BT_M ? (u8)(3 + earm * (s == 0)) : (u8)(1 + earm * (s == 0) * (a->type == BT_W));
        if (b->hp <= dmg) { b->hp = 0; bpx8(bbg, b->x >> 4, b->y >> 4, BC_DEAD); bpx8(bbg, (b->x >> 4) + 1, b->y >> 4, BC_DEAD); }
        else b->hp -= dmg;
        a->cd = a->type == BT_M ? 26 : a->type == BT_S ? 34 : 16;
        if (a->type == BT_S && bshn < 24) { BShot *q = &bsh[bshn++]; q->x = a->x; q->y = a->y; q->tx = b->x; q->ty = b->y; q->t = 8; }
      }
      if (a->cd) a->cd--;
      if (a->y < (16 << 4)) a->y = 16 << 4;
      if (a->y > (158 << 4)) a->y = 158 << 4;
      if (a->x < (2 << 4)) a->x = 2 << 4;
      if (a->x > (237 << 4)) a->x = 237 << 4;
    }
  }
}
static void bcompact(void) { u8 s; u16 j, k; for (s = 0; s < 2; s++) { for (j = k = 0; j < bn[s]; j++) if (ba[bal[s][j]].hp) bal[s][k++] = bal[s][j]; bn[s] = k; } }
IWRAM static void bdraw(volatile u16 *pg) {
  u16 i; BAnt *a; int x, y; u8 c0, c1;
  for (i = 0; i < bcount; i++) {
    a = &ba[i]; if (!a->hp) continue;
    x = a->x >> 4; y = a->y >> 4;
    c0 = a->side ? BC_EN : BC_ME; c1 = (u8)(c0 + 1);
    if (a->type == BT_M) { vplot(pg, x, y, c0); vplot(pg, x + 1, y, c0); vplot(pg, x, y + 1, c0); vplot(pg, x + 1, y + 1, c0); vplot(pg, x + (a->side ? -1 : 2), y, c1); }
    else { vplot(pg, x, y, c0); vplot(pg, x + (a->side ? -1 : 1), y, a->type == BT_S ? BC_ACID : c1); }
  }
  for (i = 0; i < bshn; i++) {                     // acid shots fly
    BShot *q = &bsh[i];
    x = (q->tx + (q->x - q->tx) * q->t / 8) >> 4; y = (q->ty + (q->y - q->ty) * q->t / 8) >> 4;
    vplot(pg, x, y, BC_ACID);
  }
}
static u16 bside_count(u8 s, u8 type) { u16 j, n = 0; for (j = 0; j < bn[s]; j++) if (ba[bal[s][j]].type == type) n++; return n; }
static void bhud(u8 *dst) {                        // top line: YOU 312   287 RED
  char b[8]; u16 k;
  for (k = 0; k < 240 * 12; k++) dst[k] = BC_SH;
  bbtext(dst, 2, 2, "YOU", BC_TXT); bnum(b, bn[0]); bbtext(dst, 34, 2, b, BC_GOLD);
  bnum(b, bn[1]); bbtext(dst, 150, 2, b, BC_GOLD); bbtext(dst, 190, 2, ebat_name, BC_TXT);
  bbtext(dst, 98, 2, "VS", BC_TXT);
}
// Runs a battle. In: both armies (warriors, spitters, majors), the terrain, your towers nearby, whether to watch.
// Out: survivors written back. Returns 1 if you won.
static u8 battle(u16 *aw, u16 *as, u16 *am, u16 *ew, u16 *es, u16 *em, u8 terr, u8 towers, u8 watch) {
  u16 fr = 0, k, sv, tw; u8 won, par = 0; volatile u16 *pg;
  static u16 palsave[256];
  bcount = 0; bn[0] = bn[1] = 0; bshn = 0;
  if (*aw + *as + *am > 500) { u16 t = *aw + *as + *am; *aw = (u16)(*aw * 500u / t); *as = (u16)(*as * 500u / t); *am = (u16)(*am * 500u / t); }
  if (*ew + *es + *em > 500) { u16 t = *ew + *es + *em; *ew = (u16)(*ew * 500u / t); *es = (u16)(*es * 500u / t); *em = (u16)(*em * 500u / t); }
  bspawn(0, *aw, *as, *am); bspawn(1, *ew, *es, *em);
  if (!watch) {                                    // quick result: the same simulation, not drawn
    while (bn[0] && bn[1] && fr < 3600) { bstep(0); bstep(1); fr++; if (!(fr & 7)) bcompact(); if (towers && !(fr % 40) && bn[1]) { u16 t = bal[1][rnd() % bn[1]]; ba[t].hp = 0; } for (k = 0; k < bshn; k++) if (bsh[k].t) bsh[k].t--; bshn = 0; }
    bcompact();
  } else {
    music_stop();
    for (k = 0; k < 256; k++) palsave[k] = BGPAL[k];
    fade(0, 16);
    hide_all(); oam_flush();
    bbground(terr, towers);
    BGPAL[0] = 0; BGPAL[BC_ME] = OBJPAL[2]; BGPAL[BC_ME + 1] = OBJPAL[3];
    BGPAL[BC_EN] = OBJPAL[ebat_pal * 16 + 2]; BGPAL[BC_EN + 1] = OBJPAL[ebat_pal * 16 + 3];
    BGPAL[BC_DEAD] = RGB(6, 3, 2); BGPAL[BC_ACID] = RGB(14, 31, 6); BGPAL[BC_TXT] = RGB(31, 31, 31); BGPAL[BC_SH] = RGB(2, 1, 0);
    BGPAL[BC_TOWER] = RGB(17, 17, 19); BGPAL[BC_GOLD] = RGB(31, 26, 6);
    DISPCNT = 0x0404 | 0x80;
    bhud(bbg);
    bbtext(bbg, 88, 70, "BATTLE!", BC_GOLD);
    for (k = 0; k < 2; k++) { pg = (volatile u16 *)(0x06000000 + k * 0xA000); DMAR(3, 0) = (u32)bbg; DMAR(3, 4) = (u32)pg; DMAR(3, 8) = DMA_ON | DMA_W32 | (240 * 160 / 4); }
    DISPCNT = 0x0404; fade(16, 0);
    for (k = 0; k < 45; k++) vsync();
    bbground(terr, towers); bhud(bbg);             // a clean field again (without the BATTLE! sign)
    while (bn[0] && bn[1] && fr < 3600) {
      pg = (volatile u16 *)(0x06000000 + ((fr & 1) ? 0 : 0xA000));   // draw the hidden page
      bstep(par); par ^= 1;
      if (bcount > 600) { } else bstep(par), par ^= 1;
      if (towers && !(fr % 30) && bn[1]) {         // your towers pick off attackers
        u16 t = bal[1][rnd() % bn[1]];
        if (bshn < 24) { BShot *q = &bsh[bshn++]; q->x = 10 << 4; q->y = (s16)((34 + (rnd() % 4) * 30) << 4); q->tx = ba[t].x; q->ty = ba[t].y; q->t = 8; }
        ba[t].hp = 0; bpx8(bbg, ba[t].x >> 4, ba[t].y >> 4, BC_DEAD);
      }
      if (!(fr & 7)) { bcompact(); bhud(bbg); }
      DMAR(3, 0) = (u32)bbg; DMAR(3, 4) = (u32)pg; DMAR(3, 8) = DMA_ON | DMA_W32 | (240 * 160 / 4);
      bdraw(pg);
      for (k = 0; k < bshn; ) { if (bsh[k].t) { bsh[k].t--; k++; } else bsh[k] = bsh[--bshn]; }
      vsync();
      DISPCNT = (u16)(0x0404 | ((fr & 1) ? 0 : 0x10));
      if (!(fr & 15) && !nosfx) noise(0x31, (u8)(0x40 + (rnd() & 0x1F)));
      fr++;
      if (joy() & J_START) { while (bn[0] && bn[1] && fr < 3600) { bstep(0); bstep(1); fr++; if (!(fr & 7)) bcompact(); } bcompact(); break; }
    }
    bcompact();
    won = bn[1] == 0 || bn[0] > bn[1];
    bhud(bbg);
    bbtext(bbg, won ? 84 : 92, 66, won ? "VICTORY!" : "DEFEAT", BC_GOLD);
    bbtext(bbg, 62, 84, "PRESS A", BC_TXT);
    pg = (volatile u16 *)(0x06000000 + ((fr & 1) ? 0 : 0xA000));
    DMAR(3, 0) = (u32)bbg; DMAR(3, 4) = (u32)pg; DMAR(3, 8) = DMA_ON | DMA_W32 | (240 * 160 / 4);
    bdraw(pg); vsync(); DISPCNT = (u16)(0x0404 | ((fr & 1) ? 0 : 0x10));
    if (won) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
    while (joy()) vsync();
    for (k = 0; k < 600 && !(joy() & (J_A | J_START)); k++) vsync();
    while (joy()) vsync();
    fade(0, 16);
    DISPCNT = 0x80;
    for (k = 0; k < 256; k++) BGPAL[k] = palsave[k];
    vram_reload();                                 // the bitmap overwrote the tiles: put them all back
  }
  sv = bside_count(0, BT_W); tw = bside_count(0, BT_S); *am = bside_count(0, BT_M); *aw = sv; *as = tw;
  *ew = bside_count(1, BT_W); *es = bside_count(1, BT_S); *em = bside_count(1, BT_M);
  return (u8)(bn[1] == 0 || (bn[0] > 0 && bn[0] >= bn[1]));
}
