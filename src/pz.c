// ============================================================================================================================
//  EL PRESIDANTE (included from city.c): a Tropico-style politics engine for ANT CITY and ANTVERSE. Switch it on in
//  OPTIONS > EL PRESIDANTE, on the FREE CITY / ANTVERSE setup screen, or per mission.
//
//  The CONSPICUOUS side: the PALACE menu (groups, happiness, 15 edicts, the charter, votes, the two superpowers, the honey
//  pot, the rebels), vote cards with speeches and rigging, coup and uprising cards, events with two choices, exile.
//  The INCONSPICUOUS side (the hidden hand): every month each group quietly pushes the city sim around according to how it
//  feels: strikes trim your income, prophets send ants away, angry greens start fires in the works, natives feed the crime
//  numbers, happy scholars make the zones grow faster... The city never says who did it; the PALACE > ADVISOR does.
// ============================================================================================================================
static const char *const FN[NFAC] = {"WORKERS", "TRADERS", "MYSTICS", "SOLDIERS", "SCHOLARS", "GREENS", "NATIVES", "LOYALISTS"};
static const char *const FW[NFAC] = {"JOBS HOMES WAGES", "MONEY LOW TAXES TRADE", "TEMPLES AND FAITH", "SAFETY GUARDS ORDER",
  "SCHOOLS CLINICS FREEDOM", "CLEAN AIR AND PARKS", "FOOD FUN THE OLD WAYS", "YOU! RESPECT AND ORDER"};
static const char *const HN2[NH] = {"FOOD", "HOMES", "HEALTH", "FAITH", "FUN", "SAFETY", "JOBS", "FREEDOM", "WAGES"};
static const u8 FWT[NFAC][NH] = {                  // how much each group cares about each part of life
  {2, 3, 1, 0, 1, 1, 3, 1, 3}, {1, 1, 1, 0, 2, 2, 2, 2, 3}, {1, 1, 1, 3, 0, 1, 0, 1, 1}, {2, 1, 1, 1, 1, 3, 1, 0, 2},
  {1, 1, 3, 0, 1, 1, 1, 3, 1}, {2, 1, 3, 0, 2, 0, 0, 2, 0}, {3, 2, 1, 2, 2, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 2, 1, 0, 1}};
static const char *const EN2[NE] = {"FOOD FOR ALL", "LITERACY", "MARTIAL LAW", "FESTIVAL", "NO SPORE JUICE", "SECRET POLICE", "PROPAGANDA",
  "TAX THE RICH", "OPEN BORDERS", "CLOSED BORDERS", "GREEN LAW", "PENSIONS", "CRACKDOWN", "CULT OF ME", "AMNESTY"};
static const char *const ED2[NE] = {"FREE FOOD: FOOD +15 NATIVES +", "SCHOLARS +  HEALTH +5", "SAFETY +20 FREEDOM -25 NO VOTE",
  "3 MONTHS OF FUN: ALL +5", "MYSTICS +  FUN -10", "COUPS -  REBELS -  FREEDOM -15", "RESPECT UP  LOYALISTS +",
  "MONEY +10 PERCENT  TRADERS -", "ANTS MOVE IN  NATIVES -", "NATIVES +  FEWER NEW ANTS", "SMOG -20  WORKS EARN LESS",
  "WORKERS +  NATIVES +  WAGES +", "NOW: REBELS HALVED  FREEDOM -10", "RESPECT +  FREEDOM -  SCHOLARS -", "NOW: REBELS GO HOME  SOLDIERS -"};
static const u16 EC2[NE] = {40, 30, 0, 100, 10, 50, 30, 0, 0, 0, 40, 60, 150, 30, 50};   // monthly cost (one-shot for FESTIVAL CRACKDOWN AMNESTY)
static const u8 EONCE[NE] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static const s8 EFX[NE][NFAC] = {                  // what each edict does to each group (per month while on)
  {6, -2, 0, 0, 0, 0, 8, 0}, {2, 0, -2, 0, 8, 2, -2, 0}, {-6, -4, -2, 10, -8, -4, -4, 6}, {4, 4, 2, 2, 2, 2, 6, 2},
  {-4, -2, 8, 0, -2, 0, -2, 2}, {-2, -2, 0, 4, -8, -2, -2, 6}, {0, 0, 0, 2, -4, 0, 0, 8}, {4, -10, 0, 0, 2, 2, 2, 0},
  {2, 6, 0, -2, 4, -2, -8, -2}, {-2, -6, 2, 2, -4, 2, 8, 2}, {-4, -4, 0, 0, 2, 12, 2, 0}, {8, -4, 0, 2, 0, 0, 6, 0},
  {-2, 0, 0, 4, -6, -2, -4, 4}, {-2, -2, 2, 2, -8, -2, 0, 10}, {2, 0, 2, -8, 4, 2, 2, -4}};
static const char *const CAN[6] = {"VOTES", "SPEECH", "FAITH", "ARMY", "TRADE", "RULER"};
static const char *const CAV[6][3] = {{"2 YEARS", "4 YEARS", "6 YEARS"}, {"FREE", "POLITE", "NONE"}, {"FREE", "STATE TEMPLE", "NONE"},
  {"MILITIA", "STANDING", "NONE"}, {"OPEN", "TARIFFS", "CLOSED"}, {"PRESIDANT", "QUEEN", "COUNCIL"}};
static const char *const CAD[6] = {"HOW OFTEN THE COLONY VOTES", "FREE: FREEDOM +  POLITE: CALM", "STATE TEMPLE: MYSTICS LOVE IT",
  "STANDING ARMY: SAFE  COUPS", "TARIFFS: MONEY  CLOSED: NATIVES", "QUEEN: NO VOTES  COUNCIL: FREE"};
static const char *const SPN2[2] = {"RED EMPIRE", "GOLDEN HIVE"};
#define NEV 12
static const char *const EVN[NEV] = {"A SPY!", "STRIKE!", "A PROPHET", "THE JOURNALIST", "A HERO", "THE AUDIT", "A BIRTHDAY", "ASSASSIN!",
  "BEETLE TRADERS", "A POET", "THE RUMOR", "THE FLOOD FUND"};
static const char *const EV3[NEV][3] = {
  {"GUARDS CAUGHT A SPY FROM", "THE RED EMPIRE IN THE", "PALACE KITCHEN."},
  {"THE WORKERS HAVE DOWNED", "THEIR LEAVES. THEY WANT", "BETTER WAGES."},
  {"A WANDERING PROPHET", "PREACHES IN THE TUNNELS.", "CROWDS GATHER."},
  {"A JOURNALIST ASKS WHERE", "THE PALACE MONEY GOES.", "THE TRADERS LISTEN."},
  {"A WORKER SAVED TEN ANTS", "FROM A CAVE IN.", "THE CITY WANTS A STATUE."},
  {"THE GOLDEN HIVE WANTS TO", "AUDIT OUR BOOKS BEFORE", "ANY MORE AID."},
  {"IT IS YOUR BIRTHDAY!", "THE LOYALISTS WANT A", "PARADE IN YOUR HONOR."},
  {"A SOLDIER TRIED TO BITE", "YOU IN YOUR SLEEP. THE", "GUARDS STOPPED HIM."},
  {"BEETLE TRADERS OFFER", "SHINY GOODS FOR A PILE", "OF OUR FOOD."},
  {"A POET WROTE A SONG", "ABOUT YOU. IT IS NOT", "A KIND SONG."},
  {"A RUMOR SAYS THE DEW", "IS POISONED. ANTS ARE", "SCARED."},
  {"THE RED EMPIRE OFFERS", "GOLD FOR FLOOD WALLS.", "WITH STRINGS ATTACHED."}};
static const char *const EVA[NEV] = {"SEND HIM HOME", "PAY THEM *300", "WELCOME HIM", "OPEN THE BOOKS", "BUILD IT *400", "LET THEM LOOK", "HOLD A PARADE *200",
  "JAIL THE SOLDIERS", "TRADE 100 FOOD", "LAUGH ALONG", "TEST THE DEW *200", "TAKE THE GOLD"};
static const char *const EVB[NEV] = {"PUT HIM IN A JAR", "STAND FIRM", "SEND HIM AWAY", "SHUT THE DOOR", "A SPEECH IS ENOUGH", "REFUSE", "WORK AS USUAL",
  "FORGIVE HIM", "NO THANKS", "JAIL THE POET", "SAY IT IS FINE", "REFUSE"};
