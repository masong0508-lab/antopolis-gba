// ============================================================================================================================
//  ANT CITY (included from empire.c): a SimCity mode on the normal map screen.  And ANTVERSE: every mode at once.
//
//  You are the MAYOR. Dig TUNNELS, paint ZONES next to them (HOMES, FARMS, WORKS) and the ants build by themselves:
//  each zoned square grows from level 1 to 3 when the colony wants it (DEMAND), it has a tunnel, and its block is nice
//  enough (LAND VALUE, AIR, GUARDS, CLINIC, SCHOOL...). Taxes pay for services. Smog, crime, fires, floods, cave-ins and
//  a giant beetle get in the way. 10 SCENARIOS; finish them all to open ANTVERSE (city + empire + rivals + god powers).
//
//  Data per cell lives in ezt: bits 0-2 territory, 3-4 zone, 5-6 level (0 = empty lot), 7 tunnel.
//  Per 4x4 block (24 x 24): land value, smog, crime, service coverage, parks, water. A month = 30 game ticks.
// ============================================================================================================================
#define ELV(y, x)  ((ezt[y][x] >> 5) & 3)
#define ETUN(y, x) (ezt[y][x] >> 7)
static void set_lv(u8 y, u8 x, u8 v) { ezt[y][x] = (u8)((ezt[y][x] & ~0x60) | (v << 5)); }
static void set_tun(u8 y, u8 x, u8 v) { ezt[y][x] = (u8)((ezt[y][x] & 0x7F) | (v << 7)); }
#define CBW 24                                     // blocks across / down (4 x 4 cells each)
enum { CV_AIR = 1, CV_GUARD = 2, CV_CLINIC = 4, CV_SCHOOL = 8, CV_PUMP = 16 };
static u8 clv[CBW][CBW], cpol[CBW][CBW], ccri[CBW][CBW], ccov[CBW][CBW], cpk[CBW][CBW], cwat[CBW][CBW], csrc[CBW][CBW], cden[CBW][CBW];

static u32 cpop, cjw, cjf, cfood_p;                // residents, works jobs, farm jobs, food grown last month
static u16 czn[4][4];                              // zoned squares per zone and level
static u16 ctun, cnoacc, cmon, cyears_lim;         // tunnels, zoned squares with no tunnel, months played, scenario month limit
static s8 dR, dF, dW;                              // demand -100..100
static u8 taxR = 9, taxF = 9, taxW = 9, cmile, cdis_on, chunger, cmoney = 1, cscen = 255, csmog, ccrime, cvl, cbroke, cfires_n, cnews;
static u16 claws;                                  // laws (ordinances) switched on, one bit each
static s16 inc_r, inc_f, inc_w, inc_x, exp_s, exp_t, exp_l, exp_n;   // last month's budget
static u8 cloan;                                   // months of loan left to pay
static u16 cscan;                                  // incremental map scan: next row
static u8 cchg, cmaxc = 8, cquiet;                         // zone changes this month, and the most allowed
static u32 a_pop, a_jw, a_jf, a_fp; static u16 a_zn[4][4], a_tun, a_noacc;   // scan accumulators
static u16 hpop[48]; static s16 hcash[48]; static u8 nh2;
// disasters
#define NFIRE 20
static u16 fcell[NFIRE]; static u8 ftime[NFIRE];
#define NFLOOD 40
static u16 flcell[NFLOOD]; static u8 fltime;
static u8 bt_x, bt_y, bt_on, bt_hp, bt_ox, bt_oy, bt_age, bt_fl; static s8 bt_dx, bt_dy;

// ---------- tools ----------
enum { L_TUN = 106, L_DOZE = 107, P_RAISE = 110, P_LOWER, P_FLOOD, P_RAIN, P_BLESS };
static const char *const CTN[16] = {"PILE UP LAND", "DIG DOWN LAND", "ZONE: HOMES", "ZONE: FARMS", "ZONE: WORKS", "REMOVE ZONE", "TUNNEL", "BULLDOZE", "", "",
  "RAISE LAND", "LOWER LAND", "FLOOD", "RAIN", "BLESSING"};
static const char *const CTD[16] = {"*20  RAISE THE GROUND", "*20  LOWER THE GROUND", "*5  ANTS MOVE IN HERE", "*5  FOOD GROWS HERE", "*5  JOBS ARE MADE HERE",
  "*1  CLEAR A ZONE", "*10  ZONES NEED ONE CLOSE", "*2  CLEAR ANYTHING", "", "",
  "2 AP  GROUND UP", "2 AP  GROUND DOWN", "8 AP  SINK LAND + ARMIES", "5 AP  PUTS OUT FIRES", "10 AP  EVERYONE HAPPIER"};
static const u8 TOOLC[8] = {20, 20, 5, 5, 5, 1, 10, 2};
static const u8 PWRC[5] = {2, 2, 8, 5, 10};
static const char *const ZNAME[4] = {"", "HOMES", "FARMS", "WORKS"};
static const char *const MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
static const u16 MILE[6] = {0, 150, 600, 2000, 6000, 15000};
static const char *const MILEN[6] = {"OUTPOST", "VILLAGE", "TOWN", "CITY", "METROPOLIS", "MEGALOPOLIS"};
#define NLAW 8
static const char *const LAWN[NLAW] = {"CLEAN AIR", "NEIGHBOR WATCH", "FREE CLINICS", "NIGHT SHIFT", "TOURISM", "FARMERS MARKET", "FIRE CODES", "TAX HOLIDAY"};
static const char *const LAWD[NLAW] = {"SMOG -30 PERCENT", "CRIME -20", "LAND VALUE +6", "MORE JOBS  MORE SMOG", "PARKS BRING VISITORS: MONEY",
  "FOOD SELLS FOR DOUBLE", "FIRES SPREAD LESS", "EVERYONE WANTS TO MOVE IN"};
static const u8 LAWC[NLAW] = {40, 20, 50, 0, 10, 15, 25, 0};
static u8 c_fout, c_pw;                            // fires saved by dew pumps, god powers used (trophies)
static u8 law(u8 i) { return (u8)((claws >> i) & 1); }
static u16 ccost(u8 t) { return bc((u16)((BD[t].f + BD[t].d + 2 * BD[t].a) * 10)); }
static u8 cupk(u8 t) { return (u8)((BD[t].f + BD[t].d + 2 * BD[t].a) / 5); }
static u8 city_b(u8 t) { return t >= B_GUARD; }   // city service buildings: paid with money in ANTVERSE too
static u8 cash_paid(u8 t) { return ecity == 1 || t >= 100 || city_b(t); }

// ---------- per-cell helpers ----------
static u8 has_access(u8 x, u8 y) {                 // a tunnel (or the nest) within 2 squares
  s8 a, b;
  if (dist(x, nestx[0]) <= 3 && dist(y, nesty[0]) <= 3) return 1;
  for (b = (s8)(y - 2); b <= y + 2; b++) for (a = (s8)(x - 2); a <= x + 2; a++)
    if (a >= 0 && b >= 0 && a < W && b < H && ETUN(b, a)) return 1;
  return 0;
}
static u8 in_fire(u8 x, u8 y) { u8 i; u16 c = (u16)((y << 7) | x); for (i = 0; i < NFIRE; i++) if (ftime[i] && fcell[i] == c) return 1; return 0; }
static u8 in_flood(u8 x, u8 y) { u8 i; u16 c = (u16)((y << 7) | x); if (!fltime) return 0; for (i = 0; i < NFLOOD; i++) if (flcell[i] == c) return 1; return 0; }
static u16 city_tile(u8 x, u8 y) {                 // what to draw on the decor layer for a city square (0xFFFF = nothing special)
  u8 z, l, m = 0, k;
  if (cfires_n && in_fire(x, y)) return 69;
  if (fltime && in_flood(x, y)) return 70;
  if (ETUN(y, x)) {
    for (k = 0; k < 4; k++) { s8 a = (s8)x + DX[k], b = (s8)y + DY[k];
      if (a >= 0 && b >= 0 && a < W && b < H && (ETUN(b, a) || (dist((u8)a, nestx[0]) == 0 && dist((u8)b, nesty[0]) == 0))) m |= (u8)(k == 0 ? 2 : k == 1 ? 8 : k == 2 ? 4 : 1); }
    return (u16)(45 + m);
  }
  z = EZN(y, x); l = ELV(y, x);
  if (!z) return 0xFFFF;
  return l ? (u16)(36 + (z - 1) * 3 + l - 1) : (u16)(32 + z);
}
static void cell_info(u8 x, u8 y, u16 *people, u16 *jobs, u8 *food) {
  u8 z = EZN(y, x), l = ELV(y, x);
  static const u8 HR[4] = {0, 8, 24, 64}, WJ[4] = {0, 4, 12, 30}, FJ[4] = {0, 3, 8, 16}, FP[4] = {0, 2, 5, 10};
  *people = 0; *jobs = 0; *food = 0;
  if (!l) return;
  if (z == 1) *people = HR[l];
  else if (z == 3) *jobs = (u16)(WJ[l] + (law(3) ? WJ[l] / 5 : 0));
  else if (z == 2) { *jobs = FJ[l]; *food = FP[l]; }
}
// why a zoned square cannot grow (0 = it can). Also used by the INFO panel, so the player always knows what to do.
static const char *grow_block(u8 x, u8 y) {
  u8 z = EZN(y, x), l = ELV(y, x), bx = (u8)(x >> 2), by = (u8)(y >> 2), cv = ccov[by][bx];
  s8 d = z == 1 ? dR : z == 2 ? dF : dW;
  if (!z) return "NOT ZONED";
  if (!hgt[y][x]) return "UNDER WATER";
  if (l >= 3) return "FULLY GROWN!";
  if (!has_access(x, y)) return "NO TUNNEL CLOSE BY";
  if (d <= 0) return z == 1 ? "NO DEMAND FOR HOMES" : z == 2 ? "NO DEMAND FOR FARMS" : "NO DEMAND FOR WORKS";
  if (l >= 1) {
    if (!(cv & CV_AIR)) return "NEEDS AN AIR SHAFT";
    if (z == 2 && cpol[by][bx] > 40) return "TOO SMOGGY FOR FARMS";
    if (z == 1 && ccri[by][bx] > 60) return "CRIME: NEEDS GUARDS";
    if (z != 3 && clv[by][bx] < 30) return "LOW VALUE: ADD PARKS";
  }
  if (l >= 2) {
    if (cmile < 2) return "CITY MUST BE A TOWN";
    if (!(cv & CV_GUARD)) return "NEEDS A GUARD POST";
    if (z == 1 && !(cv & CV_CLINIC)) return "NEEDS A CLINIC";
    if (z != 3 && !(cv & CV_SCHOOL)) return "NEEDS A SCHOOL";
    if (z == 3 && !(cv & CV_PUMP)) return "NEEDS A DEW PUMP";
    if (z != 3 && clv[by][bx] < 55) return "NEEDS LAND VALUE 55";
  }
  return 0;
}

// ---------- building and painting ----------
static const char *city_can(u8 t, u8 x, u8 y) {
  u8 h = hgt[y][x], i;
  if (t >= P_RAISE) {
    if (mana < PWRC[t - P_RAISE]) return "NOT ENOUGH AP";
    if (t == P_RAISE && h >= 3) return "ALREADY HIGHEST";
    if (t == P_LOWER && !h) return "ALREADY WATER";
    if ((t == P_RAISE || t == P_LOWER) && ebi[y][x]) return "A BUILDING IS THERE";
    return 0;
  }
  if (ecity == 2 && ETER(y, x) != 1) return "OUTSIDE YOUR LAND";
  for (i = 0; i < enriv; i++) if (ralive[i] && x == rx[i] && y == ry[i]) return "ENEMY NEST";
  if (t >= 100) {
    u16 c = TOOLC[t - 100];
    if (t == L_DOZE) { if (!ebi[y][x] && !ETUN(y, x) && !EZN(y, x)) return "NOTHING TO CLEAR"; if (ebi[y][x] && ebl[ebi[y][x] - 1].t == B_NEST) return "NOT THE NEST!"; }
    else if (ebi[y][x]) return "A BUILDING IS THERE";
    if (t == L_PILE && h >= 3) return "ALREADY HIGHEST";
    if (t == L_DIG && !h) return "ALREADY WATER";
    if ((t == L_TUN || (t >= L_ZH && t <= L_ZW)) && !h) return "NOT ON WATER";
    if (t == L_TUN && ETUN(y, x)) return "TUNNEL IS THERE";
    if (t >= L_ZH && t <= L_ZW) { if (ETUN(y, x)) return "A TUNNEL IS THERE"; if (EZN(y, x) == t - L_ZH + 1) return "ALREADY ZONED"; }
    if (t == L_ZX && !EZN(y, x)) return "NO ZONE HERE";
    if (cmoney != 3 && cash < c) return "NOT ENOUGH MONEY";
    return 0;
  }
  if (ecity == 2 && !city_b(t) && !has_t(BD[t].tech)) return "RESEARCH IT FIRST";
  if (ebi[y][x]) return "A BUILDING IS THERE";
  if (!((BD[t].ter >> h) & 1)) return t == B_BRIDGE ? "ONLY ON WATER" : t == B_MINE ? "ONLY ON HILLS" : h == 0 ? "NOT ON WATER" : "WRONG GROUND HERE";
  if (t == B_STADIUM && cmile < 2) return "NEEDS A TOWN";
  if (t == B_ARCO && cmile < 4) return "NEEDS A METROPOLIS";
  if ((t == B_WONDER || t == B_ARCO || t == B_PALACE) && nb_of[t]) return "ONLY ONE";
  if (cash_paid(t)) { if (cmoney != 3 && cash < ccost(t)) return "NOT ENOUGH MONEY"; }
  else if (rf < bc(BD[t].f) || rd < bc(BD[t].d) || ra < bc(BD[t].a)) return "NOT ENOUGH STUFF";
  return 0;
}
static void pay(u16 c) { if (cmoney != 3) cash -= c; }
static void cburn_clear(u8 x, u8 y) { u8 i; u16 c = (u16)((y << 7) | x); for (i = 0; i < NFIRE; i++) if (ftime[i] && fcell[i] == c) { ftime[i] = 0; cfires_n--; } }
static void city_flood_power(u8 x, u8 y);
static u8 city_do(u8 t, u8 x, u8 y) {
  const char *why = city_can(t, x, y);
  if (why) { say(why); sfx_deny(); return 0; }
  if (t >= P_RAISE) {
    mana -= PWRC[t - P_RAISE]; c_pw |= (u8)(1 << (t - P_RAISE)); if (c_pw == 31) ach_get(45);
    if (t == P_RAISE) { hgt[y][x]++; draw_around(x, y); sfx_raise(); }
    else if (t == P_LOWER) { hgt[y][x]--; if (!hgt[y][x]) { ezt[y][x] &= 7; } draw_around(x, y); sfx_lower(); }
    else if (t == P_FLOOD) city_flood_power(x, y);
    else if (t == P_RAIN) { u8 i; for (i = 0; i < NFIRE; i++) ftime[i] = 0; cfires_n = 0; efarm_mod = 1; efarm_t = 6; say("RAIN! FIRES ARE OUT"); sfx_flood(); win_all(); }
    else { approval_all(10); say("THE COLONY FEELS BLESSED"); sfx_mana(); dR = (s8)(dR + 20 > 100 ? 100 : dR + 20); }
    return 1;
  }
  if (t >= 100) {
    pay(TOOLC[t - 100]);
    if (t == L_PILE) { hgt[y][x]++; draw_around(x, y); sfx_raise(); }
    else if (t == L_DIG) { hgt[y][x]--; if (!hgt[y][x]) ezt[y][x] &= 7; draw_around(x, y); sfx_lower(); }
    else if (t == L_TUN) { set_zn(y, x, 0); set_lv(y, x, 0); set_tun(y, x, 1); eredraw(x, y, 1); sfx_food(); }
    else if (t == L_DOZE) {
      if (ebi[y][x]) { del_b((u8)(ebi[y][x] - 1)); terr_calc(); }
      else if (ETUN(y, x)) set_tun(y, x, 0);
      else if (ELV(y, x)) set_lv(y, x, 0);
      else set_zn(y, x, 0);
      cburn_clear(x, y); eredraw(x, y, 1); sfx_lower(); fx_add(x, y, FX_PUFF);
    }
    else { u8 z = t == L_ZX ? 0 : (u8)(t - L_ZH + 1); if (EZN(y, x) != z) set_lv(y, x, 0); set_zn(y, x, z); emp_dec(x, y); sfx_food(); }
    return 1;
  }
  if (cash_paid(t)) pay(ccost(t)); else { rf -= bc(BD[t].f); rd -= bc(BD[t].d); ra -= bc(BD[t].a); }
  ezt[y][x] &= 7;                                  // a building replaces the zone and the tunnel
  add_b(x, y, t); terr_calc(); eredraw(x, y, 1); sfx_spawn(); fx_add(x, y, FX_PLUS);
  return 1;
}

