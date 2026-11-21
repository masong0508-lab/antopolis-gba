#!/usr/bin/env python3
"""Headless *real libretro* two-GBA cable smoke test.

Runs two Antopolis ROMs inside the mGBA Splitscreen link-cable core.
The game's battery SRAM achievements are used as durable evidence of
a completed handshake (48) and entering first-person co-op (7).
This is emulator integration, not a hardware timing or FPS certification.
"""
from __future__ import annotations

import argparse
import base64
import ctypes as C
import json
import pathlib
import struct
import sys
import time
import zlib


class GameInfo(C.Structure):
    _fields_ = [
        ("path", C.c_char_p),
        ("data", C.c_void_p),
        ("size", C.c_size_t),
        ("meta", C.c_char_p),
    ]


class RetroVariable(C.Structure):
    _fields_ = [("key", C.c_char_p), ("value", C.c_char_p)]


EnvCallback = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VideoCallback = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AudioCallback = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
PollCallback = C.CFUNCTYPE(None)
InputCallback = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)

# libretro joypad IDs; these are NOT the game's bitmask positions.
B, SELECT, START, UP, DOWN, LEFT, RIGHT, A, L, R = 0, 2, 3, 4, 5, 6, 7, 8, 10, 11
RETRO_GAME_TYPE_GBA_LINK_2P = 0x201
RETRO_MEMORY_SAVE_RAM = 0
RETRO_MEMORY_P2 = 0x100


