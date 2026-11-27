/* GBA Classic colony steering. Keep the existing trail, home-distance
   and backtracking scores, but never overlook a legal adjacent berry.
   Worst empty score = 255+31+24=310; worst berry = 336-24=312. */
#ifndef ANTOPOLIS_COLONY_FORAGING_H
#define ANTOPOLIS_COLONY_FORAGING_H
#include <stdint.h>
static inline int16_t colony_forager_score(uint8_t scent, uint8_t berry,
    uint8_t roll, uint16_t home_distance, uint8_t backtrack) {
  int16_t bias = home_distance == UINT16_MAX ? 0 :
      (int16_t)(home_distance < 12u ? home_distance : 12u) * 2;
  return (int16_t)((int16_t)scent + (int16_t)(roll & 31u) +
      (berry ? 336 : 0) + bias - (backtrack ? 24 : 0));
}
/* Use full BFS distance, not a saturated 2000-step proxy. Even the
   theoretical uint16_t finite range fits int32_t. Every known route
   outranks the unreachable fallback, whose Manhattan argument is <=190. */
static inline int32_t colony_nest_score(uint16_t distance,
    uint16_t manhattan, uint8_t attack, uint8_t roll) {
  if (distance == UINT16_MAX)
    return -1000000L - (int32_t)(attack ? 14 : 10) * manhattan +
           (roll & (attack ? 31u : 15u));
  return -(int32_t)(attack ? 14 : 12) * distance + (roll & 7u);
}
#endif
