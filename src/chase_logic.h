// Small pure helpers for DUAL VIEW expedition geometry, team scoring and
// deterministic berry supply. Host regression tests exercise each helper.
#ifndef ANTOPOLIS_CHASE_LOGIC_H
#define ANTOPOLIS_CHASE_LOGIC_H
// Integer-only 8-bucket spread, stable across builds and both machines.
static u8 chase_berry_bucket(u8 x, u8 y) {
  return (u8)(((u16)x * 17u + (u16)y * 29u) & 7u);
}
// ANT EYE stores 8.8 cell coordinates; the map uses eight screen pixels
// per world tile. Thus each 1/32-cell movement advances one map pixel.
static s32 chase_view_px(s32 position) { return position >> 5; }
static u8 chase_marker_legal(u8 height, u16 home_path, u16 scout_distance) {
  // Only reachable walkable land, and no instant 'place on scout' bonus.
  return (u8)(height != 0 && home_path != 0xFFFFu && scout_distance >= 3u);
}
static u8 chase_team_points(u8 delivered, u8 complete) {
  return (u8)(delivered + (complete ? 3u : 0u));
}
#endif
