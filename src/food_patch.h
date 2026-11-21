/* Deterministic food-patch selection for hand-made maps.
   The cartridge has only 16 patch centers. Preserve every authored
   location on small maps (<=16). On dense maps, first select one
   patch from each occupied 4x4 world sector, then fill remaining
   slots from other distinct candidates. Prevents the top-left
   16 patches from starving the rest of a 96x96 custom world.
   No heap, RNG, extra persistent buffers or per-frame work. */
#ifndef ANTOPOLIS_GBA_FOOD_PATCH_H
#define ANTOPOLIS_GBA_FOOD_PATCH_H
#include <stdint.h>
#define FOOD_PATCH_LIMIT 16u

static uint8_t food_patch_pick(const char *cells, uint8_t width,
                               uint8_t height, uint8_t *px,
                               uint8_t *py) {
  uint8_t x,y,n=0,i;
  uint16_t total=0, sectors=0, bit;
  if (width < 1u || height < 1u) return 0;
  for(y=0;y<height;++y) for(x=0;x<width;++x)
    if(cells[(uint16_t)y*width+x]=='f') ++total;

  if(total<=FOOD_PATCH_LIMIT) {
    for(y=0;y<height;++y) for(x=0;x<width;++x)
      if(cells[(uint16_t)y*width+x]=='f') {
        px[n]=x; py[n]=y; ++n;
      }
    return n;
  }
  /* One candidate per world sector, preserving the 4x4 atlas and
     respecting non-square maps (32x96, 96x32, etc.). */
  for(y=0;y<height;++y) for(x=0;x<width;++x)
    if(cells[(uint16_t)y*width+x]=='f') {
      uint8_t sector=(uint8_t)(((uint16_t)y*4u/height)*4u+
                                ((uint16_t)x*4u/width));
      bit=(uint16_t)(1u<<sector);
      if(!(sectors&bit)) {
        sectors|=bit; px[n]=x; py[n]=y;
        if(++n==FOOD_PATCH_LIMIT) return n;
      }
    }
  /* Fill still-open slots, excluding the already selected coordinates.
     This also handles worlds whose berries occupy only one sector. */
  for(y=0;y<height;++y) for(x=0;x<width;++x)
    if(cells[(uint16_t)y*width+x]=='f') {
      for(i=0;i<n;++i) if(px[i]==x && py[i]==y) break;
      if(i==n) {
        px[n]=x; py[n]=y;
        if(++n==FOOD_PATCH_LIMIT) return n;
      }
    }
  return n;
}
#endif
