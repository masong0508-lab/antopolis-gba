// Pure CAT & MOUSE spotting rule, also exercised by the host regression test.
// No allocation, VRAM writes or random state: both GBA peers can evaluate it.
#ifndef ANTOPOLIS_CHASE_LOGIC_H
#define ANTOPOLIS_CHASE_LOGIC_H
static u8 chase_revealed(u8 scan, u8 carrying, u8 hidden, u16 distance) {
  // The hunter always spots a mouse within two tiles, even in tall cover.
  if (distance <= 2u) return 1;
  // Cover defeats long-range scans and the giveaway of carried food.
  return (u8)((scan || carrying) && (!hidden || distance <= 3u));
}
#endif
