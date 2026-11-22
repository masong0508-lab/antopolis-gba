/* Map Maker resize invariant: keep the two queen starts distinct.
   A smaller world can crop BOTH authored nests to the same former
   bottom/right edge. The old logic silently left two queens on one cell,
   making path checks report a false connection and breaking gameplay.

   Called only when map dimensions change; no heap, random calls, arrays,
   or simulation-frame overhead. The returned flag reports relocation. */
#ifndef ANTOPOLIS_EDITOR_NEST_H
#define ANTOPOLIS_EDITOR_NEST_H
#include <stdint.h>

static uint8_t editor_nests_fit(uint8_t width, uint8_t height,
                                uint8_t x[2], uint8_t y[2]) {
  uint8_t i, changed = 0;
  for (i = 0; i < 2u; ++i) {
    if (x[i] >= width) { x[i] = (uint8_t)(width - 5u); changed = 1; }
    if (y[i] >= height) { y[i] = (uint8_t)(height - 5u); changed = 1; }
  }
  if (x[0] == x[1] && y[0] == y[1]) {
    /* Move only the second queen, four cells along X, rather than
       stacking two nests or scattering both to arbitrary corners.
       Every supported world is >=32 wide, so one offset is in bounds. */
    if (x[1] >= 8u) x[1] = (uint8_t)(x[1] - 4u);
    else x[1] = (uint8_t)(x[1] + 4u);
    changed = 1;
  }
  return changed;
}
#endif
