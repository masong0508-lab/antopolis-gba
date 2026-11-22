#!/usr/bin/env python3
"""Use real Map Maker controls to catch overlapping queens after resize.

A 96x96 saved map has two separate queens outside the future 32x32
bounds. On older ROMs the two nests crop to one tile; saving silently
drops the Y queen. This test uses SRAM only before game boot and reads
the game-written SRAM afterward, without modifying running game state.
"""
from __future__ import annotations

import argparse
import ctypes as C
import json
from pathlib import Path

from linked_smoke import A, B, DOWN, LEFT, SELECT, START, UP
from test_editor_roundtrip import EditorEmulator, MYO


def source_map(wet: bool):
    w = h = 96
    cells = bytearray(b"g" * (w * h))
    cells[90 * w + 90] = ord("Y")
    cells[91 * w + 91] = ord("R")
    cells[7 * w + 7] = ord("f")
    if wet:
        cells[27 * w + 27] = ord("w")
        cells[27 * w + 23] = ord("w")
    else:
        cells[27 * w + 27] = ord("f")
        cells[27 * w + 23] = ord("f")
    return {"width": w, "height": h, "cells": cells}


def check_save(emu: EditorEmulator, port: int):
    ptr, _ = emu.memory(port)
    data = C.string_at(ptr + MYO, 6 + 32 * 32 + 2)
    assert data[:4] == b"EMAP", "Map Maker did not save"
    w, h = data[4], data[5]
    assert (w, h) == (32, 32), ("wrong resized dimensions", w, h)
    cells = data[6:6 + w * h]
    checksum = sum(cells) & 0xFFFF
    assert data[6 + w * h:8 + w * h] == checksum.to_bytes(2, "little")
    queens = {chr(c): i for i, c in enumerate(cells) if c in (ord("Y"), ord("R"))}
    assert cells.count(ord("Y")) == 1 and cells.count(ord("R")) == 1, queens
    assert queens["Y"] == 27 * w + 27, queens
    assert queens["R"] == 27 * w + 23, queens
    assert cells[7 * w + 7] == ord("f"), "Unrelated painted berry lost"
    assert cells.count(ord("f")) == 1, "A relocated nest retained hidden food"
    return {"port": port + 1, "dimensions": [w, h],
            "queen_cells": {"Y": [27, 27], "R": [23, 27]},
            "berries": 1, "checksum": checksum, "passed": True}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom", type=Path)
    ap.add_argument("core", type=Path)
    ap.add_argument("--out", type=Path, default=Path("build/editor-nest-resize"))
    args = ap.parse_args()
    emu = EditorEmulator(args.rom, args.core, args.out)
    report = {"scenario": "96x96 to 32x32 nest collision via Map Maker",
              "checks": [], "passed": False}
    try:
        emu.tick(1)
        emu.seed([source_map(False), source_map(True)])
        emu.tick(850)
        emu.press(A, released=180)      # PLAY -> settings
        emu.press(DOWN)
        emu.press(LEFT, released=180)   # MEADOW wraps to MY MAP
        emu.press(B, released=80)
        emu.press(SELECT, released=180) # editor
        emu.snapshot("01_before_resize")
        emu.press(START)                # editor menu starts on KEEP EDITING
        for _ in range(3): emu.press(DOWN)  # WIDTH
        for _ in range(8): emu.press(LEFT)  # 96 -> 32
        emu.press(DOWN)                # HEIGHT
        for _ in range(8): emu.press(LEFT)  # 96 -> 32
        emu.snapshot("02_after_resize")
        emu.press(UP)
        emu.press(UP)                  # SAVE AND QUIT
        emu.press(A, released=180)
        report["checks"] = [check_save(emu, i) for i in range(2)]
        report["passed"] = True
        emu.snapshot("03_after_save")
    except Exception as exc:
        report["error"] = str(exc)
        if emu.last:
            emu.snapshot("failure")
    finally:
        report["frames"] = emu.frame
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
