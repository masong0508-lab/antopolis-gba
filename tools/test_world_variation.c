/* Exact shipping GBA tile-map flip rule, exhaustively tested across
   every supported 96x96 world and 81 cliff/shore edge combinations. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/world_variation.h"
int main(void) {
  unsigned tile_counts[4]={0,0,0,0};
  uint8_t x,y,h,e;
  for(h=0;h<4;h++)
    for(y=0;y<96;y++)
      for(x=0;x<96;x++)
        for(e=0;e<81;e++) {
          uint16_t flip=world_interior_flip(h,e,x,y);
          assert(!(flip&~0x0C00u)); // 10/11, no tile ID or palette mutation
          if(!h||e) assert(flip==0u);
          else tile_counts[flip>>10u]++;
          // Position must not depend on frame number or mutable RNG.
          assert(flip==world_interior_flip(h,e,x,y));
        }
  for(unsigned i=0;i<4;i++) assert(tile_counts[i]>4000u);
  assert(world_interior_flip(2,1,23,17)==0); // no mirrored shore
  assert(world_interior_flip(3,2,23,17)==0); // no mirrored cliff
  assert(world_interior_flip(0,0,23,17)==0); // water bands stay oriented
  printf("GBA terrain flip PASS: 3 heights x 96x96, all 81 edge codes, four interior orientations\n");
  return 0;
}
