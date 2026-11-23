#!/usr/bin/env python3
"""Guard read-only, on-demand GBA campaign reports across all game modes."""
from pathlib import Path
from re import findall

main = Path("src/main.c").read_text()
empire = Path("src/empire.c").read_text()
city = Path("src/city.c").read_text()
missions = Path("src/missions.c").read_text()
catalog = Path("src/mission_catalog.h").read_text()

body = missions.split("static void mis_progress_screen(void) {", 1)[1].split(
    "static void mis_tick(void) {", 1
)[0]
goal_def = catalog.split("enum { MG_CWIN,", 1)[1].split(" };", 1)[0]
goals = ["MG_CWIN"] + findall(r"\bMG_[A-Z0-9]+\b", goal_def)
assert len(set(goals)) == 38, goals
covered = set(findall(r"case (MG_[A-Z0-9]+)\s*:", body))
assert set(goals) - {"MG_MEGA"} <= covered, set(goals) - covered
assert "default: mis_track(7," in body, "Unspecified goal needs a visible fallback"
# The first progress value begins at column 17. Labels start at column
# 2 and must end strictly before the value (GBA card is 30 tiles wide).
for label in findall(r'mis_track\([^,]+,\s*"([^"]+)"', body):
    assert len(label) <= 14, ("Mission metric collides with its value", label)

assert "if (!mis_on) return;" in body
assert 'ps(2, 3, m->name, 9)' in body
assert "mis_end_goal(m->goal)" in body, "Survival goals must show deadline behavior"
assert "m->kind >= MK_CITY ? cmon" in body, "City deadlines use months"
assert "m->kind == MK_EMPIRE ? (u32)ecyc * 4u / 60u" in body
assert "((u32)gt * 2u / 15u) / 60u" in body, "Classic deadlines use minutes"
assert "mission_progress_mark(" not in body and "mis_mark(" not in body
assert "SRAM" not in body and "malloc(" not in body and "alloc" not in body
assert 'if ((p & J_R) && mis_on)' in main
assert 'R: MISSION REPORT' in main
assert 'if (mis_on) mis_progress_screen();' in empire
assert 'if (mis_on) mis_progress_screen(); else city_goal_card();' in city
assert "static u8 mis_on;" in main
assert "static u8 mis_on, mis_i" not in missions, "No duplicate mission state"
# Mission launches temporarily disable sandbox and lessons. On return, even
# after an explicit quit, NEW GAME must retain the player's prior choices.
run = missions.split("static void mission_run(u8 i) {", 1)[1].split(
    "static void missions_menu(void) {", 1
)[0]
assert "u8 saved_sandbox = sandbox, saved_tutor = tutor;" in run
assert run.index("saved_sandbox = sandbox") < run.index("mis_on = 1;")
assert "sandbox = 0; tutor = 0;" in run, "Campaigns still need normal rules"
assert "sandbox = saved_sandbox; tutor = saved_tutor;" in run
assert run.index("sandbox = saved_sandbox;") > run.index("if (!quit) mis_result(win);")
assert run.index("sandbox = saved_sandbox;") > run.index("mis_on = 0;")
print("PASS: GBA mission reports and sandbox/tutorial option restoration")

