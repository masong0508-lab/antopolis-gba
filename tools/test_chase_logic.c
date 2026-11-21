#include <assert.h>
#include <stdio.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
#include "../src/chase_logic.h"
int main(void) {
  // Close pursuit must be meaningful, even against concealed runners.
  assert(chase_revealed(0, 0, 1, 0));
  assert(chase_revealed(0, 0, 1, 2));
  assert(!chase_revealed(0, 0, 1, 3));
  // A scan exposes a running mouse but not a concealed one at range.
  assert(chase_revealed(1, 0, 0, 40));
  assert(!chase_revealed(1, 0, 1, 40));
  assert(chase_revealed(1, 0, 1, 3));
  // The carried berry betrays a runner, except in tall cover.
  assert(chase_revealed(0, 1, 0, 40));
  assert(!chase_revealed(0, 1, 1, 40));
  assert(chase_revealed(0, 1, 1, 3));
  assert(!chase_revealed(0, 0, 0, 40));
  // A refill visits all eight phases: no reachable position can be
  // permanently excluded by the placement pattern.
  for (u16 x = 0; x < 96; x++)
    for (u16 y = 0; y < 96; y++) {
      u8 bucket = chase_berry_bucket((u8)x, (u8)y);
      assert(bucket < 8);
      assert(bucket == (u8)((x * 17u + y * 29u) & 7u));
    }
  // First-person fractional movement should not snap to tile edges on
  // the hunter's screen. 8.8-cell coordinates convert to 8px tiles.
  assert(chase_view_px(0) == 0);
  assert(chase_view_px(256) == 8);
  assert(chase_view_px(511) == 15);
  assert(chase_view_px(5 * 256 + 128) == 44);
  puts("CAT & MOUSE visibility, supply and movement tests passed");
  return 0;
}
