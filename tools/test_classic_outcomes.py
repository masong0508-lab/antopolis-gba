#!/usr/bin/env python3
"""Execute the cartridge's Classic outcome rules in a small native fixture.

The two functions below are read unchanged from main.c and compiled with
ordinary C. Only hardware-independent world globals and mission responses
are supplied by the fixture. This exercises exact clock boundaries and
simultaneous outcomes without patching live ROM state or waiting 20 minutes.
The separate test_eye_missions.py checks actual first-person integration.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def function(source: str, name: str) -> str:
    match = re.search(r"static void " + re.escape(name) + r"\([^)]*\)\s*\{", source)
    assert match, f"production function {name} missing"
    depth = 1
    end = match.end()
    while depth:
        assert end < len(source), f"unterminated production function {name}"
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


FIXTURE = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#define MAXA 48
#define MANA_MAX 20
enum { O_TIME };
#define O(i) (opt[i])
typedef struct { u8 alive, team; } Ant;
static Ant ant[MAXA];
static u8 over, tut, sandbox, qhp[2], qbase, appr, mana, ncnt[2], timeup;
static u8 mp_on, mis_on, opt[1], goal, goal_reads;
static u16 gt, frc, mission_limit;
static u8 mis_ok(void) { goal_reads++; return goal; }
static const u16 *mis_lim_p(void) { return &mission_limit; }
"""

TESTS = r"""
static void reset(void) {
  u8 i;
  for (i = 0; i < MAXA; i++) { ant[i].alive = 0; ant[i].team = 0; }
  over = tut = sandbox = timeup = mp_on = mis_on = opt[0] = goal = goal_reads = 0;
  gt = 0; frc = 16; mission_limit = 0;
  qhp[0] = qhp[1] = qbase = 5;
  appr = 50; mana = 10; ncnt[0] = ncnt[1] = 0;
}
static void population(u8 black, u8 red) {
  u8 i;
  for (i = 0; i < MAXA; i++) {
    ant[i].alive = (u8)(i < black + red);
    ant[i].team = (u8)(i >= black);
  }
}
int main(void) {
  u8 setting, side, result;
  static const u16 ticks[4] = {0, 2250, 4500, 9000};

  // The 5 / 10 / 20 minute clocks expire on exactly the correct game tick.
  // Stale HUD counts deliberately favor black; living ants favor red.
  for (setting = 1; setting <= 3; setting++) {
    reset(); opt[O_TIME] = setting; population(1, 4);
    ncnt[0] = 24; ncnt[1] = 0;
    gt = (u16)(ticks[setting] - 1); classic_outcome_tick();
    assert(over == 0 && timeup == 0);
    gt++; classic_outcome_tick();
    assert(over == 2 && timeup == 1 && ncnt[0] == 1 && ncnt[1] == 4);
  }
  reset(); gt = 65535; classic_outcome_tick();
  assert(over == 0 && timeup == 0);  // clock OFF really is unlimited

  // Queen hearts retain their three-ant weight, and an exact tie favors black.
  reset(); opt[O_TIME] = 1; gt = ticks[1]; qhp[0] = 3; qhp[1] = 2;
  population(0, 3); classic_outcome_tick(); assert(over == 1 && timeup == 1);
  reset(); opt[O_TIME] = 1; gt = ticks[1]; population(4, 4);
  classic_outcome_tick(); assert(over == 1);

  // A fallen queen decides the match before either an expired clock or goal.
  for (side = 0; side < 2; side++) {
    reset(); opt[O_TIME] = 1; gt = ticks[1]; mis_on = goal = 1;
    qhp[side] = 0; classic_outcome_tick();
    assert(over == (side ? 1 : 2) && timeup == 0 && goal_reads == 0);
  }
  reset(); qhp[0] = qhp[1] = 0; classic_outcome_tick(); assert(over == 1);

  // Results remain latched when control returns from ANT EYE to the map.
  for (result = 1; result <= 5; result++) {
    reset(); over = result; mis_on = goal = 1; sandbox = 1;
    qhp[0] = qhp[1] = 0; classic_outcome_tick();
    assert(over == result && qhp[0] == 0 && goal_reads == 0);
  }

  // Mission checks keep their established 16-frame cadence. A completed
  // objective wins at its deadline; an incomplete objective fails there.
  reset(); mis_on = goal = 1; frc = 15; classic_outcome_tick();
  assert(over == 0 && goal_reads == 0);
  frc++; classic_outcome_tick(); assert(over == 4 && goal_reads == 1);
  reset(); mis_on = 1; mission_limit = 5; gt = ticks[1] - 1;
  classic_outcome_tick(); assert(over == 0);
  gt++; classic_outcome_tick(); assert(over == 5);
  reset(); mis_on = goal = 1; mission_limit = 5; gt = ticks[1];
  classic_outcome_tick(); assert(over == 4);
  reset(); mis_on = 1; gt = 65535; classic_outcome_tick(); assert(over == 0);

  // The user-selected match clock still takes precedence over a mission goal.
  reset(); mis_on = goal = 1; opt[O_TIME] = 1; gt = ticks[1];
  classic_outcome_tick(); assert(over == 1 && timeup == 1 && goal_reads == 0);

  // Tutorial and sandbox remain protected after simulation in either view,
  // including a last-heart hit on the same frame the clock expires.
  for (side = 0; side < 2; side++) {
    reset(); tut = (u8)!side; sandbox = side; opt[O_TIME] = 1; gt = ticks[1];
    mis_on = goal = 1; qhp[0] = qhp[1] = 0; appr = 0; mana = 0;
    classic_outcome_tick();
    assert(over == 0 && timeup == 0 && qhp[0] == qbase && qhp[1] == qbase);
    assert(appr == (side ? 99 : 50) && mana == (side ? MANA_MAX : 0));
    assert(goal_reads == 0);
  }

  // Linked and pass-and-play matches must ignore stale solo mission state.
  // Their clock uses the same refreshed population and shared deterministic data.
  for (side = 1; side <= 2; side++) {
    reset(); mp_on = side; mis_on = goal = 1; mission_limit = 1; gt = 450;
    classic_outcome_tick(); assert(over == 0 && goal_reads == 0);
    opt[O_TIME] = 1; gt = ticks[1]; population(0, 2);
    ncnt[0] = 24; ncnt[1] = 0; classic_outcome_tick();
    assert(over == 2 && timeup == 1 && goal_reads == 0);
    reset(); mp_on = side; tut = sandbox = 1; qhp[0] = 0;
    classic_outcome_tick(); assert(over == 2 && qhp[0] == 0);
  }
  puts("Classic outcome PASS: clocks, fresh counts, queen/mission precedence, latched results, protected modes, linked isolation");
  return 0;
}
"""


def main() -> None:
    source = (Path(__file__).resolve().parents[1] / "src/main.c").read_text()
    program = FIXTURE + function(source, "count") + "\n" + function(source, "classic_outcome_tick") + TESTS
    with tempfile.TemporaryDirectory(prefix="antopolis-outcomes-") as folder:
        folder = Path(folder)
        unit = folder / "outcomes.c"
        binary = folder / "outcomes"
        unit.write_text(program)
        command = shlex.split(os.environ.get("HOSTCC", "cc"))
        subprocess.run(command + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                   str(unit), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
