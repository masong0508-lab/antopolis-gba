/* Host test of the exact water-shimmer helper compiled into GBA.
   Checks all 256 8-bit frame phases, palettes and 3D/map coherence. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/ambient_water.h"
static unsigned luma(uint16_t c) {
  unsigned r=(unsigned)c&31u,g=((unsigned)c>>5)&31u,b=((unsigned)c>>10)&31u;
  return r+2u*g+b;
}
int main(void) {
  uint16_t prev[4],seen[4][4];
  unsigned f,slot,p,variations;
  for(slot=0;slot<4;slot++) {
    variations=0;
    for(p=0;p<4;p++) {
      uint16_t c=ant_water_shimmer((uint8_t)(p*16), (uint8_t)slot);
      assert((c&0x8000u)==0u);
      assert(luma(c)>=55u&&luma(c)<=119u);
      seen[slot][p]=c;
      for(unsigned t=0;t<16;t++)
        assert(ant_water_shimmer((uint8_t)(p*16+t),(uint8_t)slot)==c);
      if(p && c!=seen[slot][p-1])variations++;
    }
    assert(variations>=2u);
    prev[slot]=seen[slot][3];
  }
  (void)prev;
  for(f=0;f<256;f++) {
    for(slot=0;slot<4;slot++) {
      assert(ant_water_shimmer((uint8_t)f,(uint8_t)slot)==
             seen[slot][(f>>4)&3u]);
      assert(ant_water_shimmer((uint8_t)f,(uint8_t)(slot+4))==
             ant_water_shimmer((uint8_t)f,(uint8_t)slot));
    }
  }
  // Even during animation the two overhead tones never become equal:
  // the surface's dark/light contrast remains readable in both modes.
  for(p=0;p<4;p++) {
    assert(seen[0][p]!=seen[1][p]);
    assert(seen[2][p]!=seen[3][p]);
  }
  puts("GBA environmental water PASS: 4-phase 2D/3D palette animation, no new tiles");
  return 0;
}
