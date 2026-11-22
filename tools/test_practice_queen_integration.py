#!/usr/bin/env python3
"""Ensure the actual GBA cartridge wires the protected hit rule at BOTH
combat sites, not just in the test-only helper. No SDK needed."""
from pathlib import Path

src = Path("src/main.c").read_text()
colony = src.split("static void step_ant(Ant *a) {", 1)[1].split(
    "static void grow_food(void)", 1
)[0]
eye = src.split("// bite\n", 1)[1].split("// red ants bite back", 1)[0]
assert src.count("practice_queen_can_lose_hp(") == 2, "Both damage sites need the guard"
assert "if ((rnd() & 1) && practice_queen_can_lose_hp(qhp[e], sandbox, tut, mp_on))" in colony
assert "qhp[e]--;" in colony and "if (e == 0) { apr(-3);" in colony
assert "if (practice_queen_can_lose_hp(qhp[1], sandbox, tut, mp_on))" in eye
assert "qhp[1]--;" in eye and "ach_get(34)" in eye
assert "PRACTICE QUEEN SAFE" in eye
assert "THE RED QUEEN FALLS!" in eye, "Real-game knockout must stay reachable"
print("PASS: normal-simulation and ANT EYE queen-hit gates are both live")
