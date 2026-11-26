#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/colony_foraging.h"
int main(void) {
  unsigned cases = 0;
  const int16_t max_empty = colony_forager_score(255,0,31,12,0);
  assert(max_empty == 310);
  for (unsigned scent=0; scent<256; ++scent)
    for (unsigned roll=0; roll<32; ++roll)
      for (unsigned i=0; i<15; ++i)
        for (unsigned back=0; back<2; ++back) {
          uint16_t d = i==14 ? UINT16_MAX : (uint16_t)i;
          int16_t old = (int16_t)((int)scent+(int)roll+
              (d==UINT16_MAX ? 0 : d<12 ? 2*(int)d : 24)-
              (back ? 24 : 0));
          int16_t empty = colony_forager_score((uint8_t)scent,0,(uint8_t)roll,d,(uint8_t)back);
          int16_t berry = colony_forager_score((uint8_t)scent,1,(uint8_t)roll,d,(uint8_t)back);
          assert(empty==old && empty<=max_empty);
          assert(berry==old+336 && berry>max_empty);
          ++cases;
        }
  printf("GBA foraging: %u scent/roll/distance/backtrack combinations PASS\n",cases);
  return 0;
}
