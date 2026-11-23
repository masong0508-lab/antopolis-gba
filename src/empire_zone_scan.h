/* Bounded automatic-zone cursor for Empire/ANTVERSE.
 * Advance in tile order without random misses. An economy cycle reads at
 * most 300 cells, and successive cycles eventually visit all map cells.
 * No map allocation, heap, per-frame work or save-format change. */
#ifndef ANTOPOLIS_EMPIRE_ZONE_SCAN_H
#define ANTOPOLIS_EMPIRE_ZONE_SCAN_H
#include <stdint.h>
static void empire_zone_next(uint8_t *x, uint8_t *y,
                             uint8_t width, uint8_t height) {
  if (++*x >= width) {
    *x = 0;
    if (++*y >= height) *y = 0;
  }
}
#endif