static u8 pz_mart(void) { return (u8)((PZ_.ed >> E_MARTIAL) & 1); }
static u8 pz_ed(u8 e) { return (u8)((PZ_.ed >> e) & 1); }
static void pz_add(u8 f, s8 d) { s16 v = (s16)PZ_.sup[f] + d; PZ_.sup[f] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v); }
static void pz_all(s8 d) { u8 i; for (i = 0; i < NFAC; i++) pz_add(i, d); }
static void pz_rel(u8 p, s8 d) { s16 v = (s16)PZ_.rel[p] + d; if (O(O_PZPOW) == 1 && v < 40) v = 40; PZ_.rel[p] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v); }
static void pz_resp(s8 d) { s16 v = (s16)PZ_.resp + d; PZ_.resp = (u8)(v < 0 ? 0 : v > 100 ? 100 : v); }
static void pz_lib(s8 d) { s16 v = (s16)PZ_.lib + d; PZ_.lib = (u8)(v < 0 ? 0 : v > 100 ? 100 : v); }
static u8 pz_votes_on(void) { return O(O_PZEL) != 3 && PZ_.con[5] != 1; }
static u8 pz_vote_gap(void) { return (u8)(PZ_.con[0] == 0 ? 24 : PZ_.con[0] == 1 ? 48 : 72); }
static u8 pz_avg(void) { u16 s = 0, n = 0; u8 i; for (i = 0; i < NFAC; i++) { s += (u16)PZ_.sup[i] * PZ_.siz[i]; n += PZ_.siz[i]; } return (u8)(n ? s / n : 50); }

// ---------- start ----------
static void pz_setup(u8 scen) {
  u8 i;
  memset(&PZ_, 0, sizeof PZ_);
  PZ_.on = (u8)(pzw == 1 || (pzw == 0 && (O(O_PZ) == 2 || (O(O_PZ) == 1 && scen == 255))));
  if (!PZ_.on) return;
  for (i = 0; i < NFAC; i++) { PZ_.sup[i] = 55; PZ_.siz[i] = 12; }
  PZ_.siz[F_LOYAL] = 16;
  for (i = 0; i < NH; i++) PZ_.hap[i] = 50;
  PZ_.resp = 50; PZ_.lib = 60; PZ_.rel[0] = PZ_.rel[1] = 50; PZ_.con[0] = O(O_PZEL) == 3 ? 1 : O(O_PZEL); PZ_.elt = pz_vote_gap(); PZ_.evc = 4;
}

