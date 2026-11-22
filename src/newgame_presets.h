/* Two optional NEW GAME setups. Explicit settings; no hidden rules.
   Neither preset edits the player's saved custom map, options or SRAM. */
#ifndef ANTOPOLIS_NEWGAME_PRESETS_H
#define ANTOPOLIS_NEWGAME_PRESETS_H
#include <stdint.h>
typedef struct {
  uint8_t difficulty, map, width, height, sandbox, lessons;
} AntStartPreset;
/* 0 = guided: easy Meadow, manageable area and interactive lessons.
   1 = wild: hard Canyon with a spacious 64x64 terrain.
   Both use existing modes, so original saves and link rules are unchanged. */
static AntStartPreset ant_newgame_preset(uint8_t challenge) {
  AntStartPreset p;
  p.difficulty = challenge ? 2u : 0u;
  p.map = challenge ? 3u : 0u;
  p.width = challenge ? 64u : 48u;
  p.height = challenge ? 64u : 48u;
  p.sandbox = 0u;
  p.lessons = challenge ? 0u : 1u;
  return p;
}
#endif
