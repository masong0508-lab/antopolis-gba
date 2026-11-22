#!/usr/bin/env python3
"""Run production floor-cache and drowning rules without GBA hardware.

The fixture supplies registers/world storage, then compiles unchanged
function bodies from main.c and mp.c. Incremental VRAM updates must equal
a full rebuild without touching the adjacent packed cell, and pilot death
must have the same exactly-once bookkeeping as ordinary ant death.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def function(source: str, name: str) -> str:
    match = re.search(r"static (?:inline )?(?:void|u8) " + re.escape(name) + r"\([^)]*\)\s*\{", source)
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
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16;
#define MAXA 48
#define MAXW 128
#define MAXH 96
#define FT8 177
enum { FX_PUFF, FX_SPLASH };
typedef struct { u8 x, y, alive, team; } Ant;
static Ant ant[MAXA];
static u8 W, H, nestx[2], nesty[2], hgt[MAXH][MAXW], qhp[2];
static s16 fox, foy;
static const void *eye_tab;
static struct { u16 before[2], cells[2048], after[2]; } vram;
#define EYE_FLOOR_MAP (vram.cells)
static u16 st_kill, st_lost, rdrown;
static u8 appr, effects, last_effect, achievements;
static void fx_add(u8 x, u8 y, u8 type) {
  assert(x < W && y < H); effects++; last_effect = type;
}
static void ach_get(u8 id) { assert(id == 3); achievements++; }
"""