// ---------- the month ----------
static void pz_sizes(void) {                       // who lives here: the city's make-up sets each group's share
  u16 w[NFAC], t = 0; u8 i;
  w[F_WORK] = (u16)(10 + czn[3][1] + czn[3][2] * 2 + czn[3][3] * 3);
  w[F_TRADE] = (u16)(6 + nb_of[B_MARKET] * 6 + nb_of[B_TRADE] * 6 + (inc_x > 0 ? inc_x / 20 : 0) + PZ_.deal[0] * 6 + PZ_.deal[1] * 6);
  w[F_MYST] = (u16)(6 + nb_of[B_TEMPLE] * 8 + (PZ_.con[2] == 1 ? 10 : 0));
  w[F_SOLD] = (u16)(6 + nb_of[B_GUARD] * 5 + nb_of[B_BARR] * 6 + nb_of[B_TOWER] * 3 + (PZ_.con[3] == 1 ? 10 : 0) + (u16)(my_troops() / 25));
  w[F_SCHOL] = (u16)(6 + nb_of[B_SCHOOL] * 6 + nb_of[B_ARCH] * 5 + nb_of[B_ACAD] * 6 + nb_of[B_CLINIC] * 2);
  w[F_GREEN] = (u16)(6 + nb_of[B_PARK] * 5 + nb_of[B_GARDEN] * 4 + (pz_ed(E_GREEN) ? 8 : 0));
  w[F_NAT] = (u16)(10 + czn[2][1] + czn[2][2] * 2 + czn[2][3] * 3 + nb_of[B_FARM] * 2);
  w[F_LOYAL] = (u16)(8 + PZ_.resp / 5 + nb_of[B_PALACE] * 10 + nb_of[B_STATUE] * 4);
  for (i = 0; i < NFAC; i++) t += w[i];
  for (i = 0; i < NFAC; i++) PZ_.siz[i] = (u8)(w[i] * 100 / t);
}
static void pz_happy(void) {                       // the 9 parts of life, read off the city
  u16 cvt = 0, cvc = 0, cvs = 0, cvg = 0, pk = 0; u8 a, b; s16 v;
  for (b = 0; b < (H + 3) / 4; b++) for (a = 0; a < (W + 3) / 4; a++) { cvt++; if (ccov[b][a] & CV_CLINIC) cvc++; if (ccov[b][a] & CV_SCHOOL) cvs++; if (ccov[b][a] & CV_GUARD) cvg++; pk += cpk[b][a]; }
  if (!cvt) cvt = 1;
  v = (s16)(chunger ? 10 : rf >= 300 ? 90 : 40 + rf / 6); if (pz_ed(E_FOOD)) v += 15; PZ_.hap[H_FOOD] = (u8)(v > 100 ? 100 : v);
  v = (s16)(70 - dR / 2); if (cmile >= 3) v += 5; PZ_.hap[H_HOME] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
  v = (s16)(20 + cvc * 50 / cvt + (100 - csmog) / 4); if (pz_ed(E_LIT)) v += 5; PZ_.hap[H_HEALTH] = (u8)(v > 100 ? 100 : v);
  v = (s16)(30 + (nb_of[B_TEMPLE] > 4 ? 4 : nb_of[B_TEMPLE]) * 12 + (PZ_.con[2] == 1 ? 15 : PZ_.con[2] == 2 ? -20 : 0) + (pz_ed(E_PROHIB) ? 5 : 0)); PZ_.hap[H_FAITH] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
  v = (s16)(25 + (pk > 40 ? 40 : pk) + nb_of[B_STADIUM] * 15 + nb_of[B_DANCE] * 8 + (PZ_.fest ? 30 : 0) - (pz_ed(E_PROHIB) ? 10 : 0)); PZ_.hap[H_FUN] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
  v = (s16)(100 - ccrime - (PZ_.reb > 60 ? 15 : 0) + (pz_mart() ? 20 : 0) + cvg * 10 / cvt); PZ_.hap[H_SAFE] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
  v = (s16)(70 - dW / 2); PZ_.hap[H_JOBS] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
  PZ_.hap[H_FREE] = PZ_.lib;
  v = (s16)(95 - (taxR + taxF + taxW) * 2 + (pz_ed(E_PENSION) ? 10 : 0) - (pz_ed(E_TAXRICH) ? 0 : 0)); PZ_.hap[H_MONEY] = (u8)(v < 0 ? 0 : v > 100 ? 100 : v);
}
static void pz_hand(void) {                        // the hidden hand: the groups quietly act on the city for next month
  u8 i; s8 d;
  PZ_.m_inc = 0; PZ_.m_dr = 0; PZ_.m_grow = 0; PZ_.m_crime = 0; PZ_.m_smog = 0; PZ_.m_lv = 0; PZ_.m_resp = 0;
  if (O(O_PZHID)) return;
  for (i = 0; i < NFAC; i++) {
    if (PZ_.lead[i] == 2 || PZ_.lead[i] == 3) continue;   // a jailed or exiled leader: the group keeps quiet
    d = (s8)(PZ_.sup[i] < 30 ? -1 : PZ_.sup[i] >= 75 ? 1 : 0);
    if (!d) continue;
    switch (i) {
    case F_WORK: PZ_.m_inc = (s8)(PZ_.m_inc + (d < 0 ? -25 : 8)); break;                   // strikes / overtime
    case F_TRADE: PZ_.m_lv = (s8)(PZ_.m_lv + (d < 0 ? -10 : 5)); PZ_.m_inc = (s8)(PZ_.m_inc + (d < 0 ? -8 : 8)); break;
    case F_MYST: PZ_.m_dr = (s8)(PZ_.m_dr + (d < 0 ? -20 : 10)); break;                   // prophets send ants away / bless the homes
    case F_SOLD: if (d < 0) { if (!O(O_PZCOUP) && PZ_.cwarn < 250) PZ_.cwarn++; } else PZ_.m_crime = (s8)(PZ_.m_crime - 10); break;
    case F_SCHOL: PZ_.m_grow = (s8)(PZ_.m_grow + (d < 0 ? -40 : 25)); break;                // brain drain / clever zoning
    case F_GREEN: if (d < 0) { if (!(rnd() & 1)) { u16 k; u8 x, y; for (k = 0; k < 200; k++) { x = (u8)(rnd() % W); y = (u8)(rnd() % H); if (EZN(y, x) == 3 && ELV(y, x)) { fire_start(x, y); break; } } } } else PZ_.m_smog = (s8)(PZ_.m_smog - 10); break;
    case F_NAT: PZ_.m_crime = (s8)(PZ_.m_crime + (d < 0 ? 15 : -5)); break;
    case F_LOYAL: PZ_.m_resp = (s8)(PZ_.m_resp + (d < 0 ? -3 : 2)); break;
    }
  }
  if (PZ_.reb > 60) PZ_.m_crime = (s8)(PZ_.m_crime + PZ_.reb / 12);
}
static void pz_vote_due(void);
static void pz_month(void) {
  u8 i, j; s16 t, w; u16 cost = 0, reb_in = 0, cvg = 0, cvt = 0; u8 a, b;
  if (!PZ_.on || cquiet) return;
  pz_sizes(); pz_happy();
  for (b = 0; b < (H + 3) / 4; b++) for (a = 0; a < (W + 3) / 4; a++) { cvt++; if (ccov[b][a] & CV_GUARD) cvg++; }
  if (!cvt) cvt = 1;
  // every group drifts toward what it wants
  for (i = 0; i < NFAC; i++) {
    t = 0; w = 0;
    for (j = 0; j < NH; j++) { t += (s16)FWT[i][j] * PZ_.hap[j]; w += FWT[i][j]; }
    t = (s16)(10 + t * 8 / (w * 10));
    for (j = 0; j < NE; j++) if (pz_ed(j) && !EONCE[j]) t += EFX[j][i];
    t += PZ_.mood[i];
    if (i == F_LOYAL) t += (s16)(PZ_.resp / 4 - 12);
    if (i == F_SCHOL || i == F_TRADE) t += (s16)(PZ_.lib / 5 - 12);
    if (i == F_GREEN) t -= (s16)(csmog / 4);
    if (i == F_SOLD) t += (s16)((PZ_.con[3] == 1 ? 12 : PZ_.con[3] == 2 ? -15 : 0) + (cvg * 20 / cvt));
    if (i == F_MYST) t += (s16)(PZ_.con[2] == 1 ? 12 : PZ_.con[2] == 2 ? -25 : 0);
    if (i == F_TRADE) t += (s16)(PZ_.con[4] == 0 ? 8 : PZ_.con[4] == 2 ? -12 : 0) + PZ_.deal[0] * 5 + PZ_.deal[1] * 5;
    if (i == F_NAT) t += (s16)(PZ_.con[4] == 2 ? 10 : 0);
    if (i == F_SCHOL) t += (s16)(PZ_.con[1] == 0 ? 8 : PZ_.con[1] == 2 ? -15 : 0);
    if (PZ_.con[5] == 2) t += 4; if (PZ_.con[5] == 1 && i != F_LOYAL && i != F_MYST) t -= 4;
    if (PZ_.lead[i] == 1) t += 15; if (PZ_.lead[i] == 2) t -= 12; if (PZ_.lead[i] == 3) t -= 20;
    t += O(O_PZDIF) == 0 ? 10 : O(O_PZDIF) == 2 ? -8 : 0;
    if (cht[21]) t = 100;
    if (t > 100) t = 100; if (t < 0) t = 0;
    if (PZ_.sup[i] < t) pz_add(i, (s8)(t - PZ_.sup[i] > 4 ? 4 : t - PZ_.sup[i])); else if (PZ_.sup[i] > t) pz_add(i, (s8)-(PZ_.sup[i] - t > 4 ? 4 : PZ_.sup[i] - t));
    if (PZ_.mood[i] > 0) PZ_.mood[i]--; else if (PZ_.mood[i] < 0) PZ_.mood[i]++;
    if (PZ_.ltm[i] && !--PZ_.ltm[i]) { if (PZ_.lead[i] == 1) PZ_.lead[i] = 0; else { PZ_.lead[i] = 0; PZ_.mood[i] = (s8)(PZ_.mood[i] - 10); } }
  }
  // respect and freedom
  t = (s16)(pz_avg() / 2 + PZ_.wins * 4 + PZ_.m_resp + (pz_ed(E_CULT) ? 10 : 0) + (pz_ed(E_PROPAG) ? 6 : 0) + nb_of[B_STATUE] * 2 + nb_of[B_PALACE] * 6 + (cmile >= 3 ? 10 : cmile * 3));
  if (t > 100) t = 100; if (t < 0) t = 0;
  if (PZ_.resp < t) pz_resp((s8)(t - PZ_.resp > 3 ? 3 : t - PZ_.resp)); else if (PZ_.resp > t) pz_resp((s8)-(PZ_.resp - t > 3 ? 3 : PZ_.resp - t));
  t = (s16)(55 + (PZ_.con[1] == 0 ? 15 : PZ_.con[1] == 2 ? -25 : 0) + (PZ_.con[5] == 2 ? 15 : PZ_.con[5] == 1 ? -20 : 0) - (pz_mart() ? 25 : 0) - (pz_ed(E_SECRET) ? 15 : 0)
    - (pz_ed(E_PROPAG) ? 5 : 0) - (pz_ed(E_CULT) ? 10 : 0) + (pz_ed(E_LIT) ? 5 : 0) + nb_of[B_SCHOOL] * 2 - (PZ_.lead[F_SCHOL] >= 2 ? 10 : 0));
  if (t > 100) t = 100; if (t < 0) t = 0;
  if (PZ_.lib < t) pz_lib((s8)(t - PZ_.lib > 3 ? 3 : t - PZ_.lib)); else if (PZ_.lib > t) pz_lib((s8)-(PZ_.lib - t > 3 ? 3 : PZ_.lib - t));
  // edict costs and the honey pot
  for (i = 0; i < NE; i++) if (pz_ed(i) && !EONCE[i]) cost += EC2[i];
  PZ_.edmon = (u8)(cost > 250 ? 250 : cost);
  if (cmoney != 3) cash -= cost;
  if (PZ_.fest && !--PZ_.fest) { PZ_.ed &= (u16)~(1 << E_FEST); say("THE FESTIVAL IS OVER"); }
  if (PZ_.skim) { s16 in = (s16)(inc_r + inc_w + inc_f); if (in > 0) { u32 k = (u32)in * PZ_.skim / 100; PZ_.stash += k; if (cmoney != 3) cash -= (s32)k; if (PZ_.stash >= 10000) ach_get(58); if (PZ_.whis < 250) PZ_.whis = (u8)(PZ_.whis + PZ_.skim / 5); } }
  else if (PZ_.whis) PZ_.whis--;
  if (PZ_.whis > 40 && !(rnd() % 6)) { PZ_.mood[F_TRADE] = (s8)(PZ_.mood[F_TRADE] - 15); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] - 10); PZ_.whis = 10; say("WHISPERS ABOUT THE PALACE MONEY"); }
  // rebels: the angry join them in the hills; guards and amnesty thin them out
  if (O(O_PZREB)) {
    for (i = 0; i < NFAC; i++) if (PZ_.sup[i] < 25) reb_in += (u16)(PZ_.siz[i] * O(O_PZREB) / 3);
    if (PZ_.lib < 25) reb_in += 6;
    PZ_.reb = (u16)(PZ_.reb + reb_in / 4);
    if (PZ_.reb > (u16)(cvg * 10 / cvt + (pz_ed(E_SECRET) ? 6 : 0) + (pz_mart() ? 8 : 0) + PZ_.guards / 2)) PZ_.reb = (u16)(PZ_.reb - (cvg * 10 / cvt + (pz_ed(E_SECRET) ? 6 : 0) + (pz_mart() ? 8 : 0) + PZ_.guards / 2)); else PZ_.reb = 0;
    if (PZ_.reb > 250) PZ_.reb = 250;
    if (PZ_.reb >= 30 && (u8)(rnd() % 100) < PZ_.reb / 4) {   // a raid
      u8 r = (u8)(rnd() % 3);
      if (r == 0 && ecity == 2 && enriv) { EA *e = new_army(1, rx[0], ry[0], (u16)(PZ_.reb / 4), 0, 0); if (e) say("REBELS MARCH ON THE CITY!"); }
      else if (r == 1) { fire_any(); say("REBELS SET A FIRE! \6 LOOK"); }
      else { s32 k = (s32)(cash > 0 ? cash / 20 : 0); cash -= k; say("REBELS RAIDED THE TREASURY"); }
    }
    if (PZ_.reb >= 150) PZ_.pend |= PZ_RISE;
  }
  // coups
  if (!O(O_PZCOUP)) {
    if (PZ_.sup[F_SOLD] < 25 && PZ_.con[3] != 2) { if (PZ_.cwarn < 250) PZ_.cwarn++; } else if (PZ_.cwarn) PZ_.cwarn--;
    if (PZ_.cwarn >= 4 && !(rnd() & 1)) { PZ_.cwarn = 0; PZ_.pend |= PZ_COUP; }
  }
  // the superpowers
  if (O(O_PZPOW)) for (i = 0; i < 2; i++) {
    if (PZ_.rel[i] < 50) pz_rel(i, 1); else if (PZ_.rel[i] > 50 && !PZ_.ally[i] && !PZ_.deal[i]) pz_rel(i, -1);
    if (pz_ed(E_OPEN)) pz_rel(i, 1); if (pz_ed(E_CLOSED)) pz_rel(i, -1);
    if (PZ_.deal[i]) { if (cmoney != 3) cash += 60 + (s32)cpop / 40; pz_rel((u8)(i ^ 1), -1); }
    if (PZ_.ally[i]) { if (cmoney != 3) cash += 40; }
    if (PZ_.askt[i]) PZ_.askt[i]--;
    if (PZ_.rel[i] < (O(O_PZPOW) == 3 ? 30 : 20) && !PZ_.ally[i ^ 1] && !PZ_.inv[i] && !(rnd() % 4)) { PZ_.inv[i] = 6; PZ_.sp = (u8)(i + 1); say(i ? "THE GOLDEN HIVE IS ANGRY!" : "THE RED EMPIRE IS ANGRY!"); }
    if (PZ_.inv[i]) { if (PZ_.rel[i] >= 40) { PZ_.inv[i] = 0; say("THE THREAT HAS PASSED"); } else if (!--PZ_.inv[i]) { PZ_.sp = (u8)(i + 1); PZ_.pend |= PZ_INV; } }
  }
  // votes
  if (pz_votes_on() && !pz_mart()) { if (PZ_.elt > 1) PZ_.elt--; else { PZ_.elt = pz_vote_gap(); PZ_.pend |= PZ_VOTE; } }
  if (PZ_.conc) PZ_.conc--;
  // events
  if (O(O_PZEVT) && PZ_.evc < 250) PZ_.evc++;
  if (O(O_PZEVT) && PZ_.evc >= (O(O_PZEVT) == 1 ? 14 : O(O_PZEVT) == 2 ? 8 : 4) && !(rnd() % 5)) { PZ_.evc = 0; PZ_.ev = (u8)(rnd() % NEV); PZ_.pend |= PZ_EVENT; }
  // the hidden hand for next month, and trophies
  pz_hand();
  if (PZ_.resp >= 90) ach_get(60);
  if (++PZ_.msgm >= 6) { PZ_.msgm = 0; { u8 lo = 0; for (i = 1; i < NFAC; i++) if (PZ_.sup[i] < PZ_.sup[lo]) lo = i; if (PZ_.sup[lo] < 30) { static char b[31]; const char *s = FN[lo]; u8 k = 0; s = "THE "; while (*s) b[k++] = *s++; s = FN[lo]; while (*s) b[k++] = *s++; s = " ARE UNHAPPY. \5 PALACE"; while (*s && k < 30) b[k++] = *s++; b[k] = 0; say(b); } } }
}