def png565(pixels: bytes, width: int, height: int, pitch: int, dest: pathlib.Path):
    def chunk(tag: bytes, data: bytes):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)

    scan = bytearray()
    for y in range(height):
        scan.append(0)
        for x in range(width):
            off = y * pitch + x * 2
            v = pixels[off] | (pixels[off + 1] << 8)
            scan.extend((
                ((v >> 11) & 31) * 255 // 31,
                ((v >> 5) & 63) * 255 // 63,
                (v & 31) * 255 // 31,
            ))
    dest.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(bytes(scan), 3)) +
        chunk(b"IEND", b"")
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=pathlib.Path)
    parser.add_argument("core", type=pathlib.Path)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/link-smoke"))
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    rom = args.rom.resolve().read_bytes()
    if rom[0xAC:0xB0] != b"EANT":
        raise RuntimeError("wrong GBA game code: expected EANT")
    lib = C.CDLL(str(args.core.resolve()))
    report = {"rom": args.rom.name, "core": args.core.name, "screens": {}, "observations": []}
    state = {"keys": [set(), set()], "frame": 0, "last": None, "width": 0, "height": 0, "pitch": 0}
    options = {
        b"splitscreen_players": b"2",
        b"splitscreen_layout": b"2x1",
        b"splitscreen_overlays": b"off",
        b"splitscreen_fs_assist": b"off",
        b"splitscreen_audio": b"player 1",
    }
    save_dir = str(args.out.resolve()).encode()

    @EnvCallback
    def environment(command, data):
        cmd = command & 0xFFFF
        if cmd == 15:  # GET_VARIABLE
            v = C.cast(data, C.POINTER(RetroVariable)).contents
            value = options.get(v.key)
            if value is None:
                return False
            v.value = value
            return True
        if cmd in (9, 31):  # GET_SYSTEM_DIRECTORY, GET_SAVE_DIRECTORY
            C.cast(data, C.POINTER(C.c_char_p))[0] = save_dir
            return True
        if cmd == 10:  # SET_PIXEL_FORMAT: RGB565 = 2
            return C.cast(data, C.POINTER(C.c_uint))[0] == 2
        if cmd == 17:  # GET_VARIABLE_UPDATE
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        if cmd == 52:  # GET_CORE_OPTIONS_VERSION: use legacy vars
            return False
        if cmd in (16, 32, 34, 37, 61, 64):  # SET_... callbacks
            return True
        return False

    @VideoCallback
    def video(data, w, h, pitch):
        if data and w and h:
            state["last"] = C.string_at(data, pitch * h)
            state["width"] = int(w)
            state["height"] = int(h)
            state["pitch"] = int(pitch)

    @AudioCallback
    def audio(data, count):
        return count

    @PollCallback
    def poll():
        pass

    @InputCallback
    def input_state(port, device, index, button):
        if device == 1 and port < 2 and button in state["keys"][port]:
            return 1
        return 0

    lib.retro_set_environment.argtypes = [EnvCallback]
    lib.retro_set_video_refresh.argtypes = [VideoCallback]
    lib.retro_set_audio_sample_batch.argtypes = [AudioCallback]
    lib.retro_set_input_poll.argtypes = [PollCallback]
    lib.retro_set_input_state.argtypes = [InputCallback]
    lib.retro_set_environment(environment)
    lib.retro_set_video_refresh(video)
    lib.retro_set_audio_sample_batch(audio)
    lib.retro_set_input_poll(poll)
    lib.retro_set_input_state(input_state)
    lib.retro_init()

    lib.retro_load_game_special.argtypes = [C.c_uint, C.POINTER(GameInfo), C.c_size_t]
    lib.retro_load_game_special.restype = C.c_bool
    lib.retro_get_memory_data.argtypes = [C.c_uint]
    lib.retro_get_memory_data.restype = C.c_void_p
    lib.retro_get_memory_size.argtypes = [C.c_uint]
    lib.retro_get_memory_size.restype = C.c_size_t

    # Hold the cartridge backing memory alive until the core has copied it.
    cartridge = C.create_string_buffer(rom)
    name = str(args.rom.resolve()).encode()
    games = (GameInfo * 2)(
        GameInfo(name, C.cast(cartridge, C.c_void_p), len(rom), None),
        GameInfo(name, C.cast(cartridge, C.c_void_p), len(rom), None),
    )
    if not lib.retro_load_game_special(RETRO_GAME_TYPE_GBA_LINK_2P, games, 2):
        raise RuntimeError("mGBA Splitscreen rejected its two-ROM GBA subsystem")

    def tick(frames):
        start = time.perf_counter()
        for _ in range(frames):
            lib.retro_run()
            state["frame"] += 1
            if (state["frame"] & 63) == 0 and time.perf_counter() - start > 35:
                raise TimeoutError("linked core stalled or ran extremely slowly")
        return round(time.perf_counter() - start, 3)

    def press(*buttons, ports=(0, 1), held=3, released=14):
        for port in ports:
            state["keys"][port] = set(buttons)
        tick(held)
        state["keys"] = [set(), set()]
        tick(released)

    def snapshot(label):
        pixels, w, h, pitch = (state[k] for k in ("last", "width", "height", "pitch"))
        if pixels is None or w < 480 or h < 160:
            raise RuntimeError("linked core did not produce a 2-player 480x160 frame")
        name = args.out / (label + ".png")
        png565(pixels, w, h, pitch, name)
        width = w // 2
        left = b"".join(pixels[y * pitch:y * pitch + 2 * width] for y in range(h))
        right = b"".join(pixels[y * pitch + 2 * width:y * pitch + 4 * width] for y in range(h))
        report["screens"][label] = {
            "frame": state["frame"], "dimensions": [w, h],
            "p1_crc32": hex(zlib.crc32(left)), "p2_crc32": hex(zlib.crc32(right)),
            "different": left != right,
            "png": name.name,
        }
        print("SCREEN", label, report["screens"][label], flush=True)
        # Diagnostic PNG is printed so we can inspect actual emulator pixels
        # in failed CI jobs without trusting noisy emulator serial messages.
        print("SCREEN_PNG64", label, base64.b64encode(name.read_bytes()).decode("ascii"), flush=True)

    def save_info():
        found = []
        for mem_id in (RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_P2):
            pointer = lib.retro_get_memory_data(mem_id)
            length = lib.retro_get_memory_size(mem_id)
            if not pointer or length < 9306:
                raise RuntimeError("linked core did not expose both GBA SRAMs; cannot confirm multiplayer")
            buf = C.string_at(pointer, 9306)
            found.append({
                "sram_marker": buf[:4].hex(),
                "link_achievement_48": bool(buf[9302] & 1),  # ach2 bit 16
                "ant_eye_achievement_7": bool(buf[56] & 0x80),
                "achievement_tag": buf[9304:9306].decode("ascii", "replace"),
            })
        return found

    try:
        report["boot_seconds"] = tick(500)
        snapshot("01_title")
        report["observations"].append({"after_title": save_info()})
        # Both ROMs: select bottom-left title card "2 PLAYER".
        press(DOWN)
        press(A)
        tick(100)
        snapshot("02_multiplayer_menu")
        # Menu order: VERSUS -> CO-OP CLASSIC by pressing RIGHT once.
        press(RIGHT)
        press(DOWN)      # second row: PLAY ON
        press(RIGHT)     # SAME GBA -> LINK CABLE
        snapshot("03_link_selected")
        press(START)
        report["link_wait_seconds"] = tick(700)
        snapshot("04_after_link")
        report["link_sram"] = save_info()
        print("LINK SRAM", json.dumps(report["link_sram"]), flush=True)
        # Both achievements are written by the game itself when the lobby
        # completes, NOT by the harness or emulator.
        if not all(x["link_achievement_48"] for x in report["link_sram"]):
            raise AssertionError("GBA cable handshake or linked lobby did not complete on BOTH consoles")
        # The first pilot alone triggers the shared 3D transition.
        press(SELECT, START, ports=(0,), held=4)
        tick(100)
        snapshot("05_two_pilots")
        report["eye_sram"] = save_info()
        if not all(x["ant_eye_achievement_7"] for x in report["eye_sram"]):
            raise AssertionError("linked 3D co-op did not start on BOTH consoles")
        # Exercise *different* directional inputs on each connected GBA.
        state["keys"][0] = {UP}
        state["keys"][1] = {RIGHT}
        tick(75)
        state["keys"] = [set(), set()]
        press(A, ports=(0,))
        press(B, ports=(1,), held=25)
        snapshot("06_independent_controls")
        press(START, ports=(1,))
        tick(90)
        snapshot("07_back_to_map")
        report["end_sram"] = save_info()
        report["result"] = "PASS: dual-core cable lobby + synchronized ANT EYE entered on both GBAs"
    except Exception as exc:
        report["result"] = "FAIL: " + repr(exc)
        raise
    finally:
        report["total_frames"] = state["frame"]
        (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2), flush=True)
        lib.retro_unload_game()
        lib.retro_deinit()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("SMOKE TEST FAILED:", repr(exc), file=sys.stderr, flush=True)
        sys.exit(1)
