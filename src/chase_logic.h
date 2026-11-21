// Pure CAT & MOUSE spotting rule, also exercised by the host regression test.
// No allocation, VRAM writes or random state: both GBA peers can evaluate it.
#ifndef ANTOPOLIS_CHASE_LOGIC_H
#define ANTOPOLIS_CHASE_LOGIC_H
// Integer-only 8-bucket spread, stable across builds and both machines.
static u8 chase_berry_bucket(u8 x, u8 y) {
  return (u8)(((u16)x * 17u + (u16)y * 29u) & 7u);
}
// ANT EYE stores 8.8 cell coordinates; the map uses eight screen pixels
// per world tile. Thus each 1/32-cell movement advances one map pixel.
static s32 chase_view_px(s32 position) { return position >> 5; }
static u8 chase_revealed(u8 scan, u8 carrying, u8 hidden, u16 distance) {
  // The hunter always spots a mouse within two tiles, even in tall cover.
  if (distance <= 2u) return 1;
  // Cover defeats long-range scans and the giveaway of carried food.
  return (u8)((scan || carrying) && (!hidden || distance <= 3u));
}
#endif