// ---------- disasters ----------
static void fire_start(u8 x, u8 y) {
  u8 i;
  if (cht[19] || in_fire(x, y) || !hgt[y][x]) return;
  for (i = 0; i < NFIRE; i++) if (!ftime[i]) { fcell[i] = (u16)((y << 7) | x); ftime[i] = (u8)(40 + (rnd() & 15)); cfires_n++; emp_dec(x, y); return; }
}
static u8 burnable(u8 x, u8 y) { return ELV(y, x) || (ebi[y][x] && ebl[ebi[y][x] - 1].t != B_NEST); }
static void fire_any(void) {                       // set a random grown square on fire
  u16 k; u8 x, y;
  for (k = 0; k < 400; k++) { x = (u8)(rnd() % W); y = (u8)(rnd() % H); if (burnable(x, y)) { fire_start(x, y); alt = 240; alx = x; aly = y; say("FIRE! \6 LOOK  DEW PUMPS HELP"); sfx_alarm(); return; } }
}
static void flood_any(void) {
  u16 k; u8 x, y, n = 0; s8 a, b;
  for (k = 0; k < 400; k++) { x = (u8)(rnd() % W); y = (u8)(rnd() % H); if (!hgt[y][x]) break; }
  if (k == 400) { say("NO WATER TO FLOOD"); return; }
  for (b = (s8)(y - 7); b <= y + 7 && n < NFLOOD; b++) for (a = (s8)(x - 7); a <= x + 7 && n < NFLOOD; a++)
    if (a >= 0 && b >= 0 && a < W && b < H && hgt[b][a] == 1) {
      flcell[n++] = (u16)((b << 7) | a);
      if (ELV(b, a)) set_lv((u8)b, (u8)a, (u8)(ELV(b, a) - 1));
      if (ebi[b][a] && ebl[ebi[b][a] - 1].t != B_NEST) { EB *e = &ebl[ebi[b][a] - 1]; e->hp = (u8)(e->hp / 2 + 1); }
    }
  for (; n < NFLOOD; n++) flcell[n] = 0xFFFF;
  fltime = 60; alt = 240; alx = x; aly = y; say("FLOOD! LOW GROUND IS UNDER WATER"); sfx_flood(); shk = 12; win_all();
}
static void cavein_any(void) {
  u16 k; u8 x, y; s8 a, b;
  for (k = 0; k < 400; k++) { x = (u8)(rnd() % W); y = (u8)(rnd() % H); if (ELV(y, x)) break; }
  if (k == 400) { x = (u8)(W / 2); y = (u8)(H / 2); }
  for (b = (s8)(y - 1); b <= y + 1; b++) for (a = (s8)(x - 1); a <= x + 1; a++)
    if (a >= 0 && b >= 0 && a < W && b < H) { set_lv((u8)b, (u8)a, 0); set_tun((u8)b, (u8)a, 0);
      if (ebi[b][a] && ebl[ebi[b][a] - 1].t != B_NEST) { del_b((u8)(ebi[b][a] - 1)); } fx_add((u8)a, (u8)b, FX_PUFF); }
  terr_calc(); eredraw(x, y, 2); shk = 30; sfx_qdead(); alt = 240; alx = x; aly = y; say("CAVE IN! \6 LOOK");
}
static void beetle_any(void) {
  u8 side = (u8)(rnd() & 3);
  bt_x = side == 0 ? 0 : side == 1 ? (u8)(W - 1) : (u8)(rnd() % W); bt_y = side >= 2 ? (side == 2 ? 0 : (u8)(H - 1)) : (u8)(rnd() % H);
  bt_dx = (s8)(side == 0 ? 1 : side == 1 ? -1 : 0); bt_dy = (s8)(side == 2 ? 1 : side == 3 ? -1 : 0);
  bt_on = 1; bt_hp = 40; bt_ox = bt_x; bt_oy = bt_y; bt_age = 8;
  alt = 240; alx = bt_x; aly = bt_y; say("A GIANT BEETLE! \6 LOOK  GUARDS HURT IT"); sfx_alarm();
}
static void city_flood_power(u8 x, u8 y) {         // god power: sink a 3 x 3 patch; enemy armies there lose a third
  s8 a, b; u8 i;
  for (b = (s8)(y - 1); b <= y + 1; b++) for (a = (s8)(x - 1); a <= x + 1; a++)
    if (a >= 0 && b >= 0 && a < W && b < H && hgt[b][a] && !ebi[b][a]) { hgt[b][a]--; if (!hgt[b][a]) ezt[b][a] &= 7; }
  for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].team && dist(ea[i].x, x) <= 2 && dist(ea[i].y, y) <= 2) { loss(&ea[i].w, &ea[i].s, &ea[i].m, (u16)(a_size(&ea[i]) / 3 + 1)); fx_add(ea[i].x, ea[i].y, FX_SPLASH); }
  for (b = (s8)(y - 2); b <= y + 2; b++) for (a = (s8)(x - 2); a <= x + 2; a++) if (a >= 0 && b >= 0 && a < W && b < H) draw_cell((u8)a, (u8)b);
  pdirty = 1; shk = 10; sfx_flood(); say("THE WATERS RISE!");
}

