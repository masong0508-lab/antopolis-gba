/* Visual-only GBA ant gait, world-anchored plant sway and shore
 * decoration. Enforce deterministic behavior, appropriate ground types,
 * no extra object categories, and stable moving poses independent of
 * camera/renderer object sorting. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/eye_scene_motion.h"
int main(void) {
  unsigned seed,frame,age,k;
  for(age=0;age<256;age++)for(k=0;k<2;k++) {
    uint8_t p=eye_ant_stride((uint8_t)age,(uint8_t)k);
    assert(p<=1u);
    assert(p==(uint8_t)(k && age<8u && ((age>>1u)&1u)));
  }
  for(k=0;k<6;k++)for(seed=0;seed<256;seed++) {
    unsigned negative=0,positive=0,center=0;
    for(frame=0;frame<256;frame++) {
      int8_t d=eye_plant_sway((uint8_t)frame,(uint8_t)seed,(uint8_t)k);
      assert(d>=-1 && d<=1);
      assert(d==eye_plant_sway((uint8_t)frame,(uint8_t)seed,(uint8_t)k));
      if(d==-1)negative++;else if(d==1)positive++;else center++;
    }
    if(k==0||k==1||k==5) {
      assert(negative && positive && center);
    } else {assert(negative==0 && positive==0 && center==256);}
  }
  for(unsigned h=0;h<4;h++)for(unsigned noise=0;noise<256;noise++)
    for(unsigned shoreline=0;shoreline<2;shoreline++) {
      uint8_t v=eye_ground_decor((uint8_t)h,(uint8_t)noise,(uint8_t)shoreline);
      assert(v==255u || v<6u);
      if(h==0u) assert(v==255u);
      if(h==1u) assert(v==255u || v==2u || v==5u);
      if(h==2u) assert(v==255u || v==0u || v==1u || v==4u);
      if(h==3u) assert(v==255u || v==2u || v==3u);
    }
  assert(eye_ground_decor(1,225,1)==5); // reeds by real water
  assert(eye_ground_decor(1,225,0)==255); // no inland reed thicket
  assert(eye_ground_decor(2,33,0)==1); // flower in grass
  assert(eye_ground_decor(3,20,0)==3); // exposed stone
  // Every hardware scale index uses a grounded 16px contact shadow,
  // exactly one pixel beneath the previously visible row-14 sprite.
  {
    static const uint16_t sizes[16]={20,25,31,38,47,59,73,90,
      112,139,172,213,264,328,406,504};
    for(unsigned i=0;i<16;i++)for(int ground=62;ground<128;ground++) {
      int16_t centre=eye_decor_ground_center((int16_t)ground,sizes[i]);
      assert(centre+(int)((7u*sizes[i])>>8u)==ground);
    }
  }
  for(unsigned distance=0;distance<32;distance++) {
    unsigned visible=0;
    for(unsigned k=0;k<4;k++) {
      uint8_t a=eye_decor_visible((uint8_t)distance,(uint8_t)k);
      assert(a<=1u);
      visible+=a;
    }
    assert(visible==(distance<=6?4u:distance<=10?3u:1u));
  }
  puts("GBA ANT EYE motion PASS: gait age, 256-frame plants and biome-grounded decor");
  return 0;
}
