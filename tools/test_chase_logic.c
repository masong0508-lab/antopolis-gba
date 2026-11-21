#include <assert.h>
#include <stdio.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
#include "../src/chase_logic.h"
int main(void) {
  // A map guide cannot score a waypoint from the scout's own position.
  assert(!chase_marker_legal(0, 5, 5));
  assert(!chase_marker_legal(1, 0xFFFFu, 5));
  assert(!chase_marker_legal(1, 5, 2));
  assert(chase_marker_legal(1, 5, 3));
  // The exact same predicate drives both gameplay validation and the
  // guide's player-facing reason: water > unreachable > near > legal.
  assert(chase_marker_reason(0, 0xFFFFu, 0) == 1);
  assert(chase_marker_reason(1, 0xFFFFu, 0) == 2);
  assert(chase_marker_reason(1, 9, 2) == 3);
  assert(chase_marker_reason(1, 9, 3) == 0);
  for (u16 d = 0; d <= 12; d++)
    for (u16 h = 0; h <= 3; h++)
      for (u16 p = 0; p <= 2; p++) {
        u16 path = p == 0 ? 0xFFFFu : p;
        assert(chase_marker_legal((u8)h, path, d) ==
               (chase_marker_reason((u8)h, path, d) == 0));
      }
  // Ready confirmations are individual rising edges, not pre-held A/START.
  const u16 ready_mask = 0x0009u;
  assert(!chase_ready_press(0x0001u, 0x0001u, ready_mask));
  assert(!chase_ready_press(0x0008u, 0x0001u, ready_mask));
  assert(!chase_ready_press(0u, 0u, ready_mask));
  assert(chase_ready_press(0u, 0x0001u, ready_mask));
  assert(chase_ready_press(0u, 0x0008u, ready_mask));
  assert(!chase_both_released(0x0001u, 0u, ready_mask));
  assert(!chase_both_released(0u, 0x0008u, ready_mask));
  assert(!chase_both_released(0x0001u, 0x0008u, ready_mask));
  assert(chase_both_released(0u, 0u, ready_mask));
  assert(chase_both_released(0x0080u, 0x0002u, ready_mask));
  // The scout sees the actual gameplay priority: a waypoint bonus
  // overrides carrying food briefly, then guidance returns to home/food.
  assert(chase_scout_message(0, 0, 0) == 0);
  assert(chase_scout_message(0, 1, 0) == 1);
  assert(chase_scout_message(1, 0, 0) == 2);
  assert(chase_scout_message(1, 1, 0) == 2);
  assert(chase_scout_message(0, 0, 60) == 3);
  assert(chase_scout_message(1, 1, 1) == 3);
  // Real HUD repaint cache must notice that a transient reward both
  // appeared and expired even if seconds, food and stamina stay constant.
  {
    ChaseHudCache cache = {255, 255, 255, 255, 255};
    u8 task = chase_scout_message(1, 0, 0);
    assert(chase_hud_needs_update(&cache, 88, 1, 0, 90, task));
    task = chase_scout_message(1, 0, 60);
    assert(chase_hud_needs_update(&cache, 88, 1, 0, 90, task));
    assert(!chase_hud_needs_update(&cache, 88, 1, 0, 90, task));
    task = chase_scout_message(1, 0, 0);
    assert(chase_hud_needs_update(&cache, 88, 1, 0, 90, task));
  }
  // The first frame draws every panel. Stable frames must not flood VRAM
  // with redundant 3/4-row clears; changes to any displayed field must
  // still redraw promptly on the next eight-frame HUD opportunity.
  {
    ChaseHudCache cache = {255, 255, 255, 255, 255};
    assert(chase_hud_needs_update(&cache, 90, 0, 0, 0, 0));
    for (u16 f = 0; f < 60; f++)
      assert(!chase_hud_needs_update(&cache, 90, 0, 0, 0, 0));
    assert(chase_hud_needs_update(&cache, 89, 0, 0, 0, 0));
    assert(chase_hud_needs_update(&cache, 89, 1, 0, 0, 0));
    assert(chase_hud_needs_update(&cache, 89, 1, 1, 0, 0));
    assert(chase_hud_needs_update(&cache, 89, 1, 1, 3, 0));
    assert(chase_hud_needs_update(&cache, 89, 1, 1, 3, 1));
    assert(!chase_hud_needs_update(&cache, 89, 1, 1, 3, 1));
    // A new round's panel is invalidated even when scores coincide.
    cache.seconds = 255;
    assert(chase_hud_needs_update(&cache, 90, 0, 0, 99, 0));
    // Two seconds at 60 Hz, panel opportunity every 8 frames.
    // A stable guide should repaint twice, not 15 times. A live
    // scout compass is independent and still updates every 8 frames.
    {
      u16 redraws = 0, chances = 0;
      cache.seconds = 255;
      for (u16 frame = 0; frame < 120; frame += 8) {
        u8 seconds = (u8)(90 - frame / 60);
        chances++;
        if (chase_hud_needs_update(&cache, seconds, 0, 0, 0, 0))
          redraws++;
      }
      assert(chances == 15);
      assert(redraws == 2);
    }
  }
  assert(chase_team_points(0, 0) == 0);
  assert(chase_team_points(2, 0) == 2);
  assert(chase_team_points(3, 1) == 6);
  // A refill visits all eight phases: no reachable position can be
  // permanently excluded by the placement pattern.
  for (u16 x = 0; x < 96; x++)
    for (u16 y = 0; y < 96; y++) {
      u8 bucket = chase_berry_bucket((u8)x, (u8)y);
      assert(bucket < 8);
      assert(bucket == (u8)((x * 17u + y * 29u) & 7u));
    }
  // First-person fractional movement should not snap to tile edges on
  // the hunter's screen. 8.8-cell coordinates convert to 8px tiles.
  assert(chase_view_px(0) == 0);
  assert(chase_view_px(256) == 8);
  assert(chase_view_px(511) == 15);
  assert(chase_view_px(5 * 256 + 128) == 44);
  puts("DUAL VIEW geometry, waypoint, scoring and food tests passed");
  return 0;
}