// ---------- cards: a choice with two buttons ----------
static u8 pz_choice(const char *title, const char *const *lines, const char *a, const char *b) {   // 0 = a, 1 = b
  u8 i, sel = 0; u16 fr = 0;
  music_stop(); fade(0, 16); DISPCNT = 0x80;
  hide_all(); vsync(); oam_flush();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(1, title); pm_rule(3);
    for (i = 0; i < 4 && lines[i]; i++) ps(2, (u8)(5 + i * 2), lines[i], 3);
    pm_rule(13);
    BGMAP1[15 * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, 15, a, 9);
    if (b) { BGMAP1[17 * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, 17, b, 9); }
    if (b) { ps(1, 19, "UP DOWN PICK  \1 CHOOSE", 10); if (sel) BGMAP1[15 * 32 + 1] = 0; else BGMAP1[17 * 32 + 1] = 0; }
    else ps(1, 19, "\1 OK", 10);
    spr(0, 196, 118, BIGT, 0, 0, 1); oam[1] |= 0x8000;
    DISPCNT = 0x1F40; BLDY = 0;
    ewait_clear();
    for (;;) { vsync(); oam_flush(); fr++; oam[0] = (u16)((118 + ((fr >> 3) & 1)) & 0xFF); if (ekey()) break; }
    if (b && (emp_ & (J_UP | J_DOWN))) { sel ^= 1; sfx_food(); continue; }
    if (emp_ & (J_A | J_START)) break;
  }
  ewait_clear(); hide_all(); oam_flush();
  return sel;
}
static void pz_fall(const char *why) {             // a lost vote, a coup, an uprising
  static const char *const L1[4] = {"", "YOU ARE CARRIED OUT OF THE", "PALACE. THE CITY GOES ON", "WITHOUT YOU."};
  static const char *const L2[4] = {"", "YOU SNEAK BACK IN A", "BEETLE SHELL. HALF THE HONEY", "POT IS GONE. START AGAIN!"};
  const char *A[4] = {why, L1[1], L1[2], L1[3]}, *B[4] = {why, L2[1], L2[2], L2[3]};
  PZ_.falls++;
  if (O(O_PZLOSE) == 0 || PZ_.falls >= 3) { pz_choice("THE END", A, "\1 OK", 0); city_end(0, 30); return; }
  pz_choice("EXILE", B, "\1 OK", 0);
  PZ_.stash /= 2; PZ_.resp = 30; PZ_.reb = 0; PZ_.cwarn = 0; PZ_.rig = 0; PZ_.vbon = 0; PZ_.prom = 0; PZ_.spk = 0; PZ_.elt = pz_vote_gap();
  { u8 i; for (i = 0; i < NFAC; i++) { PZ_.sup[i] = 50; PZ_.mood[i] = 0; PZ_.lead[i] = 0; } }
  if (cmoney != 3) cash -= cash > 0 ? cash / 4 : 0;
  PZ_.ed &= (u16)~(1 << E_MARTIAL);
}
static u16 pz_vote_pct(void) {
  s16 v = (s16)(pz_avg() + PZ_.vbon + (PZ_.rig ? 15 : 0) + (cht[22] ? 50 : 0));
  return (u16)(v < 0 ? 0 : v > 100 ? 100 : v);
}
static void pz_vote_card(void) {
  u8 i, y = 5; u16 pct = pz_vote_pct(), fr = 0; u8 won;
  music_stop(); fade(0, 16); DISPCNT = 0x80; hide_all(); vsync(); oam_flush();
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "THE VOTE"); pm_rule(2);
  ps(1, 3, "GROUP      SIZE  FOR YOU", 10);
  for (i = 0; i < NFAC; i++, y++) { u8 j; ps(1, y, FN[i], 3); pn(12, y, PZ_.siz[i], 10); for (j = 0; j < 10; j++) BGMAP1[y * 32 + 17 + j] = (u16)((PZ_.sup[i] > j * 10 ? BAR_F : BAR_E) | ((PZ_.sup[i] >= 50 ? 13 : 8) << 12)); }
  pm_rule(13);
  won = pct >= 50;
  if (PZ_.rig) { if (PZ_.lib > 50 && (rnd() % 100) < 40) { ps(1, 14, "THE RIGGING WAS FOUND OUT!", 8); pz_lib(-15); pz_resp(-15); pz_all(-10); pct = (u16)(pct > 15 ? pct - 15 : 0); won = pct >= 50; } else ps(1, 14, "THE COUNT WAS... ARRANGED", 10); }
  else if (PZ_.vbon) ps(1, 14, "YOUR SPEECH SWAYED THEM", 10);
  ps(1, 16, "VOTES FOR YOU:", 3); pn(16, 16, (u8)(pct > 99 ? 99 : pct), won ? 13 : 8); ps(19, 16, "PERCENT", 3);
  ps(1, 17, won ? "YOU STAY IN THE PALACE!" : "THE COLONY VOTED YOU OUT", won ? 13 : 8);
  ps(1, 19, "\1 OK", 10);
  DISPCNT = 0x1B40; BLDY = 0;
  if (won) jingle(WIN_TUNE, 6); else jingle(LOSE_TUNE, 4);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_START))) { fr++; } ewait_clear();
  PZ_.lastv = (u8)pct;
  if (won) { PZ_.wins++; pz_resp(8); pz_all(2); if (PZ_.wins >= 3) ach_get(56); if (pct >= 80) ach_get(57); }
  if (PZ_.prom) {                                   // the promise made at the speech: kept?
    u8 part = (u8)(PZ_.prom - 1), kept = PZ_.hap[part] >= 60;
    PZ_.mood[PZ_.promf] = (s8)(PZ_.mood[PZ_.promf] + (kept ? 10 : -20)); if (!kept) pz_resp(-5);
    PZ_.prom = 0;
  }
  PZ_.rig = 0; PZ_.vbon = 0; PZ_.spk = 0;
  if (!won) pz_fall("THE VOTE WAS LOST.");
}
static u8 pz_defense(void) {
  u16 d = (u16)(20 + nb_of[B_GUARD] * 8 + nb_of[B_TOWER] * 4 + PZ_.sup[F_LOYAL] / 2 + (pz_mart() ? 25 : 0) + PZ_.guards + (PZ_.con[3] == 1 ? 15 : 0) + (ecity == 2 ? my_troops() / 10 : 0) + (pz_ed(E_SECRET) ? 10 : 0));
  return (u8)(d > 200 ? 200 : d);
}
static void pz_fight_card(u8 coup) {               // coup or uprising: the guards against the plotters
  static const char *const CL[4] = {"THE SOLDIERS MARCH ON THE", "PALACE! YOUR GUARDS AND", "THE LOYALISTS FIGHT BACK.", ""};
  static const char *const RL[4] = {"THE REBELS POUR OUT OF THE", "HILLS AND INTO THE CITY!", "YOUR GUARDS FIGHT BACK.", ""};
  u8 def = pz_defense(), att = (u8)(coup ? 40 + (100 - PZ_.sup[F_SOLD]) / 2 : 30 + PZ_.reb / 3), win;
  const char *L[4] = {coup ? CL[0] : RL[0], coup ? CL[1] : RL[1], coup ? CL[2] : RL[2], 0};
  pz_choice(coup ? "A COUP!" : "UPRISING!", L, "\1 TO THE GATES!", 0);
  win = (u8)((u16)def * 100 / (def + att) > (u8)(rnd() % 100));
  if (win) {
    static const char *const W1[4] = {"", "THE PLOTTERS ARE BEATEN AND", "THE CITY CHEERS. ORDER", "HOLDS... FOR NOW."};
    const char *M[4] = {coup ? "THE COUP FAILED!" : "THE UPRISING FAILED!", W1[1], W1[2], W1[3]};
    pz_choice("VICTORY", M, "\1 OK", 0); ach_get(59); pz_resp(10); PZ_.mood[F_LOYAL] = (s8)(PZ_.mood[F_LOYAL] + 10);
    if (coup) { PZ_.lead[F_SOLD] = 2; PZ_.ltm[F_SOLD] = 24; PZ_.sup[F_SOLD] = 35; } else { PZ_.reb = (u16)(PZ_.reb / 5); }
    { u8 i; for (i = 0; i < 2; i++) fire_any(); }
  } else { if (!coup) PZ_.reb = 20; pz_fall(coup ? "THE SOLDIERS TOOK THE PALACE." : "THE REBELS TOOK THE PALACE."); }
}
static void pz_invasion(void) {
  u8 p = (u8)(PZ_.sp - 1);
  static const char *const L[4] = {"THEIR RAIDERS CROSS THE", "BORDER! THEY BURN AND", "STEAL WHAT THEY CAN.", ""};
  pz_choice(p ? "THE GOLDEN HIVE ATTACKS" : "THE RED EMPIRE ATTACKS", L, "\1 DEFEND THE CITY", 0);
  if (ecity == 2 && enriv) { EA *e = new_army(1, rx[0], ry[0], 80, 20, 10); if (e) { alt = 240; alx = rx[0]; aly = ry[0]; } }
  else { beetle_any(); fire_any(); fire_any(); }
  if (cmoney != 3) cash -= cash > 0 ? cash / 10 : 0;
  pz_rel(p, 10); PZ_.inv[p] = 0; PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] + 8);
}
static void pz_event(void) {
  u8 e = PZ_.ev, c; const char *L[4] = {EV3[e][0], EV3[e][1], EV3[e][2], 0};
  c = pz_choice(EVN[e], L, EVA[e], EVB[e]);
  switch (e) {
  case 0: if (!c) { pz_rel(0, -8); pz_lib(2); } else { pz_rel(0, -20); PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] + 8); pz_lib(-5); } break;
  case 1: if (!c) { if (cmoney != 3) cash -= 300; PZ_.mood[F_WORK] = (s8)(PZ_.mood[F_WORK] + 15); } else { PZ_.mood[F_WORK] = (s8)(PZ_.mood[F_WORK] - 15); PZ_.m_inc = (s8)(PZ_.m_inc - 30); pz_resp(3); } break;
  case 2: if (!c) { PZ_.mood[F_MYST] = (s8)(PZ_.mood[F_MYST] + 15); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] - 5); } else { PZ_.mood[F_MYST] = (s8)(PZ_.mood[F_MYST] - 15); pz_lib(-5); } break;
  case 3: if (!c) { PZ_.whis = 0; PZ_.mood[F_TRADE] = (s8)(PZ_.mood[F_TRADE] + 10); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] + 8); if (PZ_.stash) { pz_resp(-10); pz_all(-5); } } else { pz_lib(-8); PZ_.whis = (u8)(PZ_.whis + 20); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] - 10); } break;
  case 4: if (!c) { if (cmoney != 3) cash -= 400; pz_all(4); pz_resp(5); } else { PZ_.mood[F_WORK] = (s8)(PZ_.mood[F_WORK] - 5); pz_resp(1); } break;
  case 5: if (!c) { if (PZ_.stash) { pz_rel(1, -15); PZ_.whis = (u8)(PZ_.whis + 30); } else { pz_rel(1, 12); if (cmoney != 3) cash += 500; } } else pz_rel(1, -8); break;
  case 6: if (!c) { if (cmoney != 3) cash -= 200; pz_resp(8); PZ_.mood[F_LOYAL] = (s8)(PZ_.mood[F_LOYAL] + 10); } else { PZ_.mood[F_LOYAL] = (s8)(PZ_.mood[F_LOYAL] - 6); PZ_.mood[F_WORK] = (s8)(PZ_.mood[F_WORK] + 3); } break;
  case 7: if (!c) { PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] - 12); PZ_.cwarn = 0; pz_lib(-5); } else { PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] + 6); pz_resp(5); } break;
  case 8: if (!c) { if (rf >= 100) { rf -= 100; if (cmoney != 3) cash += 600; PZ_.mood[F_TRADE] = (s8)(PZ_.mood[F_TRADE] + 8); } else say("NOT ENOUGH FOOD. NO DEAL"); } break;
  case 9: if (!c) { pz_resp(-3); pz_lib(3); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] + 6); } else { pz_lib(-10); pz_resp(2); PZ_.mood[F_SCHOL] = (s8)(PZ_.mood[F_SCHOL] - 12); } break;
  case 10: if (!c) { if (cmoney != 3) cash -= 200; pz_all(3); } else { PZ_.m_dr = (s8)(PZ_.m_dr - 25); PZ_.mood[F_NAT] = (s8)(PZ_.mood[F_NAT] - 8); } break;
  default: if (!c) { if (cmoney != 3) cash += 1500; pz_rel(0, 10); pz_rel(1, -10); PZ_.deal[0] = 0; pz_lib(-3); } else { pz_rel(0, -5); PZ_.mood[F_NAT] = (s8)(PZ_.mood[F_NAT] + 4); } break;
  }
}
static u8 pz_cards(void) {                         // the play loop calls this: 1 = something was shown
  if (!PZ_.on || !PZ_.pend) return 0;
  if (PZ_.pend & PZ_EVENT) { PZ_.pend &= (u8)~PZ_EVENT; pz_event(); return 1; }
  if (PZ_.pend & PZ_VOTE) { PZ_.pend &= (u8)~PZ_VOTE; pz_vote_card(); return 1; }
  if (PZ_.pend & PZ_COUP) { PZ_.pend &= (u8)~PZ_COUP; pz_fight_card(1); return 1; }
  if (PZ_.pend & PZ_RISE) { PZ_.pend &= (u8)~PZ_RISE; pz_fight_card(0); return 1; }
  if (PZ_.pend & PZ_INV) { PZ_.pend &= (u8)~PZ_INV; pz_invasion(); return 1; }
  PZ_.pend = 0; return 0;
}

