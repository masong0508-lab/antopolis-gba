#!/usr/bin/env python3
"""Exercise production compass composition without GBA display hardware.

The real bearing, font indexing, marker painters and DUAL VIEW composer
are compiled unchanged with a tiny BG1 register substitute. The glyph
named by the current task must remain at its actual bearing, even when
all reference markers share it or clamp to a screen edge.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def function(source: str, name: str) -> str:
    match = re.search(r"static (?:void|u8) " + re.escape(name) + r"\([^)]*\)\s*\{", source)
    assert match, f"production function {name} missing"
    end, depth = match.end(), 1
    while depth:
        assert end < len(source), f"unterminated function {name}"
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


FIXTURE = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#define FT 4
static struct { u16 before, cells[32], after; } vram;
#define BGMAP1 (vram.cells)
static u8 nestx[2], nesty[2], efx, efy, eex, eey, ehas_f, ehas_e;
static u8 expedition_found, expedition_x, expedition_y, searches;
static u8 gi(char c);
static void pc(u8 x, u8 y, char c, u8 p) {
  assert(x < 30 && y == 0);
  BGMAP1[x] = (u16)((FT + gi(c)) | (p << 12));
}
static void ov_fill(u16 tile, u8 pal, u8 low, u8 high) {
  u8 x; assert(low == 0 && high == 0);
  for (x = 0; x < 32; x++) BGMAP1[x] = (u16)(tile | (pal << 12));
}
static u8 chase_goal(u8 x, u8 y, u8 *gx, u8 *gy) {
  (void)x; (void)y; searches++;
  *gx = expedition_x; *gy = expedition_y;
  return expedition_found;
}
"""


TESTS = r"""
static u8 glyph_count(char c) {
  u8 x, count = 0;
  for (x = 0; x < 30; x++)
    if ((BGMAP1[x] & 0x3FFu) == (u16)(FT + gi(c))) count++;
  return count;
}
static void expect(u8 column, char glyph, u8 palette) {
  assert(BGMAP1[column] == (u16)((FT + gi(glyph)) | (palette << 12)));
  assert(vram.before == 0x1234 && vram.after == 0x5678);
}
int main(void) {
  static const s8 dx[8] = {8,8,0,-8,-8,-8,0,8};
  static const s8 dy[8] = {0,8,8,8,0,-8,-8,-8};
  u16 heading, cases = 0, saved[32]; u8 direction, carry, column;
  vram.before = 0x1234; vram.after = 0x5678;
  ehas_f = ehas_e = 1;
  for (heading = 0; heading < 256; heading++) for (direction = 0; direction < 8; direction++) {
    nestx[0] = nestx[1] = efx = eex = (u8)(32 + dx[direction]);
    nesty[0] = nesty[1] = efy = eey = (u8)(32 + dy[direction]);
    column = eye_mark_column((u8)heading, dx[direction], dy[direction]);
    assert(column < 30);
    for (carry = 0; carry < 2; carry++) {
      char goal = carry ? '$' : '%'; u8 palette = carry ? 2 : 13;
      eye_compass((u8)heading, 32, 32, carry);
      expect(column, goal, palette);
      assert(glyph_count(goal) == 1);
      memcpy(saved, BGMAP1, sizeof saved);
      eye_aux_mark((u8)heading, dx[direction], dy[direction], 'P', 9, goal);
      assert(memcmp(saved, BGMAP1, sizeof saved) == 0); // no sideways relocation
      eye_aux_mark((u8)heading, dx[direction], dy[direction], '?', 14, carry ? '$' : 0);
      expect(column, carry ? '$' : '?', carry ? 2 : 14);
      cases++;
    }
  }
  // Head-on positions and clamped offscreen bearings keep the old geometry.
  assert(eye_mark_column(0, 8, 0) == 15);
  assert(eye_mark_column(64, 0, 8) == 15);
  assert(eye_mark_column(128, -8, 0) == 15);
  assert(eye_mark_column(192, 0, -8) == 15);
  assert(eye_mark_column(0, 0, -8) == 0);
  assert(eye_mark_column(0, 0, 8) == 29);
  // A separate teammate bearing remains visible; protection is local.
  ehas_f = 1; ehas_e = 0; efx = 40; efy = 32;
  nestx[0] = 24; nesty[0] = 32; nestx[1] = 32; nesty[1] = 24;
  eye_compass(0, 32, 32, 0);
  eye_aux_mark(0, 0, 8, 'P', 9, '%');
  expect(15, '%', 13); expect(29, 'P', 9);

  // The renderer sees food east; expedition's preferred food is south.
  // Compose once with that selected target, rather than drawing two %s.
  expedition_found = 1; expedition_x = 32; expedition_y = 40; searches = 0;
  chase_compass(0, 32, 32, 0, 0, 0, 0);
  assert(searches == 1 && glyph_count('%') == 1);
  expect(29, '%', 13);
  assert(chase_scout_message(0, 0, 0) == 0);
  // Empty scout follows the guide, so ? wins an identical bearing.
  chase_compass(0, 32, 32, 0, 1, 32, 40);
  expect(29, '?', 14);
  assert(chase_scout_message(0, 1, 0) == 1);
  // Loaded scout must see home even while a guide marker is still active.
  searches = 0;
  chase_compass(0, 32, 32, 1, 1, 24, 32);
  expect(0, '$', 2); assert(glyph_count('$') == 1 && searches == 0);
  assert(chase_scout_message(1, 1, 0) == 2);
  // No expedition candidate: retain the renderer's one nearby target.
  expedition_found = 0; efx = 40; efy = 32;
  chase_compass(0, 32, 32, 0, 0, 0, 0);
  assert(glyph_count('%') == 1); expect(15, '%', 13);
  ehas_f = 0;
  chase_compass(0, 32, 32, 0, 0, 0, 0);
  assert(glyph_count('%') == 0); // do not invent a food bearing
  printf("GBA compass PASS: %u heading/goal collisions, exact bearings, teammate visibility, one expedition food target and matching guide/home priority\n", cases);
  return 0;
}
"""


def main():
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/main.c").read_text()
    chase = (root / "src/chase.c").read_text()
    logic = (root / "src/chase_logic.h").read_text()
    sine = re.search(r"static const s16 SINT\[256\]\s*=\s*\{.*?\};", source, re.S)
    assert sine, "production sine table missing"
    bodies = [function(source, name) for name in
              ("gi", "bearing", "eye_mark_column", "eye_mark", "eye_aux_mark", "eye_compass")]
    bodies += [function(logic, "chase_scout_message"), function(chase, "chase_compass")]
    with tempfile.TemporaryDirectory(prefix="antopolis-compass-") as directory:
        test = Path(directory) / "compass.c"
        executable = Path(directory) / "compass"
        test.write_text(FIXTURE + sine.group(0) + "\n" + "\n".join(bodies) + TESTS)
        command = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(command + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-Wno-misleading-indentation", str(test), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
