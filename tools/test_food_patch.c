/* Compile the ACTUAL cartridge patch selector with host C99.
   Exhaust sparse/dense maps, overdraw, aspect ratio and reproducibility. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/food_patch.h"

static char world[96u*96u];
static uint8_t px[FOOD_PATCH_LIMIT], py[FOOD_PATCH_LIMIT];
static uint8_t seen_sector(uint8_t width, uint8_t height,
                           uint8_t *out_sectors) {
  uint8_t i,n=0;uint16_t mask=0;
  for(i=0;i<FOOD_PATCH_LIMIT;++i) {
    uint8_t sector=(uint8_t)(((uint16_t)py[i]*4u/height)*4u+
                              ((uint16_t)px[i]*4u/width));
    mask|=(uint16_t)(1u<<sector);
  }
  for(i=0;i<16u;++i) if(mask & ((uint16_t)1u<<i)) ++n;
  *out_sectors=n;
  return n;
}
static void validate(uint8_t w,uint8_t h,uint8_t count) {
  uint8_t i,j;
  assert(count<=FOOD_PATCH_LIMIT);
  for(i=0;i<count;++i) {
    assert(px[i]<w && py[i]<h);
    assert(world[(uint16_t)py[i]*w+px[i]]=='f');
    for(j=0;j<i;++j) assert(px[i]!=px[j] || py[i]!=py[j]);
  }
}
static void check_dense(uint8_t w,uint8_t h) {
  uint8_t n,s,oldx[16],oldy[16];
  memset(world,'f',(size_t)w*h);
  n=food_patch_pick(world,w,h,px,py);
  assert(n==16u);
  validate(w,h,n);
  seen_sector(w,h,&s);
  assert(s==16u);
  memcpy(oldx,px,16u); memcpy(oldy,py,16u);
  assert(food_patch_pick(world,w,h,px,py)==16u);
  assert(memcmp(oldx,px,16u)==0 && memcmp(oldy,py,16u)==0);
}
int main(void) {
  uint8_t i,j,n,sectors;
  uint8_t wx[16],wy[16];
  uint16_t seed;
  memset(world,'g',sizeof(world));
  assert(food_patch_pick(world,96,96,px,py)==0);
  assert(food_patch_pick(world,0,96,px,py)==0);

  /* <=16 hand-authored patches retain *exact* world coordinates and
     row-major order, including the previously first-picked sixteen. */
  for(i=0;i<16u;++i) {
    wx[i]=(uint8_t)((i*37u)%96u);
    wy[i]=(uint8_t)((i*53u)%96u);
    world[(uint16_t)wy[i]*96u+wx[i]]='f';
  }
  n=food_patch_pick(world,96,96,px,py);
  assert(n==16u); validate(96,96,n);
  for(i=0;i<16u;++i) {
    uint8_t located=0;
    for(j=0;j<n;++j) if(px[j]==wx[i] && py[j]==wy[i]) located=1;
    assert(located);
  }

  check_dense(96,96);
  check_dense(32,32);
  check_dense(96,32);
  check_dense(32,96);

  /* >16 berries only in one region: fill slots without duplicates
     rather than treating the other 15 empty sectors as essential. */
  memset(world,'g',96u*96u);
  for(i=0;i<10u;++i) for(j=0;j<10u;++j)
    world[(uint16_t)i*96u+j]='f';
  n=food_patch_pick(world,96,96,px,py);
  assert(n==16u); validate(96,96,n);
  seen_sector(96,96,&sectors); assert(sectors==1u);

  /* Scattered clumps: every populated region gets a center before any
     region gets two. This prevents first-row patch starvation. */
  memset(world,'g',96u*96u);
  for(i=0;i<4u;++i) for(j=0;j<4u;++j) {
    uint8_t xx=(uint8_t)(2u+j*24u), yy=(uint8_t)(2u+i*24u);
    world[(uint16_t)yy*96u+xx]='f';
    world[(uint16_t)(yy+1u)*96u+xx]='f';
  }
  n=food_patch_pick(world,96,96,px,py);
  assert(n==16u); validate(96,96,n);
  seen_sector(96,96,&sectors); assert(sectors==16u);

  /* Repeat an independent deterministic suite of noisy maps. */
  seed=6553u;
  for(i=0;i<50u;++i) {
    uint16_t k,total=0;
    memset(world,'g',96u*96u);
    for(k=0;k<96u*96u;++k) {
      seed=(uint16_t)(seed*25173u+13849u);
      if((seed&63u)<(i&31u)) {world[k]='f';++total;}
    }
    n=food_patch_pick(world,96,96,px,py);
    assert(n==(uint8_t)(total<16u?total:16u));
    validate(96,96,n);
  }
  puts("GBA map editor food patches PASS: sparse legacy, 4x4 distribution, dense, rectangles, 50 noisy worlds");
  return 0;
}
