/* Host-exercise the shipping ANT EYE 14-tile 8bpp floor generator.
   No emulator needed to catch malformed palette indices, blank flora,
   stone masks, or disappearing nest holes. */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "../src/eye_floor_detail.h"
int main(void) {
  uint8_t t,x,y,c,seen[24];
  unsigned colored=0;
  for(t=0;t<14u;t++) {
    unsigned variety=0;
    for(unsigned k=0;k<24u;k++)seen[k]=0;
    for(y=0;y<8u;y++)for(x=0;x<8u;x++) {
      c=eye_floor_pixel(t,x,y);
      assert(c>=96u&&c<=119u);
      assert(c==eye_floor_pixel(t,x,y));
      if(t<2u) assert(c<=98u);
      else if(t<5u) assert(c>=99u&&c<=102u);
      else if(t<9u) assert(c>=103u&&c<=108u);
      else if(t<12u) assert(c>=109u&&c<=113u);
      else assert(c>=114u&&c<=117u);
      seen[c-96u]=1;
    }
    for(unsigned k=0;k<24u;k++) variety+=seen[k];
    assert(variety>=2u);
    colored+=variety;
  }
  // Distinct features must be visible as actual pixel colors, not just
  // comments that describe environmental detail.
  assert(eye_floor_pixel(6,2,5)==106u);  // grass blades
  assert(eye_floor_pixel(6,3,4)==106u);
  assert(eye_floor_pixel(8,2,2)==107u);  // flowers
  assert(eye_floor_pixel(8,6,1)==108u);
  assert(eye_floor_pixel(10,3,3)==113u); // sunlit rock
  assert(eye_floor_pixel(10,3,4)==112u); // shaded stone
  assert(eye_floor_pixel(11,5,3)==113u); // stratified fissure
  assert(eye_floor_pixel(11,5,4)==112u);
  assert(eye_floor_pixel(13,3,3)==117u); // black nest burrow
  assert(eye_floor_pixel(13,2,3)==116u); // rim contrast
  assert(colored>=35u);
  puts("GBA ANT EYE floor PASS: 896 bounded pixels, ripples, plants, stone strata and burrow");
  return 0;
}
