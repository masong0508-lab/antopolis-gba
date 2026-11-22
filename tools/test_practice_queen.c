/* Test the exact queen-hit eligibility used by both production GBA modes.
   Run every initial heart count and every tutorial/sandbox/versus flag.
   Protect practice's final heart without affecting ordinary victories. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/practice_queen.h"

int main(void) {
  uint16_t cases = 0u;
  uint8_t hp, sandbox, tutorial, versus;
  for (hp = 0; hp < 16u; ++hp) {
    for (sandbox = 0; sandbox < 2u; ++sandbox) {
      for (tutorial = 0; tutorial < 2u; ++tutorial) {
        for (versus = 0; versus < 2u; ++versus) {
          uint8_t expected = (uint8_t)(hp &&
              (hp > 1u || versus || !(sandbox || tutorial)));
          uint8_t remaining = hp;
          uint8_t i, strikes = 0u;
          assert(practice_queen_can_lose_hp(hp,sandbox,tutorial,versus)
                 == expected);
          for (i = 0; i < 20u; ++i) {
            if (practice_queen_can_lose_hp(remaining,sandbox,tutorial,versus)) {
              --remaining; ++strikes;
            }
          }
          if ((sandbox || tutorial) && !versus) {
            assert(remaining == (hp ? 1u : 0u));
            assert(strikes == (hp ? hp-1u : 0u));
          } else {
            assert(remaining == 0u);
            assert(strikes == hp);
          }
          assert(!practice_queen_can_lose_hp(0u,sandbox,tutorial,versus));
          ++cases;
        }
      }
    }
  }
  assert(cases == 128u);
  printf("PASS: %u queen-hit flag combinations; repeated practice/combat hits\n",
         (unsigned)cases);
  return 0;
}