// ---------- the scan (spread over the month so there is no hitch); every zoned square gets one chance to grow ----------
static void grow_try(u8 x, u8 y) {
  u8 z = EZN(y, x), l = ELV(y, x); s8 d = z == 1 ? dR : z == 2 ? dF : dW;
  if (ebi[y][x] || !hgt[y][x] || cchg >= cmaxc || (cfires_n && in_fire(x, y))) return;
  if (!grow_block(x, y)) {
    if ((u8)(rnd() % 100) < (u8)(15 + d * 85 / 100)) { set_lv(y, x, (u8)(l + 1)); emp_dec(x, y); if (!(rnd() & 3)) fx_add(x, y, FX_PLUS); cchg++; ezbuilt++; }
    return;
  }
  if (l && (!has_access(x, y) || (d < -20 && (u8)(rnd() % 100) < (u8)(-d / 3)) || (l >= 2 && clv[y >> 2][x >> 2] < 15 && !(rnd() & 3)))) { set_lv(y, x, (u8)(l - 1)); emp_dec(x, y); cchg++; }
}
static void scan_rows(u8 n) {
  u8 x, y; u16 p, j; u8 f;
  for (; n && cscan < H; n--, cscan++) {
    y = (u8)cscan;
    for (x = 0; x < W; x++) {
      u8 z = EZN(y, x), l = ELV(y, x), bx = (u8)(x >> 2), by = (u8)(y >> 2);
      if (!hgt[y][x]) cwat[by][bx] = 1;
      if (ETUN(y, x)) { a_tun++; continue; }
      if (!z) continue;
      a_zn[z][l]++;
      if (cmon) grow_try(x, y);
      if (!l) { if (!has_access(x, y)) a_noacc++; continue; }
      cell_info(x, y, &p, &j, &f);
      a_pop += p; if (z == 3) a_jw += j; else if (z == 2) { a_jf += j; a_fp += f; }
      if (z == 3) csrc[by][bx] = (u8)(csrc[by][bx] + (l == 1 ? 1 : l == 2 ? 3 : 6) > 250 ? 250 : csrc[by][bx] + (l == 1 ? 1 : l == 2 ? 3 : 6));
      if (z == 1) cden[by][bx] = (u8)(cden[by][bx] + l > 250 ? 250 : cden[by][bx] + l);
    }
  }
}
static void scan_reset(void) {
  u8 a, b, i;
  for (b = 0; b < CBW; b++) for (a = 0; a < CBW; a++) { csrc[b][a] = 0; cden[b][a] = 0; cwat[b][a] = 0; }
  for (a = 0; a < 4; a++) for (i = 0; i < 4; i++) a_zn[a][i] = 0;
  a_pop = a_jw = a_jf = a_fp = 0; a_tun = 0; a_noacc = 0; cscan = 0; cchg = 0;
}
static void cover(u8 x, u8 y, u8 r, u8 bit) {      // mark blocks within r squares
  s8 bx0 = (s8)((x - r) >> 2), bx1 = (s8)((x + r) >> 2), by0 = (s8)((y - r) >> 2), by1 = (s8)((y + r) >> 2), a, b;
  if (x < r) bx0 = 0; if (y < r) by0 = 0;
  for (b = by0; b <= by1 && b < CBW; b++) for (a = bx0; a <= bx1 && a < CBW; a++) { if (bit) ccov[b][a] |= bit; else if (cpk[b][a] < 9) cpk[b][a]++; }
}
static void blocks_calc(void) {                    // land value, smog, crime and coverage for every block
  u8 a, b, i; s16 v; s8 dx, dy; u16 nb = 0; u32 sp = 0, sc = 0;
  for (b = 0; b < CBW; b++) for (a = 0; a < CBW; a++) { ccov[b][a] = 0; cpk[b][a] = 0; }
  cover(nestx[0], nesty[0], 6, CV_AIR | CV_GUARD);
  for (i = 0; i < nbl; i++) {
    EB *e = &ebl[i];
    switch (e->t) {
    case B_AIR: cover(e->x, e->y, 10, CV_AIR); break;
    case B_GUARD: cover(e->x, e->y, 9, CV_GUARD); break;
    case B_CLINIC: cover(e->x, e->y, 9, CV_CLINIC); break;
    case B_SCHOOL: cover(e->x, e->y, 11, CV_SCHOOL); break;
    case B_PUMP: cover(e->x, e->y, 9, CV_PUMP); break;
    case B_PARK: case B_GARDEN: cover(e->x, e->y, 5, 0); break;
    case B_TEMPLE: case B_STATUE: case B_DANCE: cover(e->x, e->y, 6, 0); break;
    case B_STADIUM: case B_ARCO: case B_WONDER: case B_PALACE: cover(e->x, e->y, 12, 0); cover(e->x, e->y, 8, 0); break;
    case B_TOWER: if (ecity == 2) cover(e->x, e->y, 5, CV_GUARD); break;
    case B_HEAL: if (ecity == 2) cover(e->x, e->y, 6, CV_CLINIC); break;
    case B_ACAD: case B_ARCH: if (ecity == 2) cover(e->x, e->y, 7, CV_SCHOOL); break;
    }
  }
  for (b = 0; b < CBW; b++) for (a = 0; a < CBW; a++) {
    u16 s = (u16)csrc[b][a] * 3;
    for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) if ((dx || dy) && a + dx >= 0 && b + dy >= 0 && a + dx < CBW && b + dy < CBW) s += csrc[b + dy][a + dx];
    v = (s16)(s / 3) - cpk[b][a] * 4;
    if (law(0)) v = (s16)(v * 7 / 10);
    if (law(3)) v = (s16)(v * 11 / 10);
    if (PZ_.on) v += PZ_.m_smog;
    cpol[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    v = (s16)(cden[b][a] * 3 + cpol[b][a] / 4 - ((ccov[b][a] & CV_GUARD) ? 55 : 0) - (law(1) ? 20 : 0));
    if (PZ_.on) { v += PZ_.m_crime; if (pz_mart()) v -= 30; }
    ccri[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    v = (s16)(40 + cwat[b][a] * 12 + (cpk[b][a] > 4 ? 4 : cpk[b][a]) * 8 + ((ccov[b][a] & CV_GUARD) ? 6 : 0) + ((ccov[b][a] & CV_CLINIC) ? 6 : 0)
      + ((ccov[b][a] & CV_SCHOOL) ? 8 : 0) + ((ccov[b][a] & CV_PUMP) ? 4 : 0) + (law(2) ? 6 : 0) - cpol[b][a] * 6 / 10 - ccri[b][a] * 4 / 10);
    if (PZ_.on) v += PZ_.m_lv;
    clv[b][a] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
    if (cden[b][a] || csrc[b][a]) { nb++; sp += cpol[b][a]; sc += ccri[b][a]; }
  }
  csmog = (u8)(nb ? sp / nb : 0); ccrime = (u8)(nb ? sc / nb : 0);
}
static s8 clamp100(s32 v) { return (s8)(v < -100 ? -100 : v > 100 ? 100 : v); }

// ---------- the month ----------
static void city_end(u8 win, u8 why) { if (!eend) { eend = (u8)(win ? 1 : 2); ewhy = why; } }
static void city_cycle(void) {
  u8 i, x, y; s32 workers, jobs, need, v;
  while (cscan < H) scan_rows(8);                  // finish the scan if the month came early
  cpop = a_pop; cjw = a_jw; cjf = a_jf; cfood_p = a_fp; ctun = a_tun; cnoacc = a_noacc;
  for (x = 0; x < 4; x++) for (y = 0; y < 4; y++) czn[x][y] = a_zn[x][y];
  if (cpop > 99999) cpop = 99999;
  blocks_calc();
  cmon++;
  // food: residents eat; farms grow; extra food is sold
  need = (s32)(cpop / 25);
  if (ecity == 1) {
    v = (s32)rf + (s32)cfood_p * (efarm_mod > 0 ? 2 : 1) / (efarm_mod < 0 ? 2 : 1) - need;
    chunger = v < 0; rf = (u16)(v < 0 ? 0 : v > 999 ? 999 : v);
    if (efarm_t && !--efarm_t) efarm_mod = 0;
  } else { radd(&rf, (s16)(cfood_p > 200 ? 200 : cfood_p)); chunger = rf == 0; radd(&rd, (s16)(cjw / 20 > 50 ? 50 : cjw / 20)); if (czn[3][3]) radd(&ra, (s16)(czn[3][3] / 4 + 1)); }
  inc_x = 0; if (rf > 400) { inc_x = (s16)((rf - 400) / (law(5) ? 1 : 2)); rf = 400; }
  // demand
  workers = (s32)(cpop / 2); jobs = (s32)(cjw + cjf);
  dR = clamp100(40 + (jobs * 2 - (s32)cpop) * 60 / ((s32)cpop + 40) - taxR * 3 - (chunger ? 40 : 0) + (law(7) ? 15 : 0) + (cmile >= 3 ? 5 : 0));
  dW = clamp100(40 + (workers * 6 / 10 - (s32)cjw) * 60 / (workers * 6 / 10 + 20) - taxW * 3 + (law(7) ? 15 : 0));
  dF = clamp100(25 + (need + 5 - (s32)cfood_p) * 60 / (need + 10) + (workers * 4 / 10 - (s32)cjf) * 20 / (workers * 4 / 10 + 20) - taxF * 3 + (law(7) ? 15 : 0) + (inc_x > 0 ? 10 : 0));
  if (PZ_.on) { dR = clamp100(dR + PZ_.m_dr + (pz_ed(E_OPEN) ? 10 : 0) - (pz_ed(E_CLOSED) ? 10 : 0)); }
  // growth happens during the scan (grow_try); this sets how many squares may change next month
  cmaxc = (u8)(6 + cpop / 250 > 60 ? 60 : 6 + cpop / 250); if (cht[9]) cmaxc = (u8)(cmaxc * ZONEV[cht[9]] > 200 ? 200 : cmaxc * ZONEV[cht[9]]);
  if (PZ_.on && PZ_.m_grow) { s16 g = (s16)(cmaxc * (100 + PZ_.m_grow) / 100); cmaxc = (u8)(g < 2 ? 2 : g > 200 ? 200 : g); }
  // money
  inc_r = (s16)(cpop * taxR / 150); inc_w = (s16)(cjw * taxW / 100); inc_f = (s16)(cjf * taxF / 120);
  if (law(7)) { inc_r = (s16)(inc_r * 8 / 10); inc_w = (s16)(inc_w * 8 / 10); inc_f = (s16)(inc_f * 8 / 10); }
  if (cht[18]) { inc_r = (s16)(inc_r * (1 + cht[18])); inc_w = (s16)(inc_w * (1 + cht[18])); inc_f = (s16)(inc_f * (1 + cht[18])); }
  if (PZ_.on) { s16 m = (s16)(100 + PZ_.m_inc + (pz_ed(E_TAXRICH) ? 10 : 0) + (PZ_.con[4] == 1 ? 8 : 0)); if (m < 20) m = 20;
    inc_r = (s16)(inc_r * m / 100); inc_f = (s16)(inc_f * m / 100); inc_w = (s16)(inc_w * (m - (pz_ed(E_GREEN) ? 10 : 0)) / 100); }
  if (law(4)) { u8 pk = 0; for (i = 0; i < nbl; i++) if (ebl[i].t == B_PARK || ebl[i].t == B_GARDEN || ebl[i].t == B_STADIUM) pk++; inc_x += (s16)(pk >= 3 ? cpop / 100 + pk * 3 : 0); }
  exp_s = 0; for (i = 0; i < nbl; i++) if (cash_paid(ebl[i].t) && ebl[i].t != B_NEST) exp_s += cupk(ebl[i].t);
  exp_t = (s16)(ctun / 10); exp_l = 0; for (i = 0; i < NLAW; i++) if (law(i)) exp_l += LAWC[i];
  exp_n = cloan ? 110 : 0; if (cloan) cloan--;
  if (cmoney != 3) cash += inc_r + inc_w + inc_f + inc_x - exp_s - exp_t - exp_l - exp_n; else cash = 99999;
  if (cash > 999999) cash = 999999;
  if (cash < -1000 && cmoney != 3) { if (++cbroke >= 12) city_end(0, 20); else if (cbroke == 1 || !(cbroke % 3)) say("IN DEBT! RAISE TAXES OR CUT COSTS"); } else cbroke = 0;
  // antverse: the empire's workforce comes from the city
  if (ecity == 2) { pop = (u8)(6 + cpop / 20 > 250 ? 250 : 6 + cpop / 20); popcap = (u8)(pop + 4); }
  // milestones
  while (cmile < 5 && cpop >= MILE[cmile + 1]) {
    cmile++; if (cquiet) continue;
    if (cmoney != 3) cash += 500 * cmile;
    cnews = (u8)(cmile + 1);
    jingle(WIN_TUNE, 6);
  }
  // disasters
  if (cdis_on && cmon > 6 && !(rnd() % 40)) { u8 r = (u8)(rnd() & 3); if (r == 0) fire_any(); else if (r == 1) flood_any(); else if (r == 2) cavein_any(); else if (cpop > 400) beetle_any(); }
  if (cscen != 255) {                              // scenario disasters on a timetable
    u8 d = 0; if (cscen == 4 && cmon % 24 == 12) d = 2; if (cscen == 8 && cmon % 12 == 8) d = 4; if (cscen == 6 && cmon % 30 == 20) d = 1;
    if (d == 1) fire_any(); else if (d == 2) flood_any(); else if (d == 4) beetle_any();
  }
  // tips now and then
  if (!(cmon % 5)) {
    if (chunger) say("HUNGRY ANTS! ZONE MORE FARMS");
    else if (cnoacc > 6) say("SOME ZONES HAVE NO TUNNEL");
    else if (czn[1][1] + czn[2][1] + czn[3][1] > 10 && !(ccov[nesty[0] >> 2][nestx[0] >> 2] & CV_AIR)) say("BUILD AIR SHAFTS TO GROW");
    else if (ccrime > 45) say("CRIME IS HIGH: GUARD POSTS");
    else if (csmog > 45) say("SMOG! PARKS OR CLEAN AIR LAW");
  }
  if (nh2 == 48) { for (i = 0; i < 47; i++) { hpop[i] = hpop[i + 1]; hcash[i] = hcash[i + 1]; } nh2 = 47; }
  hpop[nh2] = (u16)(cpop > 65000 ? 65000 : cpop); hcash[nh2] = (s16)(cash > 32000 ? 32000 : cash < -32000 ? -32000 : cash); nh2++;
  if (ecity == 2 && nb_of[B_ARCO] && !mis_on) city_end(1, 5);
  // trophies that fit the city
  if (cpop >= 2000) ach_get(14); if (ezbuilt >= 10) ach_get(25);
  if (cash >= 20000) ach_get(37); if (cmile >= 5) ach_get(38);
  if (cpop >= 2000 && csmog < 10) ach_get(39); if (cpop >= 2000 && ccrime < 10) ach_get(40);
  if (czn[1][3] >= 50) ach_get(50); if (inc_r + inc_w + inc_f + inc_x - exp_s - exp_t - exp_l - exp_n >= 500) ach_get(51);
  pz_month(); mis_tick();
  scan_reset();
}
static u8 city_goal_ok(void);
static void city_tick(void) {                      // every game tick: the scan, fires, the flood, the beetle, AP
  u8 i, k;
  scan_rows((u8)((H + 23) / 24));
  if ((gt & 15) == 0 && mana < MANA_MAX && ecity == 2) mana++;
  if (cfires_n && (gt & 3) == 0) for (i = 0; i < NFIRE; i++) if (ftime[i]) {
    u8 x = (u8)(fcell[i] & 127), y = (u8)(fcell[i] >> 7), pumped = (ccov[y >> 2][x >> 2] & CV_PUMP) != 0;
    ftime[i] = (u8)(ftime[i] > (pumped ? 6 : 1) ? ftime[i] - (pumped ? 6 : 1) : 0);
    if (!pumped && !(rnd() % (law(6) ? 16 : 7))) { k = (u8)(rnd() & 3); { s8 a = (s8)x + DX[k], b = (s8)y + DY[k]; if (a >= 0 && b >= 0 && a < W && b < H && burnable((u8)a, (u8)b)) fire_start((u8)a, (u8)b); } }
    if (!ftime[i]) {
      cfires_n--;
      if (pumped) { if (++c_fout >= 10) ach_get(41); emp_dec(x, y); say("THE DEW PUMPS SAVED IT!"); continue; }   // pumps put it out in time
      if (ebi[y][x]) { EB *e = &ebl[ebi[y][x] - 1]; if (e->t != B_NEST) { del_b((u8)(ebi[y][x] - 1)); terr_calc(); } }
      else if (ELV(y, x)) set_lv(y, x, 0);
      eredraw(x, y, 1); fx_add(x, y, FX_PUFF);
    }
  }
  if (fltime && !--fltime) { win_all(); say("THE FLOOD IS GONE"); }
  if (bt_on && bt_age < 8) bt_age++;
  if (bt_on && (gt % 3) == 0) {                    // the beetle walks, eats what it steps on, guards bite it
    s8 nx, ny;
    if (!(rnd() & 3)) { if (bt_dx) bt_dy = (s8)((rnd() % 3) - 1); else bt_dx = (s8)((rnd() % 3) - 1); }
    if (bt_dx && bt_dy) { if (rnd() & 1) bt_dx = 0; else bt_dy = 0; }
    if (!bt_dx && !bt_dy) bt_dx = 1;
    nx = (s8)(bt_x + bt_dx); ny = (s8)(bt_y + bt_dy);
    if (nx < 0 || ny < 0 || nx >= W || ny >= H) { bt_on = 0; say("THE BEETLE HAS GONE"); return; }
    bt_ox = bt_x; bt_oy = bt_y; bt_x = (u8)nx; bt_y = (u8)ny; bt_age = 0; if (bt_dx) bt_fl = bt_dx < 0;
    if (ELV(bt_y, bt_x)) { set_lv(bt_y, bt_x, 0); emp_dec(bt_x, bt_y); fx_add(bt_x, bt_y, FX_PUFF); }
    if (ebi[bt_y][bt_x] && ebl[ebi[bt_y][bt_x] - 1].t != B_NEST) { del_b((u8)(ebi[bt_y][bt_x] - 1)); terr_calc(); eredraw(bt_x, bt_y, 1); }
    if (ccov[bt_y >> 2][bt_x >> 2] & CV_GUARD) { if (bt_hp <= 3) { bt_on = 0; say("THE GUARDS BEAT THE BEETLE!"); sfx_mana(); ach_get(42); return; } bt_hp -= 3; fx_add(bt_x, bt_y, FX_HEART); }
    shk = 3;
  }
}

// ---------- drawing: the HUD ----------
static void pcash(u8 x, u8 y, s32 v, u8 p) {       // money: up to 6 digits, '-' when in debt
  char b[8]; u8 n = 0, i; u32 a = (u32)(v < 0 ? -v : v);
  do { b[n++] = (char)('0' + a % 10); a /= 10; } while (a && n < 6);
  pc(x++, y, '*', v < 0 ? 8 : 9); if (v < 0) pc(x++, y, '-', 8);
  for (i = n; i--; ) pc(x++, y, b[i], v < 0 ? 8 : 9);
}
static void pnum(u8 x, u8 y, u32 v, u8 p) { char b[8]; u8 n = 0, i; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 6); for (i = n; i--; ) pc(x++, y, b[i], p); }
static void dbar(u8 x, u8 y, char c, s8 d) {       // a demand bar: green up, red down
  u8 i, n = (u8)((d < 0 ? -d : d) / 26 + (d != 0)), bank = d >= 0 ? 13 : 8;
  pc(x, y, c, d > 15 ? 13 : d < -15 ? 8 : 3);
  for (i = 0; i < 4; i++) BGMAP1[y * 32 + x + 1 + i] = (u16)((i < n ? BAR_F : BAR_E) | (bank << 12));
}
static void chud(void) {
  u8 x;
  ov_fill(1, 1, 17, 19);
  pcash(1, 17, cash, 9);
  pc(11, 17, '{', 1); pnum(13, 17, cpop, 1);
  ps(20, 17, MONTHS[cmon % 12], 3); pc(24, 17, 'Y', 10); pnum(25, 17, (u32)(cmon / 12 + 1), 3);
  spd_tag(27);
  if (ecity == 2) {
    pc(0, 18, '%', 13); p3(1, 18, rf, 13); pc(4, 18, '[', 2); p3(5, 18, rd, 2); pc(8, 18, ']', 9); p3(9, 18, ra, 9);
    pc(13, 18, '=', 8); { u16 t = my_troops(); p3(14, 18, t > 999 ? 999 : t, 1); }
    ps(18, 18, "AP", 14); pn(20, 18, mana, 14);
    pc(23, 18, 'H', dR > 15 ? 13 : dR < -15 ? 8 : 3); pc(24, 18, 'F', dF > 15 ? 13 : dF < -15 ? 8 : 3); pc(25, 18, 'W', dW > 15 ? 13 : dW < -15 ? 8 : 3);
  } else {
    dbar(1, 18, 'H', dR); dbar(8, 18, 'F', dF); dbar(15, 18, 'W', dW);
    pc(21, 18, '%', chunger ? 8 : 13); p3(23, 18, rf, chunger ? 8 : 13);
  }
  pz_hud();
  if (msgt) { psx(0, 19, msg, 2); msgt--; return; }
  if (eplace) {
    const char *why = city_can(eplace, cx, cy);
    { const char *m = why ? why : eplace >= 100 ? CTN[eplace - 100] : BD[eplace].nm; u8 i; for (i = 0; m[i] && i < 17; i++) pc(i, 19, m[i], why ? 8 : 13); }
    ps(18, 19, "\1 PUT \2 STOP", 1); return;
  }
  if (etgt) { ps(0, 19, "MARCH WHERE?  \1 GO  \2 CANCEL", 2); return; }
  if (ebi[cy][cx]) { EB *b = &ebl[ebi[cy][cx] - 1]; psx(0, 19, BD[b->t].nm, 2); ps(19, 19, "\1 OPEN", 1); return; }
  { u8 i; for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].x == cx && ea[i].y == cy) { psx(0, 19, ea[i].team ? RNAME[ea[i].team - 1] : "YOUR ARMY", ea[i].team ? 8 : 13); p3(10, 19, a_size(&ea[i]), 1); return; } }
  if (ETUN(cy, cx)) { ps(0, 19, "TUNNEL", 2); ps(15, 19, "\1 BUILD \2 INFO", 1); return; }
  if (EZN(cy, cx)) {
    const char *why = grow_block(cx, cy); u16 pp, jj; u8 ff;
    cell_info(cx, cy, &pp, &jj, &ff);
    x = psx(0, 19, ZNAME[EZN(cy, cx)], 2); pc((u8)(x + 1), 19, 'L', 10); pc((u8)(x + 2), 19, (char)('0' + ELV(cy, cx)), 9);
    if (why && ELV(cy, cx) < 3) { psx((u8)(x + 4), 19, why, why[0] == 'N' && why[3] == 'D' ? 10 : 8); return; }
    if (pp) { pnum((u8)(x + 4), 19, pp, 1); ps((u8)(x + 8), 19, "ANTS", 10); } else if (jj) { pnum((u8)(x + 4), 19, jj, 1); ps((u8)(x + 8), 19, "JOBS", 10); }
    else ps((u8)(x + 4), 19, "GROWING SOON", 13);
    ps(20, 19, "\2 INFO", 1); return;
  }
  if (!hgt[cy][cx]) { ps(0, 19, "WATER", 14); return; }
  ps(0, 19, "\1 BUILD  \2 INFO  \5 MENU", 1);
}

