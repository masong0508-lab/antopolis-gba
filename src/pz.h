// ============================================================================================================================
//  EL PRESIDANTE: the shared state (included from empire.c). The engine itself is pz.c (included from city.c).
// ============================================================================================================================
#define NFAC 8                                     // the 8 groups (Tropico's factions)
#define NH 9                                       // the 9 happiness parts
#define NE 15                                      // edicts
enum { F_WORK, F_TRADE, F_MYST, F_SOLD, F_SCHOL, F_GREEN, F_NAT, F_LOYAL };
enum { H_FOOD, H_HOME, H_HEALTH, H_FAITH, H_FUN, H_SAFE, H_JOBS, H_FREE, H_MONEY };
enum { E_FOOD, E_LIT, E_MARTIAL, E_FEST, E_PROHIB, E_SECRET, E_PROPAG, E_TAXRICH, E_OPEN, E_CLOSED, E_GREEN, E_PENSION, E_CRACK, E_CULT, E_AMNESTY };
enum { PZ_VOTE = 1, PZ_COUP = 2, PZ_EVENT = 4, PZ_RISE = 8, PZ_INV = 16 };   // cards waiting for the play loop
typedef struct {
  u8 on;
  u8 sup[NFAC], siz[NFAC], lead[NFAC], ltm[NFAC], hap[NH];   // support 0-100, share of the colony, leader (0 free 1 bribed 2 jailed 3 exiled), its timer
  s8 mood[NFAC];                                   // promises kept or broken, scandals: fades every month
  u8 resp, lib, cwarn, falls, sp;                  // respect, liberty, coup plotting, times you fell, superpower trouble
  u16 reb;                                         // rebels in the hills
  u8 rel[2], inv[2], deal[2], ally[2], askt[2];    // superpowers: relation, invasion countdown, trade deal, alliance, months until aid again
  u16 ed; u8 fest, edmon;                          // edicts on (bits), festival months left, edict costs last month
  u8 con[6], conc;                                 // the charter (constitution) and months until it can change again
  u8 elt, spk, rig, prom, promf, wins, lastv; s8 vbon;   // months to the vote, speech, rigged, a promise (part + group), votes won, last result
  u8 skim, whis, guards; u32 stash;                // the honey pot: skim percent, suspicion, hired guards
  u8 pend, ev, evc, msgm;                          // cards waiting, the event, months since the last event, months since the last tip
  s8 m_inc, m_dr, m_grow, m_crime, m_smog, m_lv, m_resp;   // the hidden hand: what the groups did to the city this month
} PZ;
static PZ PZ_;
static u8 pzw;                                     // 0 follow the option, 1 on, 2 off (the setup row; sent over the link)
static u8 pz_mart(void); static u8 pz_ed(u8 e);
static void pz_setup(u8 scen); static void pz_month(void); static u8 pz_cards(void); static void pz_palace(void); static void pz_hud(void);