// ---------- the PALACE menu ----------
static void pz_bar(u8 x, u8 y, u8 v, u8 bank) { u8 j; for (j = 0; j < 10; j++) BGMAP1[y * 32 + x + j] = (u16)((v > j * 10 ? BAR_F : BAR_E) | (bank << 12)); }
static void pz_groups(void) {
  u8 sel = 0, i, y;
  static const char *const LS[4] = {"", "BRIBED", "IN JAIL", "EXILED"};
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "THE GROUPS");
    ps(1, 2, "GROUP     SIZE  SUPPORT", 10);
    for (i = 0; i < NFAC; i++) { y = (u8)(3 + i);
      if (i == sel) BGMAP1[y * 32 + 0] = (u16)((ORN + 5) | (9 << 12));
      ps(1, y, FN[i], i == sel ? 9 : 3); pn(11, y, PZ_.siz[i], 10); pz_bar(15, y, PZ_.sup[i], PZ_.sup[i] >= 50 ? 13 : PZ_.sup[i] >= 30 ? 14 : 8);
      if (PZ_.lead[i]) ps(26, y, LS[PZ_.lead[i]], PZ_.lead[i] == 1 ? 13 : 8); }
    pm_rule(11);
    ps(1, 12, "WANT:", 10); ps(7, 12, FW[sel], 3);
    ps(1, 13, "LEADER:", 10); ps(9, 13, PZ_.lead[sel] == 0 ? "FREE" : LS[PZ_.lead[sel]], 3);
    if (PZ_.lead[sel] && PZ_.ltm[sel]) { pn(17, 13, PZ_.ltm[sel], 10); ps(20, 13, "MONTHS", 10); }
    ps(1, 15, "\1 FLATTER +3   \3 BRIBE *500", 10);
    ps(1, 16, "\4 JAIL 1 YEAR  \6 EXILE 2 YEARS", 10);
    ps(1, 18, "JAILED OR EXILED: QUIET BUT MAD", 15);
    ps(1, 19, "UP DOWN PICK   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : NFAC - 1);
    if (emp_ & J_DOWN) sel = (u8)(sel + 1 >= NFAC ? 0 : sel + 1);
    if (emp_ & J_A) { if (PZ_.mood[sel] < 20) { PZ_.mood[sel] = (s8)(PZ_.mood[sel] + 3); pz_add(sel, 3); sfx_mana(); } else sfx_deny(); }
    if (emp_ & J_L) { if (PZ_.lead[sel] == 0 && (cmoney == 3 || cash >= 500)) { if (cmoney != 3) cash -= 500; PZ_.lead[sel] = 1; PZ_.ltm[sel] = 12; pz_add(sel, 15); sfx_mana(); } else sfx_deny(); }
    if (emp_ & J_R) { if (PZ_.lead[sel] == 0 || PZ_.lead[sel] == 1) { PZ_.lead[sel] = 2; PZ_.ltm[sel] = 12; pz_add(sel, -12); pz_lib(-6); pz_all(-1); sfx_alarm(); } else sfx_deny(); }
    if (emp_ & J_SEL) { if (PZ_.lead[sel] != 3) { PZ_.lead[sel] = 3; PZ_.ltm[sel] = 24; pz_add(sel, -20); pz_lib(-10); pz_all(-2); sfx_alarm(); } else sfx_deny(); }
  }
  ewait_clear();
}
static void pz_happy_screen(void) {
  u8 i, y;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "LIFE IN THE CITY");
  for (i = 0; i < NH; i++) { y = (u8)(3 + i); ps(1, y, HN2[i], 3); pz_bar(12, y, PZ_.hap[i], PZ_.hap[i] >= 60 ? 13 : PZ_.hap[i] >= 35 ? 14 : 8); pn(23, y, PZ_.hap[i] > 99 ? 99 : PZ_.hap[i], 10); }
  pm_rule(12);
  ps(1, 13, "RESPECT", 9); pz_bar(12, 13, PZ_.resp, 9); pn(23, 13, PZ_.resp > 99 ? 99 : PZ_.resp, 9);
  ps(1, 14, "FREEDOM", 9); pz_bar(12, 14, PZ_.lib, 14); pn(23, 14, PZ_.lib > 99 ? 99 : PZ_.lib, 14);
  ps(1, 16, "EVERY GROUP CARES ABOUT A FEW", 10); ps(1, 17, "OF THESE. SEE THE GROUPS.", 10);
  ps(1, 19, "\2 BACK", 10);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_B | J_START))) {} ewait_clear();
}
static void pz_edicts(void) {
  u8 sel = 0, i, top = 0;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "EDICTS");
    if (sel < top) top = sel; if (sel >= top + 6) top = (u8)(sel - 5);
    if (top) pc(29, 2, '-', 10); if (top + 6 < NE) pc(29, 12, '+', 10);
    for (i = top; i < NE && i < top + 6; i++) { u8 y = (u8)(2 + (i - top) * 2);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, EN2[i], i == sel ? 9 : 3); if (!EONCE[i]) ps(19, y, pz_ed(i) ? "ON" : "OFF", pz_ed(i) ? 13 : 15); else ps(19, y, "NOW", 10);
      if (EC2[i]) { pc(23, y, '*', 10); pnum(24, y, EC2[i], 10); } }
    pm_rule(14); ps(1, 15, ED2[sel], 3);
    ps(1, 16, EONCE[sel] ? "PAID ONCE" : EC2[sel] ? "COSTS EVERY MONTH" : "FREE", 10);
    ps(1, 17, "LAWS LAST MONTH:", 10); pc(18, 17, '*', 9); pnum(19, 17, PZ_.edmon, 9);
    ps(1, 19, "\1 ON OFF   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : NE - 1);
    if (emp_ & J_DOWN) sel = (u8)(sel + 1 >= NE ? 0 : sel + 1);
    if (emp_ & J_A) {
      if (EONCE[sel]) {
        if (cmoney != 3 && cash < EC2[sel]) { sfx_deny(); continue; }
        if (cmoney != 3) cash -= EC2[sel]; sfx_mana();
        if (sel == E_FEST) { PZ_.fest = 3; PZ_.ed |= (u16)(1 << E_FEST); pz_all(5); say("A FESTIVAL! 3 MONTHS OF FUN"); }
        else if (sel == E_CRACK) { PZ_.reb /= 2; pz_lib(-10); PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] + 6); say("THE REBELS ARE SCATTERED"); }
        else { PZ_.reb = 0; PZ_.mood[F_SOLD] = (s8)(PZ_.mood[F_SOLD] - 8); pz_lib(5); say("THE REBELS CAME HOME"); }
        continue;
      }
      if ((sel == E_OPEN && pz_ed(E_CLOSED)) || (sel == E_CLOSED && pz_ed(E_OPEN))) { say("ONE BORDER LAW AT A TIME"); sfx_deny(); continue; }
      PZ_.ed ^= (u16)(1 << sel); sfx_mana();
      if (sel == E_MARTIAL && pz_ed(E_MARTIAL)) ach_get(22);
    }
  }
  ewait_clear();
}
static void pz_charter(void) {
  u8 sel = 0, i;
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "THE CHARTER");
    for (i = 0; i < 6; i++) { u8 y = (u8)(3 + i * 2);
      if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12));
      ps(3, y, CAN[i], i == sel ? 9 : 3); ps(11, y, "<", 10); ps(13, y, CAV[i][PZ_.con[i]], 9); ps(27, y, ">", 10); }
    pm_rule(15); ps(1, 16, CAD[sel], 3);
    if (PZ_.conc) { ps(1, 17, "NEXT CHANGE IN", 8); pn(16, 17, PZ_.conc, 8); ps(19, 17, "MONTHS", 8); } else ps(1, 17, "A CHANGE COSTS 5 RESPECT", 10);
    ps(1, 19, "< > CHANGE   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 5);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 5 ? 0 : sel + 1);
    if (emp_ & (J_LEFT | J_RIGHT)) {
      if (PZ_.conc) { sfx_deny(); continue; }
      PZ_.con[sel] = (u8)((PZ_.con[sel] + ((emp_ & J_RIGHT) ? 1 : 2)) % 3); PZ_.conc = 12; pz_resp(-5); sfx_mana();
      if (sel == 0) PZ_.elt = pz_vote_gap();
    }
  }
  ewait_clear();
}
static void pz_votes(void) {
  u8 sel = 0, i, part = 0;
  static const char *const IT[5] = {"GIVE A SPEECH", "MAKE A PROMISE", "RIG THE VOTE", "CALL A VOTE NOW", "BACK"};
  static const char *const IH[5] = {"+8 PERCENT NEXT VOTE (ONCE)", "A GROUP TRUSTS YOU. KEEP IT!", "+15 PERCENT. RISKY IF FREE", "HOLD THE VOTE THIS MONTH", ""};
  ewait_clear();
  for (;;) {
    u16 pct = pz_vote_pct();
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "VOTES");
    if (!pz_votes_on()) ps(1, 2, PZ_.con[5] == 1 ? "A QUEEN DOES NOT ASK FOR VOTES" : "VOTES ARE OFF", 8);
    else if (pz_mart()) ps(1, 2, "NO VOTES UNDER MARTIAL LAW", 8);
    else { ps(1, 2, "NEXT VOTE IN", 3); pn(14, 2, PZ_.elt, 9); ps(17, 2, "MONTHS", 3); }
    ps(1, 3, "IF HELD NOW:", 3); pn(14, 3, (u8)(pct > 99 ? 99 : pct), pct >= 50 ? 13 : 8); ps(17, 3, "PERCENT", 3); ps(25, 3, pct >= 50 ? "WIN" : "LOSE", pct >= 50 ? 13 : 8);
    ps(1, 4, "VOTES WON", 3); pn(14, 4, PZ_.wins, 9); if (PZ_.lastv) { ps(17, 4, "LAST", 3); pn(22, 4, PZ_.lastv > 99 ? 99 : PZ_.lastv, 9); }
    pm_rule(5);
    for (i = 0; i < 5; i++) { u8 y = (u8)(6 + i * 2); if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, y, IT[i], i == sel ? 9 : 3);
      if (i == 0 && PZ_.spk) ps(20, y, "DONE", 13); if (i == 1 && PZ_.prom) { ps(20, y, HN2[PZ_.prom - 1], 13); } if (i == 2 && PZ_.rig) ps(20, y, "DONE", 8); }
    pm_rule(16); ps(1, 17, sel == 1 ? "< > PICK WHAT TO PROMISE" : IH[sel], 3);
    if (sel == 1) { ps(1, 18, HN2[part], 9); ps(10, 18, "OVER 60 AT THE VOTE", 10); }
    ps(1, 19, "\1 DO IT   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 4);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 4 ? 0 : sel + 1);
    if (sel == 1 && (emp_ & (J_LEFT | J_RIGHT))) part = (u8)((part + ((emp_ & J_RIGHT) ? 1 : NH - 1)) % NH);
    if (emp_ & J_A) {
      if (sel == 4) break;
      if (!pz_votes_on() || pz_mart()) { sfx_deny(); continue; }
      if (sel == 0) { if (PZ_.spk) { sfx_deny(); continue; } PZ_.spk = 1; PZ_.vbon = (s8)(PZ_.vbon + 8); sfx_mana(); say("A FINE SPEECH!"); }
      else if (sel == 1) { u8 f = 0, j; if (PZ_.prom) { sfx_deny(); continue; }
        for (j = 1; j < NFAC; j++) if (FWT[j][part] > FWT[f][part]) f = j;
        PZ_.prom = (u8)(part + 1); PZ_.promf = f; pz_add(f, 12); PZ_.vbon = (s8)(PZ_.vbon + 4); sfx_mana(); }
      else if (sel == 2) { if (PZ_.rig) { sfx_deny(); continue; } if (cmoney != 3 && cash < 800) { say("IT COSTS *800"); sfx_deny(); continue; } if (cmoney != 3) cash -= 800; PZ_.rig = 1; sfx_alarm(); }
      else { PZ_.pend |= PZ_VOTE; PZ_.elt = pz_vote_gap(); sfx_mana(); break; }
    }
  }
  ewait_clear();
}
static void pz_powers(void) {
  u8 sel = 0, p = 0, i;
  static const char *const IT[5] = {"ASK FOR AID", "TRADE DEAL", "SEND A GIFT *300", "ALLIANCE *500", "BACK"};
  static const char *const IH[5] = {"+*1500 NOW  RELATION -8  ONCE A YEAR", "+*60 A MONTH  THE OTHER ONE SULKS", "RELATION +8", "THEY PROTECT YOU. NEEDS 70", ""};
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "SUPERPOWERS");
    if (!O(O_PZPOW)) ps(1, 2, "THE SUPERPOWERS ARE OFF", 8);
    for (i = 0; i < 2; i++) { u8 y = (u8)(3 + i * 3);
      ps(1, y, SPN2[i], i == p ? 9 : 3); ps(14, y, "<", i == p ? 10 : 15); ps(27, y, ">", i == p ? 10 : 15);
      pz_bar(16, y, PZ_.rel[i], PZ_.rel[i] >= 50 ? 13 : PZ_.rel[i] >= 30 ? 14 : 8);
      ps(1, (u8)(y + 1), PZ_.inv[i] ? "RAIDERS COMING IN" : PZ_.ally[i] ? "OUR ALLY" : PZ_.deal[i] ? "TRADE DEAL" : PZ_.rel[i] < 30 ? "ANGRY" : PZ_.rel[i] >= 70 ? "FRIENDLY" : "NEUTRAL", PZ_.inv[i] ? 8 : 13);
      if (PZ_.inv[i]) { pn(19, (u8)(y + 1), PZ_.inv[i], 8); ps(22, (u8)(y + 1), "MONTHS", 8); } }
    pm_rule(9);
    for (i = 0; i < 5; i++) { u8 y = (u8)(10 + i); if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, y, IT[i], i == sel ? 9 : 3);
      if (i == 0 && PZ_.askt[p]) { pn(22, y, PZ_.askt[p], 8); ps(25, y, "MON", 8); } if (i == 1 && PZ_.deal[p]) ps(22, y, "ON", 13); if (i == 3 && PZ_.ally[p]) ps(22, y, "ON", 13); }
    pm_rule(15); ps(1, 16, IH[sel], 3);
    ps(1, 17, "UNDER 20: THEY SEND RAIDERS", 10);
    ps(1, 19, "< > POWER  \1 DO IT  \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & (J_LEFT | J_RIGHT)) p ^= 1;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 4);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 4 ? 0 : sel + 1);
    if (emp_ & J_A) {
      if (sel == 4) break;
      if (!O(O_PZPOW)) { sfx_deny(); continue; }
      if (sel == 0) { if (PZ_.askt[p] || PZ_.rel[p] < 30) { sfx_deny(); say(PZ_.askt[p] ? "NOT YET" : "THEY SAY NO"); continue; } if (cmoney != 3) cash += 1500; pz_rel(p, -8); PZ_.askt[p] = 12; sfx_mana(); say("AID ARRIVED! +*1500"); }
      else if (sel == 1) { if (PZ_.deal[p]) { PZ_.deal[p] = 0; sfx_food(); continue; } if (PZ_.rel[p] < 40) { say("THEY WANT RELATION 40"); sfx_deny(); continue; } PZ_.deal[p] = 1; pz_rel((u8)(p ^ 1), -10); sfx_mana(); }
      else if (sel == 2) { if (cmoney != 3 && cash < 300) { sfx_deny(); continue; } if (cmoney != 3) cash -= 300; pz_rel(p, 8); sfx_mana(); }
      else { if (PZ_.ally[p]) { PZ_.ally[p] = 0; sfx_food(); continue; } if (PZ_.rel[p] < 70 || (cmoney != 3 && cash < 500)) { sfx_deny(); say(PZ_.rel[p] < 70 ? "THEY WANT RELATION 70" : "IT COSTS *500"); continue; }
        if (cmoney != 3) cash -= 500; PZ_.ally[p] = 1; PZ_.ally[p ^ 1] = 0; pz_rel((u8)(p ^ 1), -20); PZ_.inv[p] = 0; sfx_mana(); say("ALLIES! THEIR RAIDERS STAY HOME"); }
    }
  }
  ewait_clear();
}
static void pz_pot(void) {
  u8 sel = 0, i;
  static const char *const IT[6] = {"SKIM", "BUY LOYALTY *1000", "HIRE GUARDS *800", "PAY THE REBELS *500", "FUND THE VOTE *2000", "BACK"};
  static const char *const IH[6] = {"PART OF THE TAXES GOES TO YOU", "THE UNHAPPIEST GROUP +15", "COUP DEFENSE +25 (STACKS)", "REBELS -60", "+10 PERCENT NEXT VOTE", ""};
  static const char *const SK[4] = {"NONE", "5 PERCENT", "10 PERCENT", "20 PERCENT"};
  static const u8 SKV[4] = {0, 5, 10, 20};
  ewait_clear();
  for (;;) {
    ov_fill(1, 3, 0, 19); ui_full(); big(0, "THE HONEY POT");
    ps(1, 2, "YOUR SECRET SAVINGS:", 3); pcash(22, 2, (s32)PZ_.stash, 9);
    ps(1, 3, "WHISPERS:", 3); ps(11, 3, PZ_.whis < 20 ? "NONE" : PZ_.whis < 50 ? "SOME" : "LOUD!", PZ_.whis < 20 ? 13 : PZ_.whis < 50 ? 14 : 8);
    pm_rule(4);
    for (i = 0; i < 6; i++) { u8 y = (u8)(5 + i * 2); if (i == sel) BGMAP1[y * 32 + 1] = (u16)((ORN + 5) | (9 << 12)); ps(3, y, IT[i], i == sel ? 9 : 3);
      if (i == 0) { ps(10, y, "<", 10); ps(12, y, SK[PZ_.skim == 0 ? 0 : PZ_.skim == 5 ? 1 : PZ_.skim == 10 ? 2 : 3], 9); ps(23, y, ">", 10); } }
    pm_rule(17); ps(1, 18, IH[sel], 3);
    ps(1, 19, "\1 DO IT   \2 BACK", 10);
    while (!ekey()) {}
    if (emp_ & (J_B | J_START)) break;
    if (emp_ & J_UP) sel = (u8)(sel ? sel - 1 : 5);
    if (emp_ & J_DOWN) sel = (u8)(sel >= 5 ? 0 : sel + 1);
    if (sel == 0 && (emp_ & (J_LEFT | J_RIGHT))) { u8 k = PZ_.skim == 0 ? 0 : PZ_.skim == 5 ? 1 : PZ_.skim == 10 ? 2 : 3; k = (u8)((k + ((emp_ & J_RIGHT) ? 1 : 3)) % 4); PZ_.skim = SKV[k]; sfx_food(); }
    if (emp_ & J_A) {
      u16 c = sel == 1 ? 1000 : sel == 2 ? 800 : sel == 3 ? 500 : sel == 4 ? 2000 : 0;
      if (sel == 5) break;
      if (!c) continue;
      if (PZ_.stash < c) { say("NOT ENOUGH IN THE POT"); sfx_deny(); continue; }
      PZ_.stash -= c; sfx_mana();
      if (sel == 1) { u8 lo = 0; for (i = 1; i < NFAC; i++) if (PZ_.sup[i] < PZ_.sup[lo]) lo = i; pz_add(lo, 15); PZ_.mood[lo] = (s8)(PZ_.mood[lo] + 5); }
      else if (sel == 2) { if (PZ_.guards < 100) PZ_.guards = (u8)(PZ_.guards + 25); }
      else if (sel == 3) { PZ_.reb = (u16)(PZ_.reb > 60 ? PZ_.reb - 60 : 0); }
      else PZ_.vbon = (s8)(PZ_.vbon + 10);
    }
  }
  ewait_clear();
}
static void pz_rebels(void) {
  u8 y = 3;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "ORDER");
  ps(1, y, "REBELS IN THE HILLS", 3); pnum(22, y, PZ_.reb, PZ_.reb < 30 ? 13 : PZ_.reb < 100 ? 14 : 8); y++;
  ps(1, y, "COUP DEFENSE", 3); pnum(22, y, pz_defense(), 9); y++;
  ps(1, y, "SOLDIERS PLOTTING", 3); ps(22, y, PZ_.cwarn >= 3 ? "YES!" : PZ_.cwarn ? "A BIT" : "NO", PZ_.cwarn >= 3 ? 8 : PZ_.cwarn ? 14 : 13); y++;
  ps(1, y, "HIRED GUARDS", 3); pnum(22, y, PZ_.guards, 9); y++;
  ps(1, y, "TIMES YOU FELL", 3); pnum(22, y, PZ_.falls, PZ_.falls ? 8 : 13); y += 2;
  ps(1, y++, "REBELS COME FROM GROUPS UNDER", 10); ps(1, y++, "25 AND LOW FREEDOM. GUARD POSTS", 10); ps(1, y++, "SECRET POLICE AND MARTIAL LAW", 10); ps(1, y++, "THIN THEM. AT 150 THEY RISE UP.", 10);
  y++; ps(1, y++, "COUPS COME WHEN THE SOLDIERS", 10); ps(1, y++, "ARE UNDER 25. GUARD POSTS", 10); ps(1, y++, "LOYALISTS AND HIRED GUARDS", 10); ps(1, y++, "HOLD THE PALACE.", 10);
  ps(1, 19, "\2 BACK", 10);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_B | J_START))) {} ewait_clear();
}
static void pz_advisor(void) {                     // Chitin reads the hidden hand out loud
  u8 y = 3, i, n = 0;
  ov_fill(1, 3, 0, 19); ui_full(); big(0, "CHITIN SAYS");
  for (i = 0; i < NFAC && y < 16; i++) {
    const char *m = 0; u8 c = 8;
    if (PZ_.lead[i] >= 2) continue;
    if (PZ_.sup[i] < 30) {
      m = i == F_WORK ? "WORKERS STRIKE: LESS MONEY" : i == F_TRADE ? "TRADERS HOLD BACK: LAND VALUE" : i == F_MYST ? "PROPHETS SEND ANTS AWAY" : i == F_SOLD ? "THE SOLDIERS ARE PLOTTING!"
        : i == F_SCHOL ? "SCHOLARS LEAVE: SLOW GROWTH" : i == F_GREEN ? "GREENS BURN THE WORKS" : i == F_NAT ? "NATIVES FEED THE CRIME" : "LOYALISTS DOUBT YOU: RESPECT";
    } else if (PZ_.sup[i] >= 75) { c = 13;
      m = i == F_WORK ? "HAPPY WORKERS: MORE MONEY" : i == F_TRADE ? "TRADERS INVEST: LAND VALUE UP" : i == F_MYST ? "MYSTICS BLESS THE HOMES" : i == F_SOLD ? "SOLDIERS KEEP CRIME DOWN"
        : i == F_SCHOL ? "SCHOLARS: ZONES GROW FASTER" : i == F_GREEN ? "GREENS CLEAN THE AIR" : i == F_NAT ? "NATIVES KEEP THE PEACE" : "LOYALISTS SING YOUR PRAISE";
    }
    if (m) { ps(1, y++, m, c); n++; }
  }
  if (!n) ps(1, y++, "THE GROUPS ARE QUIET FOR NOW", 3);
  if (PZ_.reb >= 30) ps(1, y++, "REBELS RAID THE CITY", 8);
  if (PZ_.inv[0] || PZ_.inv[1]) ps(1, y++, "RAIDERS ARE ON THEIR WAY", 8);
  if (PZ_.whis >= 50) ps(1, y++, "EVERYONE WHISPERS ABOUT THE POT", 8);
  pm_rule(16);
  ps(1, 17, "THIS IS WHAT THE GROUPS DO TO", 10); ps(1, 18, "THE CITY. THEY NEVER SAY SO.", 10);
  ps(1, 19, "\2 BACK", 10);
  ewait_clear(); while (!ekey() || !(emp_ & (J_A | J_B | J_START))) {} ewait_clear();
}
static void pz_palace(void) {
  static const char *const IT[9] = {"GROUPS", "LIFE", "EDICTS", "CHARTER", "VOTES", "POWERS", "HONEY POT", "ORDER", "ADVISOR"};
  static const char *const IH[9] = {"THE 8 GROUPS AND THEIR LEADERS", "THE 9 PARTS OF A GOOD LIFE", "15 LAWS TO SWITCH ON AND OFF", "THE 6 RULES OF THE COLONY",
    "SPEECHES PROMISES RIGGING", "THE RED EMPIRE AND THE HIVE", "YOUR SECRET SAVINGS", "REBELS AND COUPS", "WHAT THE GROUPS REALLY DO"};
  static u8 sel = 0;
  u8 r;
  for (;;) {
    r = grid_menu("PALACE", IT, IH, 9, &sel);
    if (r == 255) break;
    switch (r) {
    case 0: pz_groups(); break;
    case 1: pz_happy_screen(); break;
    case 2: pz_edicts(); break;
    case 3: pz_charter(); break;
    case 4: pz_votes(); if (PZ_.pend & PZ_VOTE) return; break;
    case 5: pz_powers(); break;
    case 6: pz_pot(); break;
    case 7: pz_rebels(); break;
    default: pz_advisor(); break;
    }
  }
}
static void pz_hud(void) {                         // row 18, right end: respect, and the vote countdown
  if (!PZ_.on || mp_on) return;
  pc(26, 18, '^', PZ_.resp >= 50 ? 13 : 8); pn(27, 18, PZ_.resp > 99 ? 99 : PZ_.resp, PZ_.resp >= 50 ? 13 : 8);
}
