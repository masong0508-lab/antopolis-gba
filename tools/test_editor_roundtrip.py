#!/usr/bin/env python3
"""Exercise the real Map Maker through libretro input and cartridge SRAM.

Seed legal saved maps, open MY MAP in the editor, save without changing it,
reopen, erase one berry, and save/reopen again. Cell-for-cell comparisons
cover >16 berries, disconnected shores, square and rectangular worlds.
No cartridge internals are patched; SRAM is seeded during the boot logo,
before the game reads its saved maps.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import pathlib
import time

from linked_smoke import (
    A, B, SELECT, START, DOWN, LEFT, L, AudioCallback, EnvCallback,
    GameInfo, InputCallback, PollCallback, RetroVariable, VideoCallback,
    RETRO_GAME_TYPE_GBA_LINK_2P, RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_P2,
    png565,
)

MYO = 64


def fixture(width, height):
    cells = bytearray(b"g" * (width * height))
    # A complete water wall separates the two nests. Reopening an editor
    # must preserve it; only launching play is allowed to cut a route.
    for y in range(height):
        cells[y * width + width // 2] = ord("w")
    for x in range(2, 12):
        cells[2 * width + x] = ord("s")
        cells[3 * width + x] = ord("h")
    for y in range(6, 14):
        for x in range(2, 10):
            cells[y * width + x] = ord("f")
    cells[(height - 6) * width + 4] = ord("Y")
    cells[5 * width + width - 5] = ord("R")
    # The editor starts two cells right and two above the home nest.
    erase_at = (height - 8) * width + 6
    cells[erase_at] = ord("f")
    assert cells.count(ord("f")) > 16
    return {"width": width, "height": height, "cells": cells, "erase_at": erase_at}


def record(item):
    cells = bytes(item["cells"])
    checksum = sum(cells) & 0xffff
    return b"EMAP" + bytes((item["width"], item["height"])) + cells + checksum.to_bytes(2, "little")


class EditorEmulator:
    def __init__(self, rom_path, core_path, out):
        self.out = out
        out.mkdir(parents=True, exist_ok=True)
        rom = rom_path.read_bytes()
        if rom[0xAC:0xB0] != b"EANT":
            raise RuntimeError("wrong GBA cartridge: expected EANT")
        self.lib = C.CDLL(str(core_path))
        self.keys = [set(), set()]
        self.frame = 0
        self.last = None
        self.report = {"rom_sha256": hashlib.sha256(rom).hexdigest(), "checks": []}
        self.options = {
            b"splitscreen_players": b"2", b"splitscreen_layout": b"2x1",
            b"splitscreen_overlays": b"off", b"splitscreen_fs_assist": b"off",
            b"splitscreen_audio": b"player 1",
        }
        self.save_dir = str(out).encode()

        @EnvCallback
        def environment(command, data):
            cmd = command & 0xffff
            if cmd == 15:
                v = C.cast(data, C.POINTER(RetroVariable)).contents
                value = self.options.get(v.key)
                if value is None:
                    return False
                v.value = value
                return True
            if cmd in (9, 31):
                C.cast(data, C.POINTER(C.c_char_p))[0] = self.save_dir
                return True
            if cmd == 10:
                return C.cast(data, C.POINTER(C.c_uint))[0] == 2
            if cmd == 17:
                C.cast(data, C.POINTER(C.c_bool))[0] = False
                return True
            return cmd in (16, 32, 34, 37, 61, 64)

        @VideoCallback
        def video(data, width, height, pitch):
            if data and width and height:
                self.last = (C.string_at(data, pitch * height), width, height, pitch)

        @AudioCallback
        def audio(data, count):
            return count

        @PollCallback
        def poll():
            pass

        @InputCallback
        def inputs(port, device, index, button):
            return int(device == 1 and port < 2 and button in self.keys[port])

        # Keep callback and ROM buffers alive for the lifetime of the core.
        self.callbacks = [environment, video, audio, poll, inputs]
        for name, callback_type, callback in zip(
            ("environment", "video_refresh", "audio_sample_batch", "input_poll", "input_state"),
            (EnvCallback, VideoCallback, AudioCallback, PollCallback, InputCallback),
            self.callbacks,
        ):
            function = getattr(self.lib, "retro_set_" + name)
            function.argtypes = [callback_type]
            function(callback)
        self.lib.retro_init()
        self.lib.retro_load_game_special.argtypes = [C.c_uint, C.POINTER(GameInfo), C.c_size_t]
        self.lib.retro_load_game_special.restype = C.c_bool
        self.lib.retro_get_memory_data.argtypes = [C.c_uint]
        self.lib.retro_get_memory_data.restype = C.c_void_p
        self.lib.retro_get_memory_size.argtypes = [C.c_uint]
        self.lib.retro_get_memory_size.restype = C.c_size_t
        self.cartridge = C.create_string_buffer(rom)
        self.rom_name = str(rom_path).encode()
        games = (GameInfo * 2)(*[
            GameInfo(self.rom_name, C.cast(self.cartridge, C.c_void_p), len(rom), None)
            for _ in range(2)
        ])
        if not self.lib.retro_load_game_special(RETRO_GAME_TYPE_GBA_LINK_2P, games, 2):
            raise RuntimeError("mGBA rejected the two-cartridge subsystem")

    def memory(self, port):
        memory_id = (RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_P2)[port]
        pointer = self.lib.retro_get_memory_data(memory_id)
        length = self.lib.retro_get_memory_size(memory_id)
        if not pointer or length < 9306:
            raise RuntimeError("core did not expose cartridge SRAM")
        return pointer, length

    def seed(self, fixtures):
        for port, item in enumerate(fixtures):
            pointer, length = self.memory(port)
            C.memset(pointer, 0xff, length)
            data = record(item)
            C.memmove(pointer + MYO, data, len(data))

    def tick(self, frames):
        start = time.perf_counter()
        for _ in range(frames):
            self.lib.retro_run()
            self.frame += 1
            if not (self.frame & 63) and time.perf_counter() - start > 35:
                raise TimeoutError("emulator stalled")

    def press(self, *buttons, released=18):
        self.keys = [set(buttons), set(buttons)]
        self.tick(3)
        self.keys = [set(), set()]
        self.tick(released)

    def snapshot(self, name):
        if self.last is None:
            raise RuntimeError("no emulator picture")
        png565(*self.last, self.out / (name + ".png"))

    def verify(self, fixtures, stage):
        failures = []
        for port, item in enumerate(fixtures):
            expected = record(item)
            pointer, _ = self.memory(port)
            actual = C.string_at(pointer + MYO, len(expected))
            changes = [i for i, (a, b) in enumerate(zip(expected, actual)) if a != b]
            check = {
                "stage": stage, "port": port + 1,
                "dimensions": [item["width"], item["height"]],
                "expected_berries": bytes(item["cells"]).count(b"f"),
                "actual_berries": actual[6:-2].count(b"f"),
                "changed_bytes": len(changes), "passed": not changes,
            }
            self.report["checks"].append(check)
            print(json.dumps(check), flush=True)
            if changes:
                failures.append(f"P{port + 1}: {len(changes)} changed bytes, first offsets {changes[:12]}")
                (self.out / f"{stage}-p{port + 1}-expected.emap").write_bytes(expected)
                (self.out / f"{stage}-p{port + 1}-actual.emap").write_bytes(actual)
        if failures:
            raise AssertionError("Map Maker altered saved map during " + stage + "; " + "; ".join(failures))

    def save_and_quit(self):
        self.press(START, released=35)
        self.press(DOWN)
        self.press(DOWN)
        self.press(A, released=180)

    def run(self, fixtures):
        try:
            # The core allocates SRAM on its first frame. The game's save
            # loader runs only after the long boot logo, hundreds later.
            self.tick(1)
            self.seed(fixtures)
            self.tick(850)
            self.snapshot("01_title")
            self.press(A, released=180)   # PLAY -> new game settings
            self.press(DOWN)              # difficulty -> map
            self.press(LEFT, released=180)  # MEADOW wraps to MY MAP
            self.snapshot("02_my_map_selected")
            self.press(B, released=80)
            self.press(SELECT, released=180)
            self.snapshot("03_authored_editor")
            self.save_and_quit()
            self.verify(fixtures, "first_save")
            self.press(SELECT, released=180)
            self.save_and_quit()
            self.verify(fixtures, "unchanged_reopen")
            self.press(SELECT, released=180)
            self.press(B)                 # pick the berry at initial cursor
            self.press(L)                 # BERRIES -> HILL
            self.press(L)                 # HILL -> GRASS
            self.press(A)                 # erase exactly one berry
            self.snapshot("04_one_berry_erased")
            for item in fixtures:
                item["cells"][item["erase_at"]] = ord("g")
            self.save_and_quit()
            self.verify(fixtures, "erase_one_berry")
            self.press(SELECT, released=180)
            self.save_and_quit()
            self.verify(fixtures, "reopen_after_erase")
            self.report["passed"] = True
        except Exception as error:
            self.report["passed"] = False
            self.report["error"] = str(error)
            if self.last is not None:
                self.snapshot("failure")
            raise
        finally:
            self.report["frames"] = self.frame
            (self.out / "report.json").write_text(json.dumps(self.report, indent=2) + "\n")
            self.lib.retro_unload_game()
            self.lib.retro_deinit()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=pathlib.Path)
    parser.add_argument("core", type=pathlib.Path)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/editor-roundtrip"))
    parser.add_argument("--tall", action="store_true", help="exercise 32x96 and 96x96 instead of 32x32 and 96x32")
    args = parser.parse_args()
    sizes = ((32, 96), (96, 96)) if args.tall else ((32, 32), (96, 32))
    emulator = EditorEmulator(args.rom.resolve(), args.core.resolve(), args.out.resolve())
    emulator.run([fixture(*size) for size in sizes])
    print("Map Maker SRAM roundtrip PASS: all painted berries and terrain preserved; one intentional erase persisted")


if __name__ == "__main__":
    main()