// ---------- the build menu (tabs with L / R) ----------
#define NCTAB 8
static const char *const CTABN[NCTAB] = {"ZONES", "TUNNELS + LAND", "SERVICES", "PARKS + FUN", "EMPIRE", "ARMY", "GOD POWERS", ""};
static const u8 CTABS[NCTAB][9] = {
  {L_ZH, L_ZF, L_ZW, L_ZX, 0},
  {L_TUN, L_DOZE, L_PILE, L_DIG, 0},
  {B_AIR, B_GUARD, B_PUMP, B_CLINIC, B_SCHOOL, 0},
  {B_PARK, B_GARDEN, B_TEMPLE, B_STATUE, B_DANCE, B_STADIUM, B_ARCO, 0},
  {B_MARKET, B_MINE, B_ARCH, B_ACAD, B_TRADE, B_VAULT, B_OUTPOST, B_WONDER, 0},
  {B_BARR, B_ARMORY, B_SPIT, B_TOWER, B_WALL, B_SPIKE, B_HEAL, B_EMBASSY, 0},
  {P_RAISE, P_LOWER, P_FLOOD, P_RAIN, P_BLESS, 0},
  {0}};
static u8 ctab, csel2;
static u8 city_build_menu(void) {
  u8 ntab = ecity == 2 ? 7 : 4, sel = csel2, tab = ctab, n, i, t, r = 0, top = 0;
  hide_all(); vsync(); oam_flush(); ewait_clear();
  if (tab >= ntab) tab = 0;
  for (;;) {
    for (n = 0; CTABS[tab][n]; n++) {}
    if (sel >= n) sel = (u8)(n - 1);
    ov_fill(1, 3, 0, 19); ui_full();
    ps(1, 0, "\3", 1); ps((u8)(15 - (blen2(CTABN[tab]) + 1) / 2), 0, CTABN[tab], 9); ps(28, 0, "\4", 1);
    for (i = 0; i < ntab; i++) BGMAP1[1 * 32 + 15 - ntab / 2 + i] = (u16)((ORN + (i == tab ? 3 : 4)) | (9 << 12));
    pm_rule(2);
    if (sel < top) top = sel; if (sel >= top + 7) top = (u8)(sel - 6);
    if (top) pc(29, 3, '-', 10); if (top + 7 < n) pc(29, 15, '+', 10);
    for (i = top; i < n && i < top + 7; i++) {
      u8 y = (u8)(3 + (i - top) * 2); const char *why; t = CTABS[tab][i];
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      if (t >= P_RAISE) { ps(3, y, CTN[t - 100], i == sel ? 9 : 3); ps(18, y, "AP", 14); pn(21, y, PWRC[t - P_RAISE], 14); continue; }
      if (t >= 100) { ps(3, y, CTN[t - 100], i == sel ? 9 : 3); pc(18, y, '*', 9); pn(19, y, TOOLC[t - 100], 9); continue; }
      why = (t == B_STADIUM && cmile < 2) ? "TOWN" : (t == B_ARCO && cmile < 4) ? "METROPOLIS" : (ecity == 2 && !city_b(t) && !has_t(BD[t].tech)) ? "LOCKED" : 0;
      ps(3, y, BD[t].nm, why ? 15 : i == sel ? 9 : 3);
      if (why) ps(18, y, why, 15); else if (cash_paid(t)) { pc(18, y, '*', 9); pnum(19, y, ccost(t), 9); } else cost_txt(16, y, t);
    }
    pm_rule(17);
    t = CTABS[tab][sel];
    if (t >= 100) ps(1, 18, CTD[t - 100], 3);
    else if (ecity == 2 && !city_b(t) && !has_t(BD[t].tech)) { ps(1, 18, "NEEDS:", 8); ps(8, 18, TNAME[BD[t].tech], 3); }
    else { ps(1, 18, BD[t].d1, 3); ps(1, 19, BD[t].d2, 10); }
    if (t >= 100) ps(1, 19, "\1 PICK  \2 BACK  \3\4 TABS", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_L) { tab = (u8)((tab + ntab - 1) % ntab); sel = 0; top = 0; sfx_food(); }
    if (emp_ & J_R) { tab = (u8)((tab + 1) % ntab); sel = 0; top = 0; sfx_food(); }
    if (emp_ & J_UP) { sel = sel ? sel - 1 : n - 1; sfx_food(); }
    if (emp_ & J_DOWN) { sel = sel + 1 >= n ? 0 : sel + 1; sfx_food(); }
    if (emp_ & J_A) {
      if (t < 100 && ((t == B_STADIUM && cmile < 2) || (t == B_ARCO && cmile < 4) || (ecity == 2 && !city_b(t) && !has_t(BD[t].tech)))) sfx_deny();
      else { r = t; break; }
    }
  }
  ctab = tab; csel2 = sel;
  ewait_clear(); ov_clear();
  return r;
}

// ---------- INFO: everything about one square, and what it needs ----------
static void vbar(u8 x, u8 y, u8 v, u8 good_high) {
  u8 i, n = (u8)((v + 9) / 10), bank = good_high ? (v >= 50 ? 13 : v >= 25 ? 14 : 8) : (v <= 25 ? 13 : v <= 50 ? 14 : 8);
  for (i = 0; i < 10; i++) BGMAP1[y * 32 + x + i] = (u16)((i < n ? BAR_F : BAR_E) | (bank << 12));
  pn((u8)(x + 11), y, v > 99 ? 99 : v, 3);
}
static u8 city_info(void) {                        // returns 1 if the player wants to build here
  u8 x = cx, y = cy, bx = (u8)(x >> 2), by = (u8)(y >> 2), cv = ccov[by][bx], r = 0; u16 pp, jj; u8 ff;
  static const char *const HN[4] = {"WATER", "SAND", "GRASS", "HILL"};
  static const char *const CVN[5] = {"AIR", "GUARD", "CLINIC", "SCHOOL", "PUMP"};
  hide_all(); vsync(); oam_flush(); ewait_clear();
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "INFO");
  ps(1, 2, HN[hgt[y][x]], 10);
  if (ebi[y][x]) { EB *b = &ebl[ebi[y][x] - 1]; ps(1, 3, BD[b->t].nm, 9); ps(1, 4, BD[b->t].d1, 3); ps(1, 5, BD[b->t].d2, 3); }
  else if (ETUN(y, x)) { ps(1, 3, "TUNNEL", 9); ps(1, 4, "ZONES 2 STEPS AWAY CAN GROW", 3); }
  else if (EZN(y, x)) {
    const char *why = grow_block(x, y);
    cell_info(x, y, &pp, &jj, &ff);
    ps(1, 3, ZNAME[EZN(y, x)], 9); ps(8, 3, "LEVEL", 10); pc(14, 3, (char)('0' + ELV(y, x)), 9);
    if (pp) { pnum(17, 3, pp, 1); ps(21, 3, "ANTS", 10); } else if (jj) { pnum(17, 3, jj, 1); ps(21, 3, "JOBS", 10); }
    if (ff) { pnum(17, 4, ff, 13); ps(21, 4, "FOOD", 10); }
    ps(1, 5, why ? "TO GROW:" : "IT CAN GROW", why ? 10 : 13); if (why) psx(1, 6, why, why[0] == 'F' ? 13 : 8);
  } else ps(1, 3, hgt[y][x] ? "EMPTY LAND: \1 TO BUILD" : "WATER", 3);
  pm_rule(7);
  ps(1, 8, "LAND VALUE", 10); vbar(13, 8, clv[by][bx], 1);
  ps(1, 10, "SMOG", 10); vbar(13, 10, cpol[by][bx], 0);
  ps(1, 12, "CRIME", 10); vbar(13, 12, ccri[by][bx], 0);
  ps(1, 14, "NEARBY:", 10);
  { u8 i, xx = 1; for (i = 0; i < 5; i++) { ps(xx, 15, CVN[i], (cv >> i) & 1 ? 13 : 15); xx = (u8)(xx + blen2(CVN[i]) + 1); } }
  if (!(cv & CV_AIR)) ps(1, 17, "TIP: AIR SHAFTS HELP GROWTH", 2);
  else if (!(cv & CV_GUARD)) ps(1, 17, "TIP: GUARD POSTS STOP CRIME", 2);
  else if (clv[by][bx] < 40) ps(1, 17, "TIP: PARKS RAISE LAND VALUE", 2);
  ps(1, 19, "\1 BUILD HERE   \2 BACK", 10);
  DISPCNT = 0x1F40;
  for (;;) { ekey(); if (emp_ & J_A) { r = hgt[y][x] && !ebi[y][x] && ETER(y, x) == 1; break; } if (emp_ & (J_B | J_START)) break; }
  ewait_clear(); ov_clear();
  return r;
}

// ---------- BUDGET: taxes, money in and out, a loan ----------
static void city_budget(void) {                    // taxes on top, the month's money below; R flips to the details
  u8 sel = 0, page = 0; s16 net, in, out;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "BUDGET");
    { u8 i; static const char *const TN2[3] = {"TAX ON HOMES", "TAX ON FARMS", "TAX ON WORKS"}; u8 *tv[3] = {&taxR, &taxF, &taxW};
      for (i = 0; i < 3; i++) { u8 y = (u8)(3 + i * 2);
        if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
        ps(3, y, TN2[i], i == sel ? 9 : 3); ps(17, y, "<", 10); pn(19, y, *tv[i], 9); ps(22, y, ">", 10);
        ps(24, y, *tv[i] <= 5 ? "LOW" : *tv[i] <= 10 ? "OK" : *tv[i] <= 14 ? "HIGH" : "UGH!", *tv[i] <= 10 ? 13 : 8); } }
    { u8 y = 9; if (sel == 3) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, cloan ? "PAYING A LOAN" : "TAKE A LOAN +2000", sel == 3 ? 9 : 3); if (cloan) { pn(19, y, cloan, 8); ps(22, y, "MONTHS", 10); } }
    pm_rule(10);
    in = (s16)(inc_r + inc_f + inc_w + (inc_x > 0 ? inc_x : 0)); out = (s16)(exp_s + exp_t + exp_l + exp_n); net = (s16)(in - out);
    if (!page) {
      ps(3, 11, "MONEY IN", 10); pc(17, 11, '+', 13); pnum(18, 11, (u32)in, 13);
      ps(3, 13, "MONEY OUT", 10); pc(17, 13, '-', 8); pnum(18, 13, (u32)out, 8);
      ps(3, 15, "EACH MONTH", 10); pc(17, 15, net < 0 ? '-' : '+', net < 0 ? 8 : 13); pnum(18, 15, (u32)(net < 0 ? -net : net), net < 0 ? 8 : 13);
      ps(3, 17, "IN THE BANK", 10); pcash(17, 17, cash, 9);
    } else {
      ps(2, 11, "HOMES", 10); pnum(10, 11, (u32)inc_r, 13);  ps(16, 11, "SERVICE", 10); pnum(25, 11, (u32)exp_s, 8);
      ps(2, 13, "FARMS", 10); pnum(10, 13, (u32)inc_f, 13); ps(16, 13, "TUNNELS", 10); pnum(25, 13, (u32)exp_t, 8);
      ps(2, 15, "WORKS", 10); pnum(10, 15, (u32)inc_w, 13); ps(16, 15, "LAWS", 10); pnum(25, 15, (u32)exp_l, 8);
      ps(2, 17, "SALES", 10); pnum(10, 17, (u32)(inc_x < 0 ? 0 : inc_x), 13); ps(16, 17, "LOAN", 10); pnum(25, 17, (u32)exp_n, 8);
    }
    ps(1, 19, page ? "\4 TOTALS  \2 BACK" : "< > TAX  \4 DETAILS  \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_R) { page ^= 1; sfx_food(); }
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 3);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 3 ? 0 : sel + 1);
    if (sel < 3 && (emp_ & (J_LEFT | J_RIGHT))) { u8 *tv = sel == 0 ? &taxR : sel == 1 ? &taxF : &taxW;
      if (emp_ & J_LEFT) { if (*tv) (*tv)--; } else if (*tv < 20) (*tv)++; sfx_food(); }
    if (sel == 3 && (emp_ & J_A)) { if (cloan) sfx_deny(); else { cloan = 24; cash += 2000; sfx_mana(); } }
  }
  ewait_clear();
}
static void city_laws(void) {
  u8 sel = 0, i, top = 0;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "LAWS");
    ps(1, 2, "TURN ON AS MANY AS YOU LIKE", 10);
    if (sel < top) top = sel; if (sel >= top + 5) top = (u8)(sel - 4);
    if (top) pc(29, 4, '-', 10); if (top + 5 < NLAW) pc(29, 12, '+', 10);
    for (i = top; i < NLAW && i < top + 5; i++) { u8 y = (u8)(4 + (i - top) * 2);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, LAWN[i], i == sel ? 9 : 3); ps(19, y, law(i) ? "ON" : "OFF", law(i) ? 13 : 15);
      if (LAWC[i]) { pc(23, y, '*', 10); pn(24, y, LAWC[i], 10); } }
    pm_rule(14); ps(1, 15, LAWD[sel], 3); if (LAWC[sel]) { ps(1, 17, "COSTS EVERY MONTH", 10); }
    ps(1, 19, "\1 ON OFF   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : NLAW - 1);
    if (emp_ & J_DOWN) sel = (u8)(sel + 1 >= NLAW ? 0 : sel + 1);
    if (emp_ & J_A) { claws ^= (u16)(1 << sel); sfx_mana(); }
  }
  ewait_clear();
}

