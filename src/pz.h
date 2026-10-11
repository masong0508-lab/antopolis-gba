// ============================================================================================================================
//  EL PRESIDANTE: the shared state (included from empire.c). The engine itself is pz.c (included from city.c).
// ============================================================================================================================
#define NFAC 8                                     // the 8 groups (Tropico's factions)
#define NH 9                                       // the 9 happiness parts
#define NE 15                                      // edicts
enum { F_WORK, F_TRADE, F_MYST, F_SOLD, F_SCHOL, F_GREEN, F_NAT, F_LOYAL };
enum { H_FOOD, H_HOME, H_HEALTH, H_FAITH, H_FUN, H_SAFE, H_JOBS, H_FREE, H_MONEY };
enum { E_FOOD, E_LIT, E_MARTIAL, E_FEST, E_PROHIB, E_SECRET, E_PROPAG, E_TAXRICH, E_OPEN, E_CLOSED, E_GREEN, E_PENSION, E_CRACK, E_CULT, E_AMNESTY };
enum { PZ_VOTE = 1, PZ_COUP = 2, PZ_EVENT = 4, PZ_EXILE = 8 };   // cards waiting for the play loop
typedef struct {
  u8 on, inited;
  u8 sup[NFAC], siz[NFAC], lead[NFAC], ltm[NFAC], brb[NFAC], hap[NH];   // support 0-100, share of the colony, leader (0 free 1 bribed 2 jailed 3 gone), its timer, bribe months
  s8 mood[NFAC];                                   // promises kept or broken, scandals: fades a little every month
  u8 resp, lib, reb, coup, cwarn;                  // respect, liberty, rebels, coup plotting
  u8 rel[2], inv[2], deal[2], ally[2], askt[2];    // superpowers: relation, invasion countdown, trade deal, alliance, months until aid can be asked again
  s8 pmood[2];                                     // gifts and insults (fade)
  u16 ed; u8 fest;                                 // edicts on (bits), festival months left
  u8 con[6], conc;                                 // the charter (constitution) and months until it can change
  u8 elt, spk, rig, prom, wins, lastv; s8 vbon;    // months to the vote, speech given, rigged, a promise, votes won, last result
  u8 skim; u32 stash; u16 skimmed, paid;           // the secret honey pot; palace costs last month
  u8 pend, ev, evc;                                // cards waiting, the event, months since the last event
  u8 whis, strike, mine, base, sp_known, lib_hit, falls, refuge, library, armyt, armyw;
  s8 m_work, m_farm, m_crime, m_smog, m_lv, m_dr, m_grow; u8 m_guard;   // the hidden hand, for next month
} PZ;
static PZ pz;
static void pz_add(u8 f, s8 d); static void pz_all(s8 d); static u8 pz_nest_fall(u8 team);
