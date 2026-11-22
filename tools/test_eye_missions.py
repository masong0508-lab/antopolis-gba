#!/usr/bin/env python3
"""Complete a campaign objective without leaving ANT EYE manually.

Boot two real GBA cartridges with an otherwise fresh, valid battery save
whose city scenarios unlock ANTVERSE. Select SCENT MASTER, enter ANT EYE,
and walk a repeating curved path while laying scent. The real ROM must
show its mission result and save completion while the player is still
using first-person controls; no START/map-exit input is sent until after
the result has been verified. No ROM bytes, CPU state, live world state,
mission counters or timers are patched.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import pathlib

from linked_smoke import A, B, DOWN, LEFT, R, RIGHT, SELECT, START, UP
from test_campaign_menu import expect_text
from test_editor_roundtrip import EditorEmulator

MSO = 9350
EMPTY_PROGRESS = b"MI" + b"\0" * 7
SCENT_MASTER_DONE = b"MI\0\x01" + b"\0" * 5  # mission 9, zero-based index 8


def expect_progress(emulator, expected, stage):
    for port in range(2):
        pointer, _ = emulator.memory(port)
        actual = C.string_at(pointer + MSO, len(expected))
        passed = actual == expected
        emulator.report["checks"].append({
            "stage": stage, "port": port + 1,
            "expected_progress_hex": expected.hex(),
            "actual_progress_hex": actual.hex(), "passed": passed,
        })
        assert passed, f"{stage}: P{port + 1} campaign completion record differs"


def run(rom, core, out):
    emulator = EditorEmulator(rom, core, out)
    emulator.report["core_sha256"] = hashlib.sha256(core.read_bytes()).hexdigest()
    emulator.report["scenario"] = "SCENT MASTER: auto-complete inside ANT EYE"
    try:
        emulator.tick(1)
        for port in range(2):
            pointer, length = emulator.memory(port)
            C.memset(pointer, 0xff, length)
            C.memmove(pointer + 22, b"\xff\x03", 2)  # city scenario unlocks
            C.memmove(pointer + 56, b"\0" * 4, 4)
            C.memmove(pointer + 9300, b"\0" * 4 + b"XA", 6)
            C.memmove(pointer + MSO, EMPTY_PROGRESS, len(EMPTY_PROGRESS))
        emulator.tick(850)
        emulator.press(DOWN)
        emulator.press(LEFT)
        emulator.press(A, released=180)  # ANTVERSE
        emulator.press(A, released=80)  # Missions
        emulator.press(R, released=80)  # jump five entries
        for _ in range(3):
            emulator.press(DOWN, released=80)
        emulator.snapshot("01_scent_master_selected")
        expect_text(emulator, 5, 11, "SCENT MASTER", "selected_mission")
        expect_progress(emulator, EMPTY_PROGRESS, "fresh_campaign_progress")

        emulator.press(A, released=100)  # mission briefing
        emulator.press(A, released=1500)  # world generation and loading fact
        emulator.press(A, released=100)  # first-match goal card
        emulator.press(A, released=100)  # live map
        emulator.press(SELECT, START, released=150)  # enter ANT EYE
        emulator.snapshot("02_ant_eye_entered")
        expect_text(emulator, 22, 16, "HELD", "first_person_entered")

        # These controls cannot dismiss a mission result. Periodic release
        # lets the game's ordinary key-release guard finish after victory.
        # This route exceeds the 60 scent-mark goal in the released baseline,
        # but that baseline stays in ANT EYE until START is pressed manually.
        for segment in range(6):
            emulator.keys = [{B, UP, RIGHT}, {B, UP, RIGHT}]
            emulator.tick(240)
            emulator.keys = [set(), set()]
            emulator.tick(5)
            if segment == 1:
                emulator.snapshot("03_laying_scent")
        emulator.tick(240)
        emulator.snapshot("04_automatic_mission_result")
        expect_text(emulator, 2, 8, "WELL DONE  ANTPEROR!", "objective_completes_inside_eye")
        expect_progress(emulator, SCENT_MASTER_DONE, "objective_completion_saved")

        # Only now acknowledge the result. The menu must reflect the saved
        # completion immediately and returning must not complete other goals.
        emulator.press(START, released=160)
        emulator.snapshot("05_completed_mission_menu")
        expect_text(emulator, 6, 2, " 1", "menu_completed_count")
        expect_text(emulator, 12, 2, "52", "menu_mission_total")
        expect_progress(emulator, SCENT_MASTER_DONE, "menu_preserves_completion")
        emulator.report["passed"] = True
        print("ANT EYE mission ROM PASS: scent objective ends automatically, saves mission 9, returns to menu with 1 of 52 completed")
    except Exception as error:
        emulator.report["passed"] = False
        emulator.report["error"] = str(error)
        if emulator.last is not None:
            emulator.snapshot("failure")
        raise
    finally:
        emulator.report["frames"] = emulator.frame
        (out / "report.json").write_text(json.dumps(emulator.report, indent=2) + "\n")
        emulator.lib.retro_unload_game()
        emulator.lib.retro_deinit()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=pathlib.Path)
    parser.add_argument("core", type=pathlib.Path)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/eye-missions"))
    args = parser.parse_args()
    run(args.rom.resolve(), args.core.resolve(), args.out.resolve())


if __name__ == "__main__":
    main()