// ---------- DATA MAPS (full screen, video mode 4): land value, smog, crime, every service ----------
static void city_data(void) {
  volatile u16 *pg = (volatile u16 *)0x06000000; u8 x, y, sc, i, c, ov = 0; int ox, oy, a, b;
  static const char *const OVN[9] = {"ZONES", "LAND VALUE", "SMOG", "CRIME", "AIR", "GUARDS", "CLINICS", "SCHOOLS", "DEW PUMPS"};
  static const u16 DC[32] = {0, RGB(6,13,26), RGB(26,23,13), RGB(9,20,7), RGB(18,13,8), RGB(2,1,0), RGB(31,31,31), RGB(12,12,12),
    RGB(31,10,10), RGB(10,28,8), RGB(28,24,4), RGB(8,14,31), RGB(3,3,5), RGB(18,18,18), 0, 0,
    RGB(4,24,6), RGB(10,26,6), RGB(18,27,5), RGB(26,26,4), RGB(30,20,3), RGB(30,12,3), RGB(28,4,4), RGB(20,0,8), 0, 0, 0, 0, 0, 0, 0, 0};
  sc = (u8)(240 / W < 136 / H ? 240 / W : 136 / H); if (!sc) sc = 1;
  ox = (240 - W * sc) / 2; oy = 14 + (136 - H * sc) / 2;
  music_stop(); fade(0, 16); hide_all(); oam_flush();
  for (;;) {
    DISPCNT = 0x0404 | 0x80;
    for (i = 0; i < 32; i++) BGPAL[i] = DC[i];
    for (a = 0; a < 240 * 160 / 2; a++) pg[a] = 0x0C0C;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
      u8 bx = (u8)(x >> 2), by = (u8)(y >> 2), v;
      if (!hgt[y][x]) c = 1;
      else if (ov == 0) { u8 z = EZN(y, x); c = ebi[y][x] ? 6 : ETUN(y, x) ? 5 : z == 1 ? (u8)(ELV(y, x) ? 9 : 17) : z == 2 ? (u8)(ELV(y, x) ? 10 : 19) : z == 3 ? (u8)(ELV(y, x) ? 11 : 13) : 7; if (z == 1 && ELV(y, x)) c = 9; }
      else if (ov <= 3) {
        v = ov == 1 ? (u8)(100 - clv[by][bx]) : ov == 2 ? cpol[by][bx] : ccri[by][bx];
        c = (u8)(16 + (v > 99 ? 99 : v) * 8 / 100);
        if (ETUN(y, x) || ebi[y][x]) c = 5;
      } else { c = (ccov[by][bx] >> (ov - 4)) & 1 ? 16 : 22; if (ETUN(y, x)) c = 5; if (ebi[y][x]) c = 6; if (!EZN(y, x) && !ETUN(y, x) && !ebi[y][x]) c = (u8)(c == 16 ? 17 : 13); }
      for (b = 0; b < sc; b++) for (a = 0; a < sc; a++) vplot(pg, ox + x * sc + a, oy + y * sc + b, c);
    }
    { const char *t = OVN[ov]; u8 gx, gy, g; int tx = 120 - blen2(t) * 4; for (; *t; t++, tx += 8) { g = gi(*t); for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) vplot(pg, tx + gx, 3 + gy, 10); } }
    { const char *t = ov == 0 ? "GREEN:HOME YELLOW:FARM BLUE:WORK" : ov <= 3 ? "GREEN: GOOD    RED: BAD" : "GREEN: COVERED   RED: NOT";
      u8 gx, gy, g; int tx = 120 - blen2(t) * 7 / 2; for (; *t; t++, tx += 7) { g = gi(*t); for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) vplot(pg, tx + gx, 150 + gy, 6); } }
    { u8 gx, gy, g, k; for (k = 0; k < 2; k++) { g = gi(k ? '>' : '<'); for (gy = 0; gy < 8; gy++) for (gx = 0; gx < 8; gx++) if ((FONT8[g][gy] >> (7 - gx)) & 1) vplot(pg, (k ? 228 : 4) + gx, 3 + gy, 10); } }
    DISPCNT = 0x0404; fade(16, 0);
    ewait_clear();
    for (;;) { ekey(); if (emp_ & (J_LEFT | J_RIGHT | J_L | J_R | J_A | J_B | J_START)) break; }
    if (emp_ & (J_B | J_START)) break;
    ov = (u8)((emp_ & (J_LEFT | J_L)) ? (ov + 8) % 9 : (ov + 1) % 9); sfx_food();
  }
  ewait_clear(); fade(0, 16); DISPCNT = 0x80;
  vram_reload();
}

// ---------- GRAPHS: population and money over the last 4 years ----------
static void graph(u8 row0, u8 which) {
  u8 tx, ty, y, x, i, n = (u8)(nh2 < 2 ? 2 : nh2); s32 mx = 1, mn = 0; u32 w;
  for (i = 0; i < nh2; i++) { s32 v = which ? hcash[i] : hpop[i]; if (v > mx) mx = v; if (v < mn) mn = v; }
  for (ty = 0; ty < 5; ty++) for (tx = 0; tx < 12; tx++) {
    for (y = 0; y < 8; y++) {
      u8 py = (u8)(ty * 8 + y); w = 0;
      for (x = 0; x < 8; x++) {
        u8 px = (u8)(tx * 8 + x), c = (py == 39 || px == 0) ? 11 : ((py & 7) == 0 && (px & 1)) ? 12 : 1;
        u8 k = (u8)((u16)px * (n - 1) / 95), k0 = (u8)((u16)(px ? px - 1 : 0) * (n - 1) / 95);
        if (k < nh2) {
          s32 v1 = which ? hcash[k] : hpop[k], v0 = which ? hcash[k0] : hpop[k0];
          u8 a1 = (u8)(38 - (v1 - mn) * 37 / (mx - mn)), a0 = (u8)(38 - (v0 - mn) * 37 / (mx - mn)), lo = a1 < a0 ? a1 : a0, hi = a1 < a0 ? a0 : a1;
          if (py >= lo && py <= hi) c = which ? (u8)(v1 < 0 ? 7 : 10) : 6;
          if (which && mn < 0 && py == (u8)(38 - (0 - mn) * 37 / (mx - mn)) && c == 1) c = 12;
        }
        w |= (u32)c << (4 * x);
      }
      TILE32[(MAPT + which * 60 + ty * 12 + tx) * 8 + y] = w;
    }
    BGMAP1[(row0 + ty) * 32 + 9 + tx] = (u16)((MAPT + which * 60 + ty * 12 + tx) | (6 << 12));
  }
}
static void city_graphs(void) {
  ewait_clear();
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "GRAPHS");
  mv_pal();
  ps(1, 3, "ANTS", 1); pnum(1, 4, cpop, 9); graph(3, 0);
  ps(1, 10, "MONEY", 9); pcash(1, 11, cash, 9); graph(10, 1);
  ps(1, 16, "LAST 4 YEARS", 10);
  ps(1, 17, "H", dR > 0 ? 13 : 8); pn(2, 17, (u8)(dR < 0 ? -dR : dR), 3); ps(6, 17, "F", dF > 0 ? 13 : 8); pn(7, 17, (u8)(dF < 0 ? -dF : dF), 3); ps(11, 17, "W", dW > 0 ? 13 : 8); pn(12, 17, (u8)(dW < 0 ? -dW : dW), 3);
  ps(16, 17, "DEMAND", 10);
  ps(1, 19, "\2 BACK", 10);
  for (;;) { ekey(); if (emp_ & (J_A | J_B | J_START)) break; }
  ewait_clear();
}
// ---------- ADVISORS: five ants, each with one thing to say ----------
static void city_advice(void) {
  u8 y = 3; u16 cvn = 0, cvt = 0, cl = 0, scl = 0; u8 a, b;
  for (b = 0; b < (H + 3) / 4; b++) for (a = 0; a < (W + 3) / 4; a++) { cvt++; if (ccov[b][a] & CV_AIR) cvn++; if (ccov[b][a] & CV_CLINIC) cl++; if (ccov[b][a] & CV_SCHOOL) scl++; }
  ewait_clear();
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "ADVISORS");
  { s16 net = (s16)(inc_r + inc_f + inc_w + inc_x - exp_s - exp_t - exp_l - exp_n);
    ps(1, y, "MONEY", 9); ps(9, y++, cash < 0 ? "WE ARE IN DEBT!" : net < 0 ? "LOSING MONEY!" : cash > 5000 ? "RICH! BUILD BIG" : "MONEY IS FINE", cash < 0 || net < 0 ? 8 : 13);
    if (net < 0) ps(9, y++, "RAISE TAX, CUT LAWS", 3); y++; }
  ps(1, y, "TOWN", 9);
  ps(9, y++, chunger ? "HUNGRY! ZONE FARMS" : dR > 40 ? "ZONE MORE HOMES" : dW > 40 ? "ZONE MORE WORKS" : dF > 40 ? "WE NEED FARM ZONES" :
    cnoacc > 4 ? "ZONES NEED TUNNELS" : "THE ZONES LOOK GOOD", chunger ? 8 : 3); y++;
  ps(1, y, "AIR", 9); ps(9, y++, cvt && cvn * 100 / cvt < 50 ? "MORE AIR SHAFTS" : "THE AIR IS FRESH", cvt && cvn * 100 / cvt < 50 ? 8 : 13); y++;
  ps(1, y, "SAFETY", 9); ps(9, y++, ccrime > 40 ? "CRIME! GUARD POSTS" : cfires_n ? "FIRE! DEW PUMPS" : "STREETS ARE SAFE", ccrime > 40 || cfires_n ? 8 : 13); y++;
  ps(1, y, "HEALTH", 9); ps(9, y++, csmog > 40 ? "SMOG! PARKS, LAWS" : cvt && cl * 100 / cvt < 30 ? "BUILD CLINICS" : cvt && scl * 100 / cvt < 30 ? "BUILD SCHOOLS" : "ANTS ARE HEALTHY", csmog > 40 ? 8 : 3); y++;
  ps(1, y, "NEXT", 9);
  if (cmile < 5) { ps(9, y, MILEN[cmile + 1], 13); ps(9 + blen2(MILEN[cmile + 1]) + 1, y, "AT", 10); pnum((u8)(9 + blen2(MILEN[cmile + 1]) + 4), y, MILE[cmile + 1], 3); } else ps(9, y, "YOU DID IT ALL!", 13);
  ps(1, 19, "\2 BACK", 10);
  for (;;) { ekey(); if (emp_ & (J_A | J_B | J_START)) break; }
  ewait_clear();
}
static void city_disasters(void) {
  u8 sel = 0, i;
  static const char *const DN[5] = {"FIRE", "FLOOD", "CAVE IN", "GIANT BEETLE", "RANDOM DISASTERS"};
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "DISASTERS");
    ps(1, 2, "START ONE... IF YOU DARE", 10);
    for (i = 0; i < 5; i++) { u8 y = (u8)(4 + i * 2);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, DN[i], i == sel ? 9 : 3); if (i == 4) ps(21, y, cdis_on ? "ON" : "OFF", cdis_on ? 8 : 13); }
    ps(1, 16, "DEW PUMPS STOP FIRES. GUARDS", 3); ps(1, 17, "FIGHT THE BEETLE.", 3);
    ps(1, 19, "\1 DO IT   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 4);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 4 ? 0 : sel + 1);
    if (emp_ & J_A) { if (sel == 4) { cdis_on ^= 1; sfx_food(); continue; }
      if (sel == 0) fire_any(); else if (sel == 1) flood_any(); else if (sel == 2) cavein_any(); else beetle_any(); break; }
  }
  ewait_clear();
}

