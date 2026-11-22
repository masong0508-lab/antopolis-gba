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
/* An authored 'f' is an actual starting berry, not just a random
   scatter hint. The 16 selected centers still control future regrowth,
   but even the 17th+ painted berry appears exactly where it was placed.
   This is evaluated in the existing world-import tile loop only. */
static uint8_t food_patch_authored_tile(char symbol, uint8_t height) {
  return (uint8_t)(symbol == 'f' && height != 0u);
}
/* Choose usable starting forage on the ring two tiles from a nest.
   Procedural generation flattens this 5x5 area to traversable land.
   Unlike retrying random coordinates, an exhausted random budget cannot
   silently create a patch centered in open water or a sheer cliff.
   Scanned only when making a world; no heap and no extra RNG calls. */
static uint8_t food_start_cell(const uint8_t *heights,
                               uint8_t stride, uint8_t width,
                               uint8_t height, uint8_t nx,
                               uint8_t ny, uint8_t rivalx,
                               uint8_t rivaly, int16_t x,
                               int16_t y) {
  uint8_t h,nh;
  int16_t dx,dy;
  if (x < 0 || y < 0 || x >= width || y >= height) return 0;
  dx = x - nx; dy = y - ny;
  if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) return 0;
  dx = x - rivalx; dy = y - rivaly;
  if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) return 0;
  h = heights[(uint16_t)y * stride + (uint16_t)x];
  nh = heights[(uint16_t)ny * stride + nx];
  return h && nh && h <= nh + 1u && nh <= h + 1u;
}
static uint8_t food_start_anchor(const uint8_t *heights,
                                 uint8_t stride, uint8_t width,
                                 uint8_t height, uint8_t nx,
                                 uint8_t ny, uint8_t rivalx,
                                 uint8_t rivaly, uint8_t *ox,
                                 uint8_t *oy) {
  int16_t x,y;
  int8_t dx,dy;
  /* Prefer the inward diagonal, where players can see and collect food
     early, but do not assume that coordinate is above water. */
  x = (int16_t)nx + (nx < width / 2u ? 2 : -2);
  y = (int16_t)ny + (ny < height / 2u ? 2 : -2);
  if (food_start_cell(heights,stride,width,height,nx,ny,
                      rivalx,rivaly,x,y)) {
    *ox=(uint8_t)x; *oy=(uint8_t)y;
    return 1;
  }
  for (dy=-2;dy<=2;++dy) for (dx=-2;dx<=2;++dx) {
    if (dx!=-2 && dx!=2 && dy!=-2 && dy!=2) continue;
    x=(int16_t)nx+dx; y=(int16_t)ny+dy;
    if (food_start_cell(heights,stride,width,height,nx,ny,
                        rivalx,rivaly,x,y)) {
      *ox=(uint8_t)x; *oy=(uint8_t)y;
      return 1;
    }
  }
  return 0;
}

#endif
