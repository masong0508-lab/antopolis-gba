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
// Explain WHY the selected cell cannot accept a guide waypoint. Reuse
// this same pure rule for A presses and the visible guide HUD, so the UI
// can never claim a cell is valid when the actual game rejects it.
// 0 valid, 1 water, 2 no home-connected path, 3 too near the scout.
static u8 chase_marker_reason(u8 height, u16 home_path, u16 scout_distance) {
  if (!height) return 1;
  if (home_path == 0xFFFFu) return 2;
  if (scout_distance < 3u) return 3;
  return 0;
}
static u8 chase_marker_legal(u8 height, u16 home_path, u16 scout_distance) {
  return (u8)(chase_marker_reason(height, home_path, scout_distance) == 0);
}
// Ready screens must be based on the SAME two linked input snapshots on
// both cartridges. A pre-held menu button is not a new confirmation.
static u8 chase_ready_press(u16 previous, u16 current, u16 mask) {
  return (u8)((current & mask) != 0 && (previous & mask) == 0);
}
// Neither console may enter a new scene while its peer still holds Ready.
static u8 chase_both_released(u16 p1, u16 p2, u16 mask) {
  return (u8)(((p1 | p2) & mask) == 0);
}
static u8 chase_team_points(u8 delivered, u8 complete) {
  return (u8)(delivered + (complete ? 3u : 0u));
}
#endif