// ---------- SCENARIOS ----------
enum { CG_POP, CG_FOOD, CG_SMOG, CG_CRIME, CG_CASH, CG_ARCO };
typedef struct { const char *name; u8 map, w, h; s16 cash; u8 years, goal; u16 v1; u8 v2, starter, dis; const char *s[4]; } Scen;
#define NSCEN 10
static const Scen SC[NSCEN] = {
  {"FIRST TUNNELS", 0, 40, 32, 4000, 6, CG_POP, 300, 0, 0, 0, {"MAYOR! THE MEADOW IS EMPTY.", "DIG TUNNELS, THEN ZONES", "NEXT TO THEM. ANTS MOVE IN", "BY THEMSELVES. GOAL: 300"}},
  {"BREAD BASKET", 1, 48, 40, 3500, 8, CG_FOOD, 600, 250, 0, 0, {"THE RIVER LAND IS RICH.", "FEED THE HILLS AROUND US.", "GOAL: 600 ANTS AND 250 FOOD", "IN THE STORES. ZONE FARMS!"}},
  {"SMOG VALLEY", 3, 48, 48, 3000, 8, CG_SMOG, 900, 20, 1, 0, {"THIS TOWN IS ALL FACTORIES.", "THE AIR IS BROWN. CLEAN IT!", "GOAL: 900 ANTS AND SMOG", "UNDER 20. PARKS + LAWS HELP"}},
  {"CRIME WAVE", 0, 48, 48, 3000, 8, CG_CRIME, 1000, 20, 2, 0, {"PACKED HOMES, NO GUARDS.", "THIEVES RUN THE TUNNELS.", "GOAL: 1000 ANTS AND CRIME", "UNDER 20. GUARD POSTS HELP"}},
  {"FLOOD PLAIN", 2, 56, 48, 4000, 10, CG_POP, 1500, 0, 0, 2, {"THE LAKE RISES EVERY", "OTHER YEAR. BUILD ON HIGH", "GROUND OR PILE UP LAND.", "GOAL: 1500 ANTS"}},
  {"DEBT TRAP", 0, 48, 48, -1500, 10, CG_CASH, 3000, 200, 3, 0, {"THE OLD MAYOR SPENT IT ALL.", "WE OWE 1500. GROW AND SAVE:", "2000 ANTS AND *3000 IN", "THE BANK. GOAL: 2000 + *3000"}},
  {"FUNGUS FIRE", 1, 56, 56, 3500, 10, CG_POP, 2000, 0, 4, 1, {"THE FARMS ARE ON FIRE!", "DEW PUMPS PUT FIRES OUT.", "REBUILD THE TOWN BIGGER.", "GOAL: 2000 ANTS"}},
  {"TWIN ISLES", 5, 32, 32, 5000, 10, CG_POP, 1200, 0, 0, 0, {"TWO ISLANDS, ONE BRIDGE.", "LITTLE ROOM: GROW UP, NOT", "OUT. LEVEL 3 NEEDS HELP.", "GOAL: 1200 ANTS"}},
  {"BEETLE SEASON", 3, 64, 64, 5000, 12, CG_POP, 3000, 0, 0, 4, {"GIANT BEETLES WALK HERE", "EVERY YEAR. GUARD POSTS", "BITE THEM. KEEP GROWING!", "GOAL: 3000 ANTS"}},
  {"MEGALOPOLIS", 7, 96, 96, 8000, 25, CG_ARCO, 8000, 0, 0, 0, {"THE BIGGEST LAND OF ALL.", "BECOME A METROPOLIS, THEN", "BUILD THE SKY MOUND.", "GOAL: 8000 ANTS + SKY MOUND"}},
};
static u16 scen_done(void) { u16 v = (u16)(SRAM[22] | (SRAM[23] << 8)); return v == 0xFFFF ? 0 : (u16)(v & 0x3FF); }
static u8 scen_count(void) { u8 i, n = 0; u16 v = scen_done(); for (i = 0; i < NSCEN; i++) n += (v >> i) & 1; return n; }
static u8 verse_unlocked(void) { return scen_count() >= NSCEN; }
static void scen_mark(u8 i) { u16 v = (u16)(scen_done() | (1 << i)); SRAM[22] = (u8)v; SRAM[23] = (u8)(v >> 8); }
static u8 city_goal_ok(void) {
  const Scen *s;
  if (cscen == 255) return 0;
  s = &SC[cscen];
  switch (s->goal) {
  case CG_POP: return cpop >= s->v1;
  case CG_FOOD: return cpop >= s->v1 && rf >= s->v2;
  case CG_SMOG: return cpop >= s->v1 && csmog < s->v2;
  case CG_CRIME: return cpop >= s->v1 && ccrime < s->v2;
  case CG_CASH: return cash >= (s32)s->v1 && cpop >= (u32)s->v2 * 10;
  default: return cpop >= s->v1 && nb_of[B_ARCO];
  }
}
static void starter_town(u8 kind) {               // a grid of tunnels and grown zones around the nest
  u8 x0, y0, x1, y1, x, y;
  x0 = (u8)(nestx[0] > 2 ? nestx[0] - 2 : 0); y1 = (u8)(nesty[0] + 2 < H ? nesty[0] + 2 : H - 1);
  x1 = (u8)(x0 + 20 < W ? x0 + 20 : W - 1); y0 = (u8)(y1 > 14 ? y1 - 14 : 0);
  for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) {
    if (!hgt[y][x] || ebi[y][x] || (dist(x, nestx[0]) <= 1 && dist(y, nesty[0]) <= 1)) continue;
    if ((y - y0) % 4 == 0 || (x - x0) % 5 == 0) { set_tun(y, x, 1); continue; }
    { u8 r = (u8)(rnd() % 10), z, l;
      if (kind == 1) { z = r < 6 ? 3 : r < 9 ? 1 : 2; l = (u8)(z == 3 ? 2 + (r & 1) : 1); }
      else if (kind == 2) { z = r < 7 ? 1 : r < 9 ? 3 : 2; l = (u8)(z == 1 ? 2 : 1); }
      else { z = r < 5 ? 1 : r < 8 ? 3 : 2; l = (u8)(1 + (r & 1)); }
      set_zn(y, x, z); set_lv(y, x, l); }
  }
  if (kind == 2 || kind == 3 || kind == 4) { add_b((u8)(x0 + 5 < W ? x0 + 5 : x0), (u8)(y0 + 2), B_AIR); add_b((u8)(x0 + 15 < W ? x0 + 15 : x0), (u8)(y0 + 6), B_AIR); }
  if (kind == 1) add_b((u8)(x0 + 10 < W ? x0 + 10 : x0), (u8)(y0 + 6), B_AIR);
}

// ---------- start / save / load ----------
static void city_setup_common(void) {
  u8 i;
  cash = cmoney == 0 ? 8000 : cmoney == 1 ? 4000 : cmoney == 2 ? 2000 : 99999;
  cpop = 0; cjw = cjf = 0; cmon = 0; cmile = 0; chunger = 0; claws = 0; cloan = 0; cbroke = 0; cnews = 0;
  taxR = taxF = taxW = 9; dR = 40; dF = 20; dW = 30; nh2 = 0; cfires_n = 0; fltime = 0; bt_on = 0;
  for (i = 0; i < NFIRE; i++) ftime[i] = 0;
  inc_r = inc_f = inc_w = inc_x = exp_s = exp_t = exp_l = exp_n = 0;
  mana = 10; scan_reset();
}
static void city_terr(void) { u8 x, y; if (ecity == 1) for (y = 0; y < H; y++) for (x = 0; x < W; x++) set_ter(y, x, 1); }
static void city_newgame(void) {
  u8 sm = mapt, sw = tw, sh = th, x;
  if (cscen != 255) { mapt = SC[cscen].map; tw = SC[cscen].w; th = SC[cscen].h; }
  ld_begin(ecity == 2 ? "MAKING THE ANTVERSE..." : "DIGGING THE CITY...", cscen != 255 ? SC[cscen].name : mname(mapt));
  gen_world();
  mapt = sm; tw = sw; th = sh;
  emp_setup_common();
  echap = 255;
  if (ecity == 2) { etech = (1 << T_FUNGUS) | (1 << T_DIG) | (1 << T_SOLD); rf = 80; rd = 80; ra = 10; gw = 30; gs = gm = 0; }
  else { enriv = 0; etech = 0xFFF; rf = 100; rd = 0; ra = 0; gw = gs = gm = 0; }
  if (cht[4]) etech = 0xFFF;
  eres = 255; erp = 0; pop = 6; popcap = 10; edry = 0; era = era_now();
  city_setup_common(); pz_setup(cscen);
  if (cscen != 255) { cash = SC[cscen].cash; cdis_on = 0; cyears_lim = (u16)(SC[cscen].years * 12); }
  if (cht[3]) cash = RICHV[cht[3]] * 20;
  if (ecity == 2) place_rivals();
  bfs(0); bfs(1); pdirty = 0;
  add_b(nestx[0], nesty[0], B_NEST);
  if (ecity == 2) { u8 i; for (i = 0; i < enriv; i++) path(nestx[0], nesty[0], rx[i], ry[i]); }
  for (x = 1; x <= 4; x++) if (nestx[0] + x < W && hgt[nesty[0]][nestx[0] + x] && !ebi[nesty[0]][nestx[0] + x]) set_tun(nesty[0], (u8)(nestx[0] + x), 1);   // a first tunnel
  if (cscen != 255 && SC[cscen].starter) starter_town(SC[cscen].starter == 4 ? 3 : SC[cscen].starter);
  bfs(0); pdirty = 0; efield(nestx[0], nesty[0]);
  city_terr(); terr_calc(); city_terr();
  { u8 r; for (r = 0; r < H; r++) scan_rows(1); }
  cquiet = 1; city_cycle(); cquiet = 0; cmon = 0; nh2 = 0;
  if (cscen != 255 && SC[cscen].starter == 4) { u8 k; for (k = 0; k < 4; k++) fire_any(); }
  ld_end();
  msgt = 0;
}
#define CSO 20480
static void city_save(void) {
  u16 o = CSO + 4; u8 x, y, i;
  if (ecity == 2) emp_save();
  sb(&o, ecity); sb(&o, mw); sb(&o, mh); sb(&o, nestx[0]); sb(&o, nesty[0]); sb(&o, cmoney); sb(&o, cdis_on);
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) sb(&o, (u8)(hgt[y][x] | (EZN(y, x) << 2) | (ELV(y, x) << 4) | (ETUN(y, x) << 6)));
  if (ecity == 1) { sb(&o, nbl); for (i = 0; i < nbl; i++) { sb(&o, ebl[i].x); sb(&o, ebl[i].y); sb(&o, ebl[i].t); sb(&o, ebl[i].hp); } }
  sw16(&o, (u16)(cash & 0xFFFF)); sw16(&o, (u16)((u32)cash >> 16)); sw16(&o, cmon); sb(&o, cmile); sb(&o, taxR); sb(&o, taxF); sb(&o, taxW); sw16(&o, claws); sb(&o, cloan); sw16(&o, rf);
  { const u8 *q = (const u8 *)&PZ_; for (i = 0; i < sizeof PZ_; i++) sb(&o, q[i]); }
  SRAM[CSO] = 'C'; SRAM[CSO + 1] = 'S'; SRAM[CSO + 2] = 'A'; SRAM[CSO + 3] = 'V';
  say("SAVED");
}
static u8 city_has_save(u8 verse) { return SRAM[CSO] == 'C' && SRAM[CSO + 1] == 'S' && SRAM[CSO + 2] == 'A' && SRAM[CSO + 3] == 'V' && SRAM[CSO + 4] == (verse ? 2 : 1) && (!verse || emp_has_save()); }
static void city_load(void) {
  u16 o = CSO + 4; u8 x, y, i, n, v; u16 lo, hi;
  ecity = lb(&o);
  if (ecity == 2) emp_load();
  mw = lb(&o); mh = lb(&o); nestx[0] = lb(&o); nesty[0] = lb(&o); cmoney = lb(&o); cdis_on = lb(&o);
  if (ecity == 1) { emp_setup_common(); echap = 255; enriv = 0; etech = 0xFFF; gw = gs = gm = 0; pop = 6; eres = 255; }
  city_setup_common();
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { v = lb(&o); hgt[y][x] = v & 3; set_zn(y, x, (u8)((v >> 2) & 3)); set_lv(y, x, (u8)((v >> 4) & 3)); set_tun(y, x, (u8)((v >> 6) & 1)); }
  if (ecity == 1) { n = lb(&o); nbl = 0; for (i = 0; i < n; i++) { u8 bx = lb(&o), by = lb(&o), bt = lb(&o), bh = lb(&o); if (!ebi[by][bx]) { add_b(bx, by, bt); ebl[nbl - 1].hp = bh; } } }
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) if (ebi[y][x]) { set_zn(y, x, 0); set_lv(y, x, 0); set_tun(y, x, 0); }
  lo = lw16(&o); hi = lw16(&o); cash = (s32)((u32)lo | ((u32)hi << 16));
  cmon = lw16(&o); cmile = lb(&o); taxR = lb(&o); taxF = lb(&o); taxW = lb(&o); claws = lw16(&o); cloan = lb(&o); rf = lw16(&o);
  { u8 *q = (u8 *)&PZ_; for (i = 0; i < sizeof PZ_; i++) q[i] = lb(&o); }
  cscen = 255; echap = 255;
  bfs(0); bfs(1); pdirty = 0; efield(nestx[0], nesty[0]);
  city_terr(); terr_calc(); city_terr();
  { u8 r; for (r = 0; r < H; r++) scan_rows(1); }
  { u16 m = cmon; u8 ml = cmile; cmile = 0; cquiet = 1; city_cycle(); cquiet = 0; cmon = m; if (cmile < ml) cmile = ml; }
}

