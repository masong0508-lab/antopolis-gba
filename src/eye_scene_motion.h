/* Pure visual motion and ecology rules for ANT EYE (GBA).
 * Movement age drives alternating grounded footsteps, independently of
 * camera ordering. Plants bend gently, in WORLD coordinates, without
 * changing collision, world RNG, link checksums, OAM count or textures. */
#ifndef ANTOPOLIS_EYE_SCENE_MOTION_H
#define ANTOPOLIS_EYE_SCENE_MOTION_H
#include <stdint.h>
static uint8_t eye_ant_stride(uint8_t age, uint8_t walking) {
  return (uint8_t)(walking && age<8u && ((age>>1u)&1u));
}
static int8_t eye_plant_sway(uint8_t frame,uint8_t seed,uint8_t plant) {
  uint8_t q;
  if(plant!=0u && plant!=1u && plant!=5u) return 0;
  q=(uint8_t)(((frame>>4u)+(seed&3u))&3u);
  return (int8_t)(q==0u ? -1 : q==2u ? 1 : 0);
}
static uint8_t eye_ground_decor(uint8_t height,uint8_t noise,uint8_t shoreline) {
  if(height==1u)
    return (uint8_t)(noise<24u?2u:
      (shoreline ? noise>210u : noise>248u)?5u:255u);
  if(height==2u)
    return (uint8_t)(noise<30u?0u:noise<38u?1u:noise<43u?4u:255u);
  if(height==3u)
    return (uint8_t)(noise<42u?3u:noise<59u?2u:255u);
  return 255u;
}
#endif
