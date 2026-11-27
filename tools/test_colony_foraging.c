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
  for (unsigned attack=0; attack<2; ++attack) {
    const int32_t unseen = colony_nest_score(UINT16_MAX,0,(uint8_t)attack,31);
    for (unsigned d=0; d<65535u; ++d) {
      int32_t low = colony_nest_score((uint16_t)d,0,(uint8_t)attack,0);
      int32_t high = colony_nest_score((uint16_t)d,0,(uint8_t)attack,7);
      assert(low == -(int32_t)(attack ? 14 : 12) * (int32_t)d);
      assert(high == low+7 && low > unseen);
      if (d) assert(colony_nest_score((uint16_t)(d-1),0,(uint8_t)attack,0)>high);
      if (d <= 2000) assert(high == -(int32_t)(attack ? 14 : 12)*(int32_t)d+7);
    }
    assert(colony_nest_score(UINT16_MAX,1,(uint8_t)attack,0) >
           colony_nest_score(UINT16_MAX,3,(uint8_t)attack,0));
  }
  printf("GBA foraging: %u scent/roll/distance/backtrack combinations PASS; full uint16 route range PASS\n",cases);
  return 0;
}
