/* Procedural ANT EYE ground micro-art. 14 fixed 8x8 8bpp tiles,
   colours 96..119: water, sand, grass, hill and nest. Run only when
   entering/refreshing the view; never allocate VRAM, sprites or maps.
   Every input tile is 0..13; x/y are pixel coordinates 0..7.
   The scattered stones and plants are deterministic, not RNG gameplay. */
#ifndef ANTOPOLIS_EYE_FLOOR_DETAIL_H
#define ANTOPOLIS_EYE_FLOOR_DETAIL_H
#include <stdint.h>
static uint8_t eye_floor_pixel(uint8_t t, uint8_t x, uint8_t y) {
  uint16_t h=(uint16_t)(x*31u+y*17u+t*101u+x*y*7u);
  uint8_t r, c;
  h^=h>>3u; h=(uint16_t)(h*0x2B9u); r=(uint8_t)(h>>4u);
  if(t<2u) {
    // Two staggered ripples with broken highlights instead of parallel
    // uninterrupted bars. Palette entries 97/98 shimmer in VBlank.
    if(((y+t*4u)&3u)==1u && ((x+y*3u+t)&7u)<4u) return 97u;
    if(((y+t)&3u)==2u && ((x*3u+y+t)&7u)==0u) return 98u;
    return r<16u?98u:96u;
  }
  if(t<5u) {
    c=(uint8_t)(r<30u?100u:r<52u?101u:r<60u?102u:
      ((y+t)%4u==0u&&r<140u)?100u:99u);
    // Two grains of a visible dune contour on sand variant 4.
    if(t==4u && y==4u && x>=2u && x<=5u && x!=4u) c=100u;
    return c;
  }
  if(t<9u) {
    c=(uint8_t)(r<50u?105u:r<95u?104u:103u);
    if(((x*3u+t*5u)%7u)==0u && (y+t)%8u<3u) c=106u;
    // A small blade cluster reads as low foliage when walking nearby,
    // without an extra billboard or sprite in the 3D scene.
    if(t==6u && ((x==2u&&y==5u)||(x==3u&&y==4u)||
                  (x==4u&&y==5u))) c=106u;
    if(t==8u && ((x==2u&&y==2u)||(x==5u&&y==5u))) c=107u;
    if(t==8u && x==6u && y==1u) c=108u;
    return c;
  }
  if(t<12u) {
    c=(uint8_t)(r<40u?111u:r<90u?110u:109u);
    if(((x+y+t)&7u)==0u && (r&1u)) c=111u;
    if(t==10u && x>=2u && x<=4u && y>=3u && y<=4u)
      c=(uint8_t)(y==3u?113u:112u);
    // Small stratified fissure on a different rock variant.
    if(t==11u && y==3u && x>=2u && x<=6u) c=113u;
    if(t==11u && y==4u && x>=3u && x<=5u) c=112u;
    return c;
  }
  c=(uint8_t)(r<45u?116u:r<95u?115u:114u);
  if(t==12u && y==3u && x>=2u && x<=5u) c=115u;
  if(t==13u && x>=2u && x<=5u && y>=2u && y<=5u)
    c=(uint8_t)((x==2u||y==2u)?116u:117u);
  return c;
}
#endif