// ---------- the city menu ----------
static u8 grid_menu(const char *title, const char *const *it, const char *const *ih, u8 n, u8 *selp) {   // returns the pick, 255 = back
  u8 i, x, y, x0, y0, w, pal, fr = 0, sel = *selp, rows = (u8)((n + 2) / 3); u16 t;
  hide_all(); vsync(); oam_flush(); ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, title);
    for (i = 0; i < 16; i++) BGPAL[96 + i] = BGPAL[i];
    pcash(1, 2, cash, 9); pc(11, 2, '{', 1); pnum(12, 2, cpop, 1); ps(19, 2, MONTHS[cmon % 12], 3); pc(23, 2, 'Y', 10); pnum(24, 2, (u32)(cmon / 12 + 1), 3);
    for (i = 0; i < n; i++) {
      x0 = (u8)(1 + (i % 3) * 9); w = (u8)(i % 3 == 2 ? 10 : 9); y0 = (u8)(3 + (i / 3) * 3); pal = i == sel ? 6 : 0;
      for (y = y0; y < y0 + 3; y++) for (x = x0; x < x0 + w; x++) {
        if (y == y0) t = x == x0 ? UIB + 4 : x == x0 + w - 1 ? UIB + 6 : UIB + 5;
        else if (y == y0 + 2) t = x == x0 ? UIB + 9 : x == x0 + w - 1 ? UIB + 11 : UIB + 10;
        else t = x == x0 ? UIB + 7 : x == x0 + w - 1 ? UIB + 8 : UIB + 31;
        BGMAP3[y * 32 + x] = (u16)(t | (pal << 12)); BGMAP1[y * 32 + x] = 0;
      }
      ps((u8)(x0 + (w - blen2(it[i])) / 2), (u8)(y0 + 1), it[i], i == sel ? 9 : 3);
    }
    if (rows < 5) { if (cscen != 255) { ps(1, (u8)(3 + rows * 3), SC[cscen].name, 9); } }
    ps(1, 18, ih[sel], 2);
    ps(1, 19, "\1 OK   \5 BACK", 10);
    DISPCNT = 0x1F40;
    while (!ekey()) hub_glow(fr++);
    if (emp_ & (J_B | J_START)) { *selp = sel; ewait_clear(); return 255; }
    if (emp_ & J_UP)    sel = (u8)(sel >= 3 ? sel - 3 : (sel + (rows - 1) * 3 < n ? sel + (rows - 1) * 3 : sel + (rows - 2) * 3));
    else if (emp_ & J_DOWN)  sel = (u8)(sel + 3 < n ? sel + 3 : sel % 3);
    else if (emp_ & J_LEFT)  sel = (u8)(sel % 3 ? sel - 1 : (sel + 2 < n ? sel + 2 : n - 1));
    else if (emp_ & J_RIGHT) sel = (u8)(sel % 3 == 2 || sel + 1 >= n ? sel - sel % 3 : sel + 1);
    if (emp_ & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) sfx_food();
    if (emp_ & J_A) { sfx_mana(); *selp = sel; return sel; }
  }
}
#include "pz.c"
static void city_goal_card(void) {
  const char *L[6] = {0, 0, 0, 0, 0, 0};
  if (cscen != 255) { u8 i; for (i = 0; i < 4; i++) L[i] = SC[cscen].s[i]; story_card(SC[cscen].name, L, "\1 OK"); }
  else if (ecity == 2) { static const char *const V[6] = {"EVERYTHING AT ONCE!", "GROW A CITY, RULE AN EMPIRE,", "BEAT THE RIVALS, USE YOUR", "GOD POWERS. WIN: BEAT ALL", "RIVALS, OR BUILD SKY MOUND", 0};
    static const char *const V2[6] = {"EVERYTHING AT ONCE!", "CITY EMPIRE WAR GOD POWERS", "AND THE PALACE: KEEP THE 8", "GROUPS HAPPY OR BE VOTED", "OUT. WIN: RIVALS OR SKY MOUND", 0}; story_card("ANTVERSE", PZ_.on ? V2 : V, "\1 OK"); }
  else { static const char *const F[6] = {"NO GOAL. NO LIMITS.", "GROW FROM OUTPOST TO", "MEGALOPOLIS: 15000 ANTS.", "HAVE FUN, MAYOR!", 0, 0}; story_card("FREE CITY", F, "\1 OK"); }
}
static u8 city_menu(void) {                        // 0 resume, 1 quit
  static const char *const IT[16] = {"RESUME", "BUILD", "BUDGET", "MAPS", "LAWS", "ADVISORS", "GRAPHS", "CHAOS", "GOAL", "SPEED", "SAVE", "QUIT",
    "SCIENCE", "RIVALS", "EDICTS", "PALACE"};
  static const char *const IH[16] = {"BACK TO THE CITY", "ZONES TUNNELS AND BUILDINGS", "TAXES AND MONEY", "SEE LAND VALUE SMOG CRIME",
    "CITY RULES THAT HELP", "WHAT THE CITY NEEDS", "ANTS AND MONEY OVER TIME", "FIRE FLOOD BEETLE...", "WHAT YOU MUST DO", "L SLOWER  A FASTER",
    "SAVE AND GO TO THE MENU", "LEAVE THIS CITY", "LEARN NEW TECHS", "TALK TO OTHER COLONIES", "PASS LAWS FOR YOUR ANTS", "GROUPS VOTES EDICTS SPIES"};
  const char *it[16], *ih[16]; u8 map[16];
  u8 sel = cm_sel, i;
  u8 n = 0, r;
  for (i = 0; i < 12; i++) { it[n] = IT[i]; ih[n] = IH[i]; map[n++] = i; }
  if (ecity == 2) { it[n] = IT[12]; ih[n] = IH[12]; map[n++] = 12; it[n] = IT[13]; ih[n] = IH[13]; map[n++] = 13; if (!PZ_.on) { it[n] = IT[14]; ih[n] = IH[14]; map[n++] = 14; } }
  if (PZ_.on) { it[n] = IT[15]; ih[n] = IH[15]; map[n++] = 15; }   // EL PRESIDANTE: the palace (in ANTVERSE it holds the edicts too)
  for (;;) {
    if (sel >= n) sel = 0;
    r = grid_menu(ecity == 2 ? "ANTVERSE" : "ANT CITY", it, ih, n, &sel);
    if (r == 255 || r == 0) break;
    r = map[r];
    switch (r) {
    case 1: { u8 b = city_build_menu(); if (b) { eplace = b; ewait_clear(); ov_clear(); cm_sel = sel; return 0; } break; }
    case 2: city_budget(); break;
    case 3: city_data(); break;
    case 4: city_laws(); break;
    case 5: city_advice(); break;
    case 6: city_graphs(); break;
    case 7: city_disasters(); ewait_clear(); ov_clear(); cm_sel = sel; return 0;
    case 8: city_goal_card(); break;
    case 9: spd_next(1); break;
    case 10: if (cscen == 255) { city_save(); ewait_clear(); ov_clear(); cm_sel = sel; return 1; } sfx_deny(); break;
    case 11: { static const char *const Y[2] = {"YES, QUIT", "NO"}; static const char *const YH[2] = {"LEAVE WITHOUT SAVING", "KEEP PLAYING"}; u8 s2 = 1;
      if (grid_menu("QUIT?", Y, YH, 2, &s2) == 0) { ewait_clear(); ov_clear(); cm_sel = sel; return 1; } break; }
    case 12: research_menu(); break;
    case 13: rivals_screen(); break;
    case 14: edict_menu(); break;
    case 15: pz_palace(); if (PZ_.pend) { ewait_clear(); ov_clear(); cm_sel = sel; return 0; } break;
    }
  }
  cm_sel = sel; ewait_clear(); ov_clear();
  return 0;
}

