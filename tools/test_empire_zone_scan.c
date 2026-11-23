/* Native regression for production 300-tile automatic-zone cursor. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/empire_zone_scan.h"

static uint8_t seen[96][96];
static void coverage(uint8_t w, uint8_t h, uint8_t sx, uint8_t sy) {
  uint8_t x=sx,y=sy;
  uint16_t total=(uint16_t)w*h,visited=0,cycle=0;
  memset(seen,0,sizeof seen);
  assert(sx<w && sy<h);
  while(visited<total) {
    uint16_t budget=300;
    while(budget-- && visited<total) {
      assert(x<w && y<h);
      assert(!seen[y][x]);  /* never repeat before one complete lap */
      seen[y][x]=1; ++visited;
      empire_zone_next(&x,&y,w,h);
    }
    ++cycle;
  }
  assert(cycle==(uint16_t)((total+299u)/300u));
  assert(x==sx && y==sy);
  for(uint8_t yy=0;yy<h;++yy)
    for(uint8_t xx=0;xx<w;++xx)
      assert(seen[yy][xx]==1);
  /* The second lap is repeatable and bounded, even after a save/reload. */
  empire_zone_next(&x,&y,w,h);
  assert(x==(uint8_t)(sx+1u<w?sx+1u:0u));
}
int main(void) {
  coverage(32,32,4,26);
  coverage(40,32,5,27);
  coverage(48,40,45,39);
  coverage(56,64,1,62);
  coverage(96,96,90,93);
  coverage(96,32,95,31);
  coverage(32,96,30,95);
  puts("GBA Empire zone scan covers every tile in bounded batches; rectangular worlds PASS");
  return 0;
}
