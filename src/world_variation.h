/* Overhead terrain texture variety using GBA hardware tile flips.
   Zero-edge interior cells can safely mirror their natural grain/tufts.
   Never flip directional shoreline or cliff autotiles: that would draw
   false physical geometry, obstructing navigation. Pure function, no
   map buffer, no extra tile atlas, no RNG or multiplayer divergence. */
#ifndef ANTOPOLIS_WORLD_VARIATION_H
#define ANTOPOLIS_WORLD_VARIATION_H
#include <stdint.h>
static uint16_t world_interior_flip(uint8_t height, uint8_t edge_code,
                                    uint8_t x, uint8_t y) {
  uint8_t v;
  if(!height || edge_code) return 0u;
  v=(uint8_t)(x*17u+y*29u+(((uint16_t)x*y)>>2)+(x ^ (y<<1)));
  return (uint16_t)(((v&1u)?0x0400u:0u)|((v&2u)?0x0800u:0u));
}
#endif
