#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "../src/colony_growth.h"
int main(void) {
  const unsigned threshold[3]={8,16,24}, award[3]={3,4,5};
  for(unsigned peak=0;peak<256;++peak) for(unsigned current=0;current<256;++current) {
    unsigned expected=0;
    for(unsigned k=0;k<3;++k)
      if(peak<threshold[k] && current>=threshold[k]) expected+=award[k];
    assert(colony_growth_bonus((uint8_t)peak,(uint8_t)current)==expected);
  }
  assert(colony_growth_bonus(7,8)==3);
  assert(colony_growth_bonus(15,24)==9);
  assert(colony_growth_bonus(24,8)==0);
  assert(colony_growth_bonus(24,24)==0);
  puts("Colony growth rewards: all 65536 peak/current combinations PASS");
  return 0;
}
