/* Portable regression for the food/water rule used by both cartridges.
   Compile with -std=c99 -Wall -Wextra -Werror -pedantic. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/terrain_food.h"

int main(void) {
  uint8_t height, berries, first, second, lost;
  for (height = 0; height < 4; ++height) {
    for (berries = 0; berries < 2; ++berries) {
      first = (uint8_t)TERRAIN_FOOD_AFTER(height, berries);
      lost = (uint8_t)TERRAIN_FOOD_DROWNED(height, berries);
      assert(first == (height ? berries : 0u));
      assert(lost == ((!height && berries) ? 1u : 0u));
      assert(!(first && !height));
      /* Loading old saves and reapplying the rule is idempotent. */
      second = (uint8_t)TERRAIN_FOOD_AFTER(height, first);
      assert(first == second);
    }
  }
  /* Every sinking transition, including a no-op from existing water. */
  for (height = 0; height < 4; ++height) {
    uint8_t next = height ? (uint8_t)(height - 1) : 0u;
    for (berries = 0; berries < 2; ++berries) {
      first = (uint8_t)TERRAIN_FOOD_AFTER(next, berries);
      assert(first == (next ? berries : 0u));
      assert((uint8_t)TERRAIN_FOOD_DROWNED(next, berries) ==
             (uint8_t)(height <= 1 && berries != 0));
    }
  }
  /* Rising dry land must never resurrect berries that water removed. */
  assert(TERRAIN_FOOD_AFTER(1, TERRAIN_FOOD_AFTER(0, 1)) == 0);
  assert(TERRAIN_FOOD_AFTER(3, 1) == 1);
  puts("Terrain water/food invariant and save normalization PASS");
  return 0;
}
