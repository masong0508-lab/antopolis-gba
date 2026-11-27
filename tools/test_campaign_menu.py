#!/usr/bin/env python3
"""Verify the expanded 56-mission campaign through real ROM input and video.

The fixture is a valid battery save with all old 50 missions completed and
the existing MISSION MASTER trophy earned. The test reads the actual 240x160
video pixels to verify the expanded menu total, unchanged original finales and bonus expeditions.
No live game variables, CPU state or ROM bytes are patched.
"""
from __future__ import annotations

import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import pathlib
import re

from linked_smoke import A, DOWN, LEFT, UP, SELECT, START
from test_editor_roundtrip import EditorEmulator

MSO = 9350
OLD_PROGRESS = b"MI" + b"\xff" * 6 + b"\x03"


def font_glyphs():
    source = (pathlib.Path(__file__).resolve().parents[1] / "src/font8.h").read_text()
    glyphs = {}
    for values, char in re.findall(r"\{([^{}]+)\},\s*//\s*'(.)'", source):
        glyphs[char] = bytes(int(x.strip(), 16) for x in values.split(","))
    assert len(glyphs) == 59
    return glyphs


def expect_text(emulator, column, row, label, stage):
    """Match the shipped font's foreground mask against real emulator pixels."""
    pixels, width, height, pitch = emulator.last
    assert width == 480 and height == 160
    glyphs = font_glyphs()
    for port in range(2):
        samples = []
        for index, char in enumerate(label):
            for y, bits in enumerate(glyphs[char]):
                for x in range(8):
                    offset = (row * 8 + y) * pitch + (port * 240 + (column + index) * 8 + x) * 2
                    value = int.from_bytes(pixels[offset:offset + 2], "little")
                    samples.append((bool(bits & (128 >> x)), value))
        ink = Counter(value for foreground, value in samples if foreground).most_common(1)[0][0]
        mismatches = sum(foreground != (value == ink) for foreground, value in samples)
        check = {"stage": stage, "port": port + 1, "text": label, "glyph_mismatches": mismatches}
        emulator.report["checks"].append(check)
        if mismatches:
            raise AssertionError(f"{stage}: P{port + 1} did not display {label!r}; {mismatches} glyph pixels differ")


def run(rom, core, out):
    emulator = EditorEmulator(rom, core, out)
    emulator.report["core_sha256"] = hashlib.sha256(core.read_bytes()).hexdigest()
    try:
        emulator.tick(1)
        for port in range(2):
            pointer, length = emulator.memory(port)
            C.memset(pointer, 0xff, length)
            C.memmove(pointer + 22, b"\xff\x03", 2)  # ten city scenarios unlock ANTVERSE
            C.memmove(pointer + 56, b"\0" * 4, 4)
            C.memmove(pointer + 9300, b"\0\0\0\x80XA", 6)  # already earned Mission Master
            C.memmove(pointer + MSO, OLD_PROGRESS, len(OLD_PROGRESS))
        emulator.tick(850)
        emulator.press(DOWN)
        emulator.press(LEFT)
        emulator.press(A, released=180)
        emulator.snapshot("01_antverse_menu")
        expect_text(emulator, 23, 6, "56", "antverse_total")
        emulator.press(A, released=80)
        emulator.snapshot("02_missions_first_page")
        expect_text(emulator, 6, 2, "50", "old_progress_loaded")
        expect_text(emulator, 12, 2, "56", "mission_total")
        expect_text(emulator, 1, 18, "COMPLETED", "initial_complete_label")
        expect_text(emulator, 1, 12, "START: NEXT MODE", "next_mode_help")
        expect_text(emulator, 18, 12, "MODE", "mode_progress_label")
        expect_text(emulator, 24, 12, "5", "classic_mode_done")
        expect_text(emulator, 29, 12, "6", "classic_mode_total")
        emulator.press(START, released=80)
        # pn right-aligns single digits in its two-cell numeric field.
        expect_text(emulator, 3, 11, "6", "next_mode_number")
        expect_text(emulator, 5, 11, "THROUGH THE EYE", "next_mode_title")
        expect_text(emulator, 24, 12, "4", "eye_mode_done")
        expect_text(emulator, 29, 12, "5", "eye_mode_total")
        for _ in range(5):
            emulator.press(START, released=80)
        expect_text(emulator, 5, 3, "FIRST STEPS", "mode_cycle_wrap")
        # SELECT skips the first 50 saved successes and highlights #51.
        # A second page is not required: this is one deliberate button press.
        emulator.press(SELECT, released=80)
        emulator.snapshot("03_next_unfinished")
        expect_text(emulator, 2, 11, "51", "next_open_number")
        expect_text(emulator, 5, 11, "EVERYTHING", "next_open_title")
        expect_text(emulator, 23, 12, "10", "verse_mode_done")
        expect_text(emulator, 28, 12, "12", "verse_mode_total")
        expect_text(emulator, 17, 19, "SEL NEXT OPEN", "next_open_help")
        expect_text(emulator, 1, 18, "NOT COMPLETED", "next_open_state_label")
        emulator.press(DOWN, released=80)
        emulator.snapshot("04_original_finale")
        expect_text(emulator, 2, 11, "52", "final_number")
        expect_text(emulator, 5, 11, "GRAND FINALE", "final_title")
        emulator.press(DOWN, released=80)
        emulator.snapshot("05_bonus_expediton")
        expect_text(emulator, 2, 11, "53", "bonus_number")
        expect_text(emulator, 5, 11, "SCOUTS FIRST", "bonus_title")
        emulator.press(UP, released=80)  # select unchanged GRAND FINALE #52
        emulator.press(A, released=100)
        emulator.snapshot("06_grand_finale_briefing")
        expect_text(emulator, 2, 5, "THREE WILD RIVALS, HOSTILE".replace(",", " "), "final_briefing")
        # Begin the restored final mission and allow its large world to build.
        emulator.press(A, released=1800)
        emulator.snapshot("07_finale_launch")
        # Loading facts stop at an acknowledgement, then the ANTVERSE goal card.
        emulator.press(A, released=240)
        emulator.snapshot("08_finale_goal")
        emulator.press(A, released=180)
        emulator.snapshot("09_finale_playing")
        expect_text(emulator, 18, 18, "AP", "final_gameplay_resources")
        expect_text(emulator, 2, 19, "BUILD", "final_gameplay_controls")
        for port in range(2):
            pointer, _ = emulator.memory(port)
            assert C.string_at(pointer + MSO, len(OLD_PROGRESS)) == OLD_PROGRESS
            assert C.string_at(pointer + 9303, 1)[0] & 0x80
        emulator.report["passed"] = True
        emulator.report["save_check"] = "old 50 mission bits and previously earned Mission Master retained"
        print("Campaign ROM PASS: 56 total, original finales and bonus visible, old progress retained")
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
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/campaign-menu"))
    args = parser.parse_args()
    run(args.rom.resolve(), args.core.resolve(), args.out.resolve())


if __name__ == "__main__":
    main()
