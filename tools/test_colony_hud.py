#!/usr/bin/env python3
"""Check live Classic / ANT EYE labels and legibility in real GBA video.

Boot ordinary games on two emulated cartridges, enter ANT EYE with the
real shortcut, walk, lay scent, and return to the map. Match production
font masks to rendered pixels and check that HUDs have a quiet dark
backdrop. No ROM, CPU state, or live simulation variables are patched.
"""
from __future__ import annotations

import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import pathlib

from linked_smoke import A, B, SELECT, START, UP
from test_editor_roundtrip import EditorEmulator
from test_campaign_menu import expect_text


def expect_backdrop(emulator, first_row, last_row, stage):
    pixels, width, height, pitch = emulator.last
    assert (width, height) == (480, 160)
    for port in range(2):
        colors = Counter()
        for y in range(first_row * 8, (last_row + 1) * 8):
            for x in range(port * 240, (port + 1) * 240):
                offset = y * pitch + x * 2
                colors[int.from_bytes(pixels[offset:offset + 2], "little")] += 1
        color, count = colors.most_common(1)[0]
        rgb = ((color >> 11) * 255 // 31, ((color >> 5) & 63) * 255 // 63,
               (color & 31) * 255 // 31)
        fraction = count / sum(colors.values())
        passed = fraction >= 0.55 and max(rgb) < 80
        emulator.report["checks"].append({"stage": stage, "port": port + 1,
            "dark_backdrop_fraction": round(fraction, 3), "backdrop_rgb": rgb,
            "passed": passed})
        assert passed, (stage, port + 1, fraction, rgb)


def run(rom, core, out):
    emulator = EditorEmulator(rom, core, out)
    emulator.report["core_sha256"] = hashlib.sha256(core.read_bytes()).hexdigest()
    try:
        emulator.tick(1)
        for port in range(2):
            pointer, length = emulator.memory(port)
            C.memset(pointer, 0xff, length)  # fresh cartridge, before the save loader
        emulator.tick(850)
        emulator.press(A, released=180)  # home -> new game
        emulator.press(A, released=1500)
        emulator.press(A, released=100)  # loading fact -> first-match briefing
        emulator.press(A, released=100)  # briefing -> live map
        emulator.snapshot("01_classic_hud")
        expect_text(emulator, 9, 18, "FOOD", "classic_food")
        expect_text(emulator, 16, 18, "MOOD", "classic_mood")
        expect_backdrop(emulator, 17, 19, "classic_backdrop")

        emulator.press(SELECT, START, released=150)
        emulator.snapshot("02_ant_eye_hud")
        for port in range(2):
            pointer, _ = emulator.memory(port)
            assert C.string_at(pointer + 56, 1)[0] & 0x80, "ANT EYE never entered"
        expect_text(emulator, 9, 16, "FOOD", "eye_food")
        expect_text(emulator, 16, 16, "AP", "eye_action_points")
        expect_text(emulator, 22, 16, "HELD", "eye_carried_food")
        expect_text(emulator, 1, 18, "D PAD WALK/TURN", "eye_movement")
        expect_backdrop(emulator, 16, 19, "eye_backdrop")
        expect_backdrop(emulator, 0, 0, "compass_backdrop")

        emulator.keys = [{B}, {B}]
        emulator.tick(60)
        emulator.snapshot("03_scent")
        emulator.keys = [{UP}, {UP}]
        emulator.tick(80)
        emulator.keys = [set(), set()]
        emulator.tick(40)
        emulator.snapshot("04_walking")
        emulator.press(START, released=100)
        emulator.snapshot("05_back_to_map")
        expect_text(emulator, 9, 18, "FOOD", "return_to_classic")
        expect_backdrop(emulator, 17, 19, "return_backdrop")
        emulator.report["passed"] = True
        print("Classic / ANT EYE ROM HUD PASS: readable resources, movement hints, dark panels, walk/scent/map transitions")
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
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/colony-hud"))
    args = parser.parse_args()
    run(args.rom.resolve(), args.core.resolve(), args.out.resolve())


if __name__ == "__main__":
    main()