// ---------- play ----------
static void city_show(void) {
  cx = nestx[0]; cy = (u8)(nesty[0] > 2 ? nesty[0] - 2 : nesty[0]);
  camx = 0; camy = (u8)(H - VH); follow(); scx = (s16)camx * 8; scy = (s16)camy * 8;
  win_all(); ov_clear(); chud();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; BG2CNT = 1 | (2 << 2) | (30 << 8);
  hide_all(); edraw_sprites(); oam_flush();
  DISPCNT = 0x1F40; BLDY = 16; music_start(); fade(16, 0);
}
static void city_resume(void) {                    // back from a menu: keep the camera where it was
  win_all(); ov_clear(); chud(); mp_tag();
  BG0HOFS = BG2HOFS = (u16)scx; BG0VOFS = BG2VOFS = (u16)scy; BG2CNT = 1 | (2 << 2) | (30 << 8);
  hide_all(); edraw_sprites(); mp_cursor2(); oam_flush(); DISPCNT = 0x1F40; BLDY = 0;
  if (!mus_on && !nomus) music_start();
}
static void milestone_card(u8 m) {
  static const char *const MC[6][3] = {{"", "", ""}, {"A REAL VILLAGE!", "+500 MONEY", "KEEP ZONING!"}, {"WE ARE A TOWN!", "+1000 MONEY. LEVEL 3 ZONES", "AND THE STADIUM ARE OPEN"},
    {"A BUSY CITY!", "+1500 MONEY", "THE ANTS LOVE YOU, MAYOR"}, {"A METROPOLIS!", "+2000 MONEY. THE SKY MOUND", "CAN NOW BE BUILT"}, {"MEGALOPOLIS!", "+2500 MONEY", "THE GREATEST ANT CITY EVER"}};
  const char *L[6] = {MC[m][0], MC[m][1], MC[m][2], 0, 0, 0};
  music_stop(); fade(0, 16); DISPCNT = 0x80;
  story_card(MILEN[m], L, "\1 HOORAY");
}
static u8 city_input(u8 p, u16 k) {                // one player's buttons this frame: 0 go on, 1 quit, 2 a menu was used
  PSt *s = &pst[p]; u16 pr = k & ~s->prev; u8 dirs, fire, mx = cx, my = cy;
  s->prev = k;
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
  if (k & J_SEL) { if (pr & J_SEL) s->selused = 0; if (k & ~J_SEL) s->selused = 1; }
  else if (s->selprev && !s->selused) {
    if (alt) { cx = alx; cy = aly; alt = 0; }
    else if (bt_on) { cx = bt_x; cy = bt_y; say("THE BEETLE"); }
    else { u8 i; s->jj = (u8)((s->jj + 1) % 3);
      if (s->jj == 0) { cx = nestx[0]; cy = nesty[0]; say("THE NEST"); }
      else if (s->jj == 1) { for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team) { cx = ea[i].x; cy = ea[i].y; say("YOUR ARMY"); break; } if (i == EAN) s->jj = 2; }
      if (s->jj == 2) { for (i = 0; i < enriv; i++) if (ralive[i]) { cx = rx[i]; cy = ry[i]; say("RIVAL NEST"); break; } if (!enriv) { cx = nestx[0]; cy = nesty[0]; } } }
    sfx_spawn();
  }
  s->selprev = (u8)(k & J_SEL);
  if (pr & J_START) {
    u8 q;
    if (eplace || etgt) { eplace = 0; etgt = 0; return 0; }
    lk_focus = p; q = city_menu(); lk_focus = 2; s->prev = 0x3FF;
    if (q) return 1;
    city_resume(); return 2;
  }
  if (etgt) {
    if (pr & J_A) { EA *a = &ea[etgt_a]; u8 j; efield(cx, cy); if (a->alive) a->ord = 1; for (j = 0; j < EAN; j++) if (ea[j].alive && !ea[j].team && ea[j].ord == 3) ea[j].ord = 1; etgt = 0; say("MARCHING!"); sfx_alarm(); }
    if (pr & J_B) etgt = 0;
  } else if (eplace) {
    u8 drag = eplace == L_TUN || eplace == L_DOZE || (eplace >= L_ZH && eplace <= L_ZX);
    if ((pr & J_A) || ((k & J_A) && (mx != cx || my != cy) && drag)) { if (city_do(eplace, cx, cy)) s->built++; }
    if (pr & J_B) eplace = 0;
  } else {
    if (pr & J_A) {
      u8 i, on = ebi[cy][cx] != 0;
      for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && ea[i].x == cx && ea[i].y == cy) on = 1;
      lk_focus = p;
      if (on) { panel_open(); lk_focus = 2; s->prev = 0x3FF; city_resume(); return 2; }
      if (hgt[cy][cx] && (ecity == 1 || ETER(cy, cx) == 1)) { u8 b = city_build_menu(); lk_focus = 2; if (b) eplace = b; s->prev = 0x3FF; city_resume(); return 2; }
      lk_focus = 2;
      say(hgt[cy][cx] ? "NOT YOUR LAND" : "WATER: NOTHING TO BUILD"); sfx_deny();
    }
    if (pr & J_B) { lk_focus = p; if (city_info()) { u8 b = city_build_menu(); if (b) eplace = b; } lk_focus = 2; s->prev = 0x3FF; city_resume(); return 2; }
  }
  if (pr & J_L) {
    lk_focus = p;
    if (ecity == 2) {                              // zoom into a fight
      u8 i, bi = 255, bd = 255;
      for (i = 0; i < EAN; i++) if (ea[i].alive && !ea[i].team && ea[i].foe != 255) { u8 dd = (u8)(dist(ea[i].x, cx) + dist(ea[i].y, cy)); if (dd < bd) { bd = dd; bi = i; } }
      if (bi != 255 && ea[ea[bi].foe].alive) { EA *e = &ea[ea[bi].foe]; zoom_battle(&ea[bi], &e->w, &e->s, &e->m, (u8)(e->team - 1), e->x, e->y);
        if (!a_size(e)) e->alive = 0; if (!a_size(&ea[bi])) ea[bi].alive = 0; ea[bi].foe = 255; e->foe = 255; lk_focus = 2; s->prev = 0x3FF; city_resume(); return 2; }
      for (i = 0; i < EAN; i++) if (ea[i].alive && ea[i].team && dnp[0][ea[i].y][ea[i].x] <= 1 && (gw + gs + gm)) {
        EA *e = &ea[i]; zoom_battle(0, &e->w, &e->s, &e->m, (u8)(e->team - 1), nestx[0], nesty[0]); if (!a_size(e)) e->alive = 0; lk_focus = 2; s->prev = 0x3FF; city_resume(); return 2; }
    }
    city_data(); lk_focus = 2; s->prev = 0x3FF; city_resume(); return 2;   // L: the data maps
  }
  if (pr & J_R) spd_toggle();
  return 0;
}
static void city_play(void) {
  u16 k; u8 t = 0, n, i, r = 0, lx, ly;
  if (mp_on == 1) { mp_turn = 0; mp_tleft = (u16)((mp_tlen + 1) * 30 * 60); mp_pass_card(0); }
  city_show();
  ps_init(cx, cy, (u8)(cx + 2), cy);
  say(cscen != 255 ? SC[cscen].name : mp_on ? "2 MAYORS! BUILD TOGETHER" : "\1 BUILD  \2 INFO  \5 MENU");
  mp_tag(); mp_cursor2();
  while (!eend) {
    vsync(); frc++;
    shx = (shk && !O(O_SHAKE)) ? ((shk & 2) ? 2 : -2) : 0; if (shk) shk--;
    BG0HOFS = BG2HOFS = (u16)(scx + shx); BG0VOFS = BG2VOFS = (u16)scy; oam_flush();
    BGPAL[176 + 3] = (frc & 32) ? RGB(14,22,31) : RGB(10,18,30); BGPAL[176 + 14] = (frc & 32) ? RGB(10,18,30) : RGB(14,22,31);
    BGPAL[192 + 5] = (frc & 8) ? RGB(31,5,8) : RGB(31,18,4);                       // fires flicker
    OBJPAL[32 + 3] = RGB(31, 26 + ((frc >> 2) & 3), (frc & 16) ? 4 : 12);
    party_tick();
    if (alt) alt--;
    lk_focus = 2;
    if (cnews) { u8 m = (u8)(cnews - 1); cnews = 0; milestone_card(m); city_resume(); continue; }
    if (PZ_.pend) { if (pz_cards()) { if (eend) break; city_resume(); continue; } }
    if (evpend != 255 && ecity == 2) { emp_event(evpend); evpend = 255; city_resume(); continue; }
    if (cscen != 255 && cmon) {
      if (city_goal_ok()) { city_end(1, 22); break; }
      if (cmon >= cyears_lim) { city_end(0, 21); break; }
    }
    mp_hash();
    k = joy();
    if (lk_err) { eend = 3; break; }
    lx = cx; ly = cy;
    for (i = 0; i < (mp_on == 2 ? 2 : 1); i++) {
      u8 p = mp_on == 2 ? i : mp_on == 1 ? mp_turn : 0;
      ps_load(p); r = city_input(p, mp_on == 2 ? lk_k[p] : k); ps_save(p);
      if (r) break;
    }
    if (r == 1) { eend = 3; break; }
    ps_load(mp_local());
    if (r == 2) continue;
    if (mp_turn_tick()) { city_resume(); continue; }
    follow(); scroll_step(); win_sync();
    bfs_tick(450);
    n = SPN[spd]; if (n && (sf++ & SPM[spd])) n = 0;
    for (; n; n--) eslice_run();
    if (++t >= 8 || (mp_on == 2 ? lk_k[lk_me] : k) || lx != cx || ly != cy) { t = 0; chud(); mp_tag(); }
    edraw_sprites(); mp_cursor2();
    if (bt_on) place16(MAXA + 1, 16, 1, bt_ox * 8 + (bt_x - bt_ox) * bt_age, bt_oy * 8 + (bt_y - bt_oy) * bt_age, bt_fl);
    else spr_hide(MAXA + 1);
  }
  music_stop();
}
static void city_end_screen(void) {
  static const char *const WL[4] = {"THE CITY WENT BANKRUPT", "TIME RAN OUT", "", "THE COLONY ROSE AGAINST YOU"};
  if (eend == 3) { emode = 0; ecity = 0; fade(0, 16); return; }
  hide_all(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full();
  if (eend == 1) {
    big(2, ecity == 2 ? "ANTVERSE WON!" : "SUCCESS!");
    if (cscen != 255) { u8 had = verse_unlocked(); ps(3, 5, SC[cscen].name, 9); ps(3, 6, "COMPLETE!", 13); scen_mark(cscen); ach_get(36); if (verse_unlocked()) ach_get(43); if (!had && verse_unlocked()) { ps(3, 15, "ANTVERSE IS UNLOCKED!", 13); ps(3, 16, "EVERY MODE AT ONCE...", 3); } }
    if (ecity == 2) ach_get(44);
    else ps(3, 5, ewhy == 5 ? "THE SKY MOUND RISES!" : ewhy == 2 ? "THE GREAT MOUND IS COMPLETE" : ewhy == 3 ? "ALL RIVALS ARE BEATEN" : "A GREAT CITY", 3);
    jingle(WIN_TUNE, 6);
  } else { big(2, "GAME OVER"); ps(3, 5, ewhy == 20 ? WL[0] : ewhy == 21 ? WL[1] : ewhy == 1 ? WL[3] : ewhy == 30 ? "YOU WERE THROWN OUT OF THE PALACE" : "YOUR NEST WAS DESTROYED", 3); jingle(LOSE_TUNE, 4); }
  pm_rule(7);
  ps(3, 9, "ANTS", 10); pnum(14, 9, cpop, 3);
  ps(3, 10, "MONEY", 10); pcash(14, 10, cash, 3);
  ps(3, 11, "SIZE", 10); ps(14, 11, MILEN[cmile], 3);
  ps(3, 12, "YEARS", 10); pnum(14, 12, (u32)(cmon / 12), 3);
  ps(3, 13, "SMOG  CRIME", 10); pn(14, 13, csmog, 3); pn(18, 13, ccrime, 3);
  pm_rule(17); ps(7, 18, "PRESS \5 START", 9);
  ewait_clear(); while (!(joy() & (J_START | J_A))) vsync(); ewait_clear();
  emode = 0; ecity = 0; fade(0, 16);
}

#include "missions.c"
static const u16 *mis_lim_p(void) { return &MIS[mis_i].lim; }
// ---------- CITY / ANTVERSE main menus ----------
static u8 city_setup(u8 verse) {                   // FREE CITY / ANTVERSE settings with a live map preview. 1 = start
  static u8 sel = 0, nriv = 2, agg = 1;
  static const char *const MN[4] = {"EASY", "NORMAL", "HARD", "SANDBOX"};
  static const char *const AGN[3] = {"CALM", "NORM", "WILD"};
  u8 nrow = verse ? 8 : 7, i; u16 fr = 0, k, p, prev = joy(); u8 dirty = 0;
  hide_all(); oam_flush(); snap_take();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, verse ? "ANTVERSE" : "FREE CITY"); pm_rule(2); pm_rule(18);
    for (i = 0; i < nrow; i++) {
      u8 y = (u8)(4 + i * 2), c = i == sel ? 9 : 3;
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      if (i == 0) { ps(3, y, "MAP", c); ps(8, y, mname(mapt), 9); }
      else if (i == 1) { ps(3, y, "SIZE", c); if (hmap(mapt)) { pn(8, y, hmap(mapt)->w, 15); ps(11, y, "X", 15); pn(13, y, hmap(mapt)->h, 15); } else { pn(8, y, tw, 9); ps(11, y, "X", 10); pn(13, y, th, 9); } }
      else if (i == 2) { ps(3, y, "MONEY", c); ps(9, y, MN[cmoney], 9); }
      else if (i == 3) { ps(3, y, "DISASTERS", c); ps(13, y, cdis_on ? "ON" : "OFF", cdis_on ? 8 : 13); }
      else if (i == 4 && verse) { ps(3, y, "RIVALS", c); pc(10, y, (char)('0' + nriv), 9); ps(12, y, AGN[agg], 9); }
      else if (i == nrow - 3) { static const char *const PW[3] = {"OPTIONS", "ON", "OFF"}; ps(3, y, "PALACE", c); ps(10, y, PW[pzw], pzw == 2 ? 15 : 13); }
      else if (i == nrow - 1) ps(3, y, "START!", i == sel ? 9 : 13);
      else if (i == nrow - 2) ps(3, y, "HOW TO PLAY", c);
    }
    ps(1, 19, sel == 1 ? "< > WIDE  \1 + < > TALL" : "< > CHANGE  \1 GO  \2 BACK", 10);
    { u8 x, y; for (y = 4; y < 16; y++) for (x = 17; x < 29; x++) BGMAP1[y * 32 + x] = 0; }
    DISPCNT = 0x1B40; BLDY = 0;
    snap_put(); gen_world(); snap_put(); map_img(17, 4, 0);
    for (;;) {
      vsync(); fr++; k = joy(); p = k & ~prev; prev = k;
      if (!p) { if (dirty && ++dirty > 30) { dirty = 0; break; } continue; }
      if (p & J_B) { ewait_clear(); return 0; }
      if (p & J_SEL) { pool_mix(fr * 2654435761u ^ VCOUNT); rs ^= (u32)fr * 40503u + VCOUNT; if (!rs) rs = 1; snap_take(); break; }
      if (p & J_UP) { sel = (u8)(sel ? sel - 1 : nrow - 1); sfx_food(); break; }
      if (p & J_DOWN) { sel = (u8)(sel + 1 >= nrow ? 0 : sel + 1); sfx_food(); break; }
      if (p & (J_LEFT | J_RIGHT)) {
        s8 dd = (p & J_RIGHT) ? 1 : -1;
        if (sel == 0) { mapt = (u8)((mapt + MAPN + dd) % MAPN); if (mapt == MMY && !has_my) mapt = (u8)((mapt + MAPN + dd) % MAPN); dirty = 1; }
        else if (sel == 1 && !hmap(mapt)) { if (k & J_A) th = (u8)(th + dd * 8 < 32 ? 96 : th + dd * 8 > 96 ? 32 : th + dd * 8); else tw = (u8)(tw + dd * 8 < 32 ? 96 : tw + dd * 8 > 96 ? 32 : tw + dd * 8); dirty = 1; }
        else if (sel == 2) cmoney = (u8)((cmoney + 4 + dd) % 4);
        else if (sel == 3) cdis_on ^= 1;
        else if (sel == 4 && verse) { if (k & J_A) agg = (u8)((agg + 3 + dd) % 3); else nriv = (u8)(nriv + dd < 1 ? 3 : nriv + dd > 3 ? 1 : nriv + dd); }
        else if (sel == nrow - 3) pzw = (u8)((pzw + 3 + dd) % 3);
        sfx_food();
        { u8 y = (u8)(4 + sel * 2); for (i = 2; i < 16; i++) BGMAP1[y * 32 + i] = (u16)(1 | (3 << 12)); }
        if (!dirty) break;
        { u8 y = (u8)(4 + sel * 2); if (sel == 0) ps(8, y, mname(mapt), 9); else if (sel == 1) { pn(8, y, tw, 9); ps(11, y, "X", 10); pn(13, y, th, 9); } ps(3, y, sel ? "SIZE" : "MAP", 9); }
        continue;
      }
      if (p & (J_A | J_START)) {
        if (sel == nrow - 2) { static const char *const H1[6] = {"1. DIG TUNNELS: \1 BUILD,", "   TUNNELS + LAND TAB.", "2. PAINT ZONES NEAR THEM:", "   HOMES FARMS AND WORKS.", "3. ANTS MOVE IN ALONE", "   AND THE ZONES GROW!"};
          static const char *const H2[6] = {"4. ZONES GROW TO LEVEL 3 IF", "   THE BLOCK HAS AIR SHAFTS", "   GUARDS CLINICS SCHOOLS", "5. \2 ON A SQUARE TELLS WHAT", "   IT NEEDS. \3: DATA MAPS", "6. H F W BARS: DEMAND"};
          story_card("HOW TO PLAY", H1, "\1 NEXT"); story_card("HOW TO PLAY", H2, "\1 OK"); prev = joy(); break; }
        if (sel == nrow - 1 || (p & J_START)) { if (verse) { enriv = nriv; eaggr = agg; } snap_put(); ewait_clear(); return 1; }
      }
    }
  }
}
static void city_main(u8 verse) {
  static u8 sel = 0;
  u8 i, n; u16 fr = 0;
  if (verse && !verse_unlocked()) {
    static const char *const L[6] = {"ANTVERSE IS LOCKED.", "FINISH ALL 10 ANT CITY", "SCENARIOS TO OPEN IT:", "EVERY MODE AT ONCE!", 0, 0};
    DISPCNT = 0x80; story_card("LOCKED", L, "\1 OK"); return;
  }
  ecity = (u8)(verse ? 2 : 1);
  for (;;) {
    const char *it[5]; u8 has = city_has_save(verse);
    n = 0;
    if (!verse) it[n++] = "SCENARIOS";
    if (verse) it[n++] = "MISSIONS";
    it[n++] = verse ? "NEW ANTVERSE" : "FREE CITY";
    if (has) it[n++] = "CONTINUE";
    it[n++] = "BACK";
    if (sel >= n) sel = 0;
    DISPCNT = 0x80; hide_all(); oam_flush();
    ov_fill(1, 3, 0, 19); ui_full(); big(1, verse ? "ANTVERSE" : "ANT CITY");
    ps(2, 3, verse ? "CITY EMPIRE WAR AND POWERS" : "TUNNELS ZONES TAXES CHAOS", 10);
    pm_rule(4);
    for (i = 0; i < n; i++) { u8 y = (u8)(6 + i * 2);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, it[i], i == sel ? 9 : 3);
      if (!verse && i == 0) { pn(17, y, scen_count(), 9); ps(20, y, "OF 10", 10); }
      if (verse && i == 0) { pn(17, y, mis_count(), 9); ps(20, y, "OF 50", 10); } }
    if (!verse) ps(1, 16, verse_unlocked() ? "ANTVERSE IS OPEN!" : "WIN ALL 10 TO OPEN ANTVERSE", verse_unlocked() ? 13 : 10);
    ps(1, 19, "\1 OK   \2 BACK", 10);
    DISPCNT = 0x1B40; BLDY = 0;
    while (!ekey()) fr++;
    if (emp_ & J_B) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : n - 1);
    if (emp_ & J_DOWN) sel = (u8)(sel + 1 >= n ? 0 : sel + 1);
    if (emp_ & J_A) {
      const char *c = it[sel];
      if (c[0] == 'B') break;
      ecity = (u8)(verse ? 2 : 1);
      if (c[0] == 'M') { missions_menu(); ecity = 2; continue; }
      if (c[0] == 'S') {                            // scenario list
        u8 s = 0, done = 0, top = 0; u16 dv = scen_done();
        for (i = 0; i < NSCEN; i++) if (!((dv >> i) & 1)) { s = i; break; }
        ewait_clear();
        for (;;) {
          ov_fill(1, 3, 0, 19); ui_full(); big(0, "SCENARIOS");
          if (s < top) top = s; if (s >= top + 5) top = (u8)(s - 4);
          if (top) pc(29, 3, '-', 10); if (top + 5 < NSCEN) pc(29, 11, '+', 10);
          for (i = top; i < NSCEN && i < top + 5; i++) { u8 y = (u8)(3 + (i - top) * 2);
            if (i == s) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
            pn(2, y, (u8)(i + 1), 10); ps(5, y, SC[i].name, i == s ? 9 : 3); if ((dv >> i) & 1) ps(21, y, "DONE", 13); }
          pm_rule(13); ps(1, 14, SC[s].s[3], 3); ps(1, 16, "YEARS", 10); pn(7, 16, SC[s].years, 9); ps(12, 16, "MONEY", 10); pcash(18, 16, SC[s].cash, 9);
          ps(1, 19, "\1 PLAY   \2 BACK", 10);
          while (!ekey()) {}
          if (emp_ & J_B) { done = 1; break; }
          if (emp_ & J_UP) s = (u8)(s ? s - 1 : NSCEN - 1);
          if (emp_ & J_DOWN) s = (u8)(s + 1 >= NSCEN ? 0 : s + 1);
          if (emp_ & J_A) break;
        }
        ewait_clear();
        if (done) continue;
        cscen = s; cmoney = 1;
        { const char *L[6] = {SC[s].s[0], SC[s].s[1], SC[s].s[2], SC[s].s[3], 0, 0}; story_card(SC[s].name, L, "\1 BEGIN"); }
        city_newgame(); city_play(); city_end_screen(); ecity = (u8)(verse ? 2 : 1); continue;
      }
      if (c[0] == 'C') { city_load(); cscen = 255; city_play(); city_end_screen(); ecity = (u8)(verse ? 2 : 1); continue; }
      cscen = 255;                                  // FREE CITY / NEW ANTVERSE
      if (!city_setup(verse)) continue;
      city_newgame(); if (verse) city_goal_card(); city_play(); city_end_screen(); ecity = (u8)(verse ? 2 : 1);
    }
  }
  ewait_clear(); ecity = 0; emode = 0;
}
