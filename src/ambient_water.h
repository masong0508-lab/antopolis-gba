/* Antopolis GBA environmental water light: four gentle palette phases.
   Slots: 0/1 = overhead shoreline tones; 2/3 = ANT EYE floor ripples.
   This is a pure 32-byte lookup using the same four palette entries
   the two views already animate. No VRAM tiles, scanline HBlank work,
   framebuffer, RNG draws or extra persistent RAM. */
#ifndef ANTOPOLIS_AMBIENT_WATER_H
#define ANTOPOLIS_AMBIENT_WATER_H
#include <stdint.h>
#define AW_RGB(r,g,b) ((uint16_t)((r) | ((g)<<5) | ((b)<<10)))
static uint16_t ant_water_shimmer(uint8_t frame, uint8_t slot) {
  static const uint16_t shades[4][4] = {
    {AW_RGB(14,22,31), AW_RGB(10,18,30), AW_RGB(12,20,30), AW_RGB(22,27,31)},
    {AW_RGB(12,21,31), AW_RGB(12,20,30), AW_RGB(11,19,30), AW_RGB(20,26,31)},
    {AW_RGB(10,18,30), AW_RGB(14,22,31), AW_RGB(9,17,29),  AW_RGB(17,23,30)},
    {AW_RGB(12,20,30), AW_RGB(12,21,31), AW_RGB(11,19,30), AW_RGB(20,26,31)}
  };
  return shades[(frame >> 4) & 3u][slot & 3u];
}
#undef AW_RGB
#endif
