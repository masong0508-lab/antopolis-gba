/* A protected tutorial/sandbox queen may be bruised but never slain.
   This guards the DAMAGE operation itself, not merely a later match-end
   check. Real versus and non-practice battles keep their existing rules.
   Shared by the normal colony simulation and ANT EYE first person. */
#ifndef ANTOPOLIS_PRACTICE_QUEEN_H
#define ANTOPOLIS_PRACTICE_QUEEN_H
#include <stdint.h>
static uint8_t practice_queen_can_lose_hp(uint8_t hp,
                                         uint8_t sandbox,
                                         uint8_t tutorial,
                                         uint8_t versus) {
  if (!hp) return 0;  /* no underflow on a dead queen */
  return (uint8_t)(hp > 1u || versus || !(sandbox || tutorial));
}
#endif
