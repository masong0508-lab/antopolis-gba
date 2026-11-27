#ifndef ANTOPOLIS_COLONY_GROWTH_H
#define ANTOPOLIS_COLONY_GROWTH_H
#include <stdint.h>
/* One reward per newly crossed living population threshold, even
   when multiple hatches occur between colony ticks. */
static inline uint8_t colony_growth_bonus(uint8_t previous_peak, uint8_t current) {
  uint8_t bonus = 0;
  if (previous_peak < 8u && current >= 8u) bonus += 3u;
  if (previous_peak < 16u && current >= 16u) bonus += 4u;
  if (previous_peak < 24u && current >= 24u) bonus += 5u;
  return bonus;
}
#endif