TESTS = r"""
static void guards(void) {
  assert(vram.before[0] == 0x1234 && vram.before[1] == 0xABCD);
  assert(vram.after[0] == 0x5678 && vram.after[1] == 0xDCBA);
}
static void terrain(void) {
  u8 x, y;
  nestx[0] = nesty[0] = 4;
  nestx[1] = (u8)(W - 5); nesty[1] = (u8)(H - 5);
  for (y = 0; y < H; y++) for (x = 0; x < W; x++)
    hgt[y][x] = (u8)((x + y * 3) & 3);
}
static void assert_dry_art(void) {
  u8 x, y, view; u16 i; u32 hash = 2166136261u;
  for (W = 32; W <= 96; W += 8) for (H = 32; H <= 96; H += 8) {
    terrain();
    // The old dry nest apron and all four materials keep their exact art.
    for (y = 0; y < H; y++) for (x = 0; x < W; x++)
      if (near_nest(x, y) && !hgt[y][x]) hgt[y][x] = 1;
    for (view = 0; view < 4; view++) {
      fox = view == 0 ? -32 : view == 1 ? 0 : view == 2 ? (s16)W - 48 : (s16)W - 1;
      foy = view == 0 ? -32 : view == 1 ? 0 : view == 2 ? (s16)H - 48 : (s16)H - 1;
      floor_map(vram.cells);
      for (i = 0; i < 2048; i++) {
        hash = (hash ^ (vram.cells[i] & 255)) * 16777619u;
        hash = (hash ^ (vram.cells[i] >> 8)) * 16777619u;
      }
    }
  }
  /* Golden hash from the unmodified 9412119 floor_map over these worlds. */
  assert(hash == 0xA1CA3DF1u);
}
static void cache_updates(void) {
  u16 rebuilt[2048], before[2048], i; u8 view, n;
  static const s16 positions[8] = {0, 1, 30, 31, 62, 63, -1, 64};
  vram.before[0] = 0x1234; vram.before[1] = 0xABCD;
  vram.after[0] = 0x5678; vram.after[1] = 0xDCBA;
  eye_tab = vram.cells;
  for (W = 32; W <= 96; W += 8) for (H = 32; H <= 96; H += 8) {
    terrain();
    for (view = 0; view < 4; view++) {
      fox = view == 0 ? -32 : view == 1 ? 0 : view == 2 ? (s16)W - 48 : (s16)W - 1;
      foy = view == 0 ? -32 : view == 1 ? 0 : view == 2 ? (s16)H - 48 : (s16)H - 1;
      floor_map(vram.cells);
      for (n = 0; n < 64; n++) {
        s16 wx = fox + positions[n & 7], wy = foy + positions[n >> 3];
        if (wx < 0 || wy < 0 || wx >= W || wy >= H) continue;
        hgt[wy][wx] = (u8)((hgt[wy][wx] + 1) & 3);
        memcpy(before, vram.cells, sizeof before);
        floor_changed((u8)wx, (u8)wy);
        floor_map(rebuilt);
        assert(memcmp(rebuilt, vram.cells, sizeof rebuilt) == 0);
        // A targeted patch must never rewrite another cache entry, even
        // when the adjacent packed byte has a deliberately arbitrary value.
        for (i = 0; i < 2048; i++) {
          s16 lx = wx - fox, ly = wy - foy;
          if (lx >= 0 && lx < 64 && ly >= 0 && ly < 64 && i == ly * 32 + lx / 2) {
            u16 mask = (lx & 1) ? 0x00FF : 0xFF00;
            assert((before[i] & mask) == (vram.cells[i] & mask));
          } else assert(before[i] == vram.cells[i]);
        }
        guards();
      }
    }
  }
  W = H = 96; terrain(); fox = foy = 16;
  for (i = 0; i < 2048; i++) vram.cells[i] = (u16)(0xA35C ^ i);
  memcpy(before, vram.cells, sizeof before);
  eye_tab = 0;
  floor_changed(32, 32); // inactive 2D map must never touch the affine map
  assert(memcmp(before, vram.cells, sizeof before) == 0);
  eye_tab = vram.cells;
  for (n = 0; n < 2; n++) {
    u8 x = (u8)(32 + n); u16 offset = 16 * 32 + (x - 16) / 2;
    memcpy(before, vram.cells, sizeof before);
    floor_changed(x, 32);
    for (i = 0; i < 2048; i++) if (i != offset) assert(before[i] == vram.cells[i]);
    assert((n ? vram.cells[offset] & 255 : vram.cells[offset] >> 8) ==
           (n ? before[offset] & 255 : before[offset] >> 8));
  }
  guards();
}
static void water_by_nests(void) {
  u8 h, tile;
  W = H = 32; terrain();
  hgt[4][4] = hgt[4][3] = 2;
  assert(floor_cell(4, 4) == FT8 + 13);
  assert(floor_cell(3, 4) == FT8 + 12);
  hgt[4][3] = 0;
  tile = floor_cell(3, 4); assert(tile == FT8 || tile == FT8 + 1);
  hgt[4][4] = 0; assert(floor_cell(4, 4) == FT8 + 13);
  for (h = 1; h <= 3; h++) { hgt[4][3] = h; assert(floor_cell(3, 4) == FT8 + 12); }
}
static void drowning_and_selection(void) {
  u8 i, h;
  W = H = 32; terrain(); memset(ant, 0, sizeof ant);
  st_lost = st_kill = rdrown = effects = achievements = 0; appr = 50;
  ant[0].x = ant[0].y = 10; ant[0].alive = 1; hgt[10][10] = 0;
  assert(ant_drown(0) == 1 && !ant[0].alive);
  assert(st_lost == 1 && st_kill == 0 && appr == 48 && effects == 1 && last_effect == FX_SPLASH);
  assert(ant_drown(0) == 0 && ant_drown(MAXA) == 0 && ant_drown(255) == 0);
  assert(st_lost == 1 && effects == 1);
  for (h = 1; h <= 3; h++) {
    ant[0].alive = 1; hgt[10][10] = h;
    assert(ant_drown(0) == 0 && ant[0].alive && effects == 1);
  }
  ant[0].team = 1; hgt[10][10] = 0;
  for (i = 0; i < 20; i++) { ant[0].alive = 1; assert(ant_drown(0) == 1); }
  assert(st_kill == 20 && rdrown == 20 && achievements == 1 && st_lost == 1);

  // The nearest worker is submerged; switching and auto-reacquisition
  // must choose a dry, unclaimed worker or return the spectator sentinel.
  memset(ant, 0, sizeof ant);
  for (i = 0; i < 3; i++) { ant[i].alive = 1; ant[i].x = (u8)(10 + i); ant[i].y = 10; hgt[10][10 + i] = 0; }
  hgt[10][12] = 1;
  assert(mp_pilot_near(10, 10, MAXA) == 2);
  assert(mp_pilot_next(0, MAXA) == 2);
  assert(mp_pilot_near(10, 10, 2) == MAXA && mp_pilot_next(0, 2) == MAXA);
  hgt[10][12] = 0;
  assert(mp_pilot_near(10, 10, MAXA) == MAXA);
  assert(mp_pilot_next(MAXA, MAXA) == MAXA);
}
int main(void) {
  assert_dry_art(); cache_updates(); water_by_nests(); drowning_and_selection();
  puts("ANT EYE world PASS: 81 sizes, cache borders/recentering, preserved packed neighbors, dry art, flooded nest aprons, exactly-once drowning and dry pilot selection");
  return 0;
}
"""


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/main.c").read_text()
    mp = (root / "src/mp.c").read_text()
    names = ["dist", "is_nest", "near_nest", "apr", "die", "ant_drown",
             "floor_cell", "floor_map", "floor_changed", "vok"]
    program = FIXTURE + "\n".join(function(source, name) for name in names)
    program += "\n" + function(mp, "mp_pilot_near") + "\n" + function(mp, "mp_pilot_next") + TESTS
    with tempfile.TemporaryDirectory(prefix="antopolis-world-") as folder:
        unit = Path(folder) / "world.c"
        binary = Path(folder) / "world"
        unit.write_text(program)
        compiler = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(compiler + ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                    str(unit), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
