#!/usr/bin/env python3
"""Check live ANT EYE terrain and pilot safety using the unmodified GBA ROM.

Only a valid battery-options/unlock record is seeded before the boot logo
finishes. Weather and multiplayer are selected through normal controls.
Read-only libretro save snapshots expose both cartridges' RAM and VRAM;
no snapshot is loaded and no live game memory is changed. Symbols come
from the matching ELF, whose load bytes are verified against the ROM.
"""
from __future__ import annotations

import argparse
import ctypes as C
import hashlib
import json
import pathlib
import struct

from linked_smoke import A, B, DOWN, LEFT, R, RIGHT, SELECT, START
from test_editor_roundtrip import EditorEmulator


class Symbols:
    def __init__(self, elf_path, rom_path):
        elf, rom = elf_path.read_bytes(), rom_path.read_bytes()
        assert elf[:6] == b"\x7fELF\x01\x01", "Expected a little-endian ELF32"
        self.sha256 = hashlib.sha256(elf).hexdigest()
        phoff, shoff = struct.unpack_from("<II", elf, 28)
        phsize, phnum, shsize, shnum = struct.unpack_from("<4H", elf, 42)
        covered = bytearray(len(rom))
        for i in range(phnum):
            kind, off, _, addr, size, _, _, _ = struct.unpack_from("<8I", elf, phoff + i * phsize)
            if kind != 1 or not size or not 0x08000000 <= addr < 0x0a000000:
                continue
            start = addr - 0x08000000
            assert start + size <= len(rom), "ELF load segment exceeds ROM"
            for j in range(size):
                pos = start + j
                # gbafix fills only the Nintendo logo/cartridge header.
                if not 4 <= pos < 0xc0:
                    assert rom[pos] == elf[off + j], f"ELF does not match ROM at {pos:#x}"
                covered[pos] = 1
        assert all(covered), "ELF does not describe the complete cartridge"
        sections = [struct.unpack_from("<10I", elf, shoff + i * shsize) for i in range(shnum)]
        self.items = {}
        for section in sections:
            _, kind, _, _, off, size, link, _, _, entry = section
            if kind != 2:
                continue
            strings = sections[link]
            names = elf[strings[4]:strings[4] + strings[5]]
            for cursor in range(off, off + size, entry):
                name, addr, length, _, _, _ = struct.unpack_from("<IIIBBH", elf, cursor)
                text = names[name:names.index(0, name)].decode()
                if text:
                    self.items[text] = (addr, length)
        assert self.items["ant"][1] == 480, "Unexpected production Ant layout"
        assert self.items["hgt"][1] == 128 * 96, "Unexpected height-map stride"


class ReadOnlyState:
    # Public mGBA raw GBA state format, version 11. See upstream
    # include/mgba/internal/gba/serialize.h and the splitscreen wrapper's
    # src/platform/libretro-splitscreen/instances.c (SPST format version 1).
    def __init__(self, emulator, symbols):
        self.emulator, self.symbols = emulator, symbols
        emulator.lib.retro_serialize_size.restype = C.c_size_t
        emulator.lib.retro_serialize.argtypes = [C.c_void_p, C.c_size_t]
        emulator.lib.retro_serialize.restype = C.c_bool

    def capture(self):
        size = self.emulator.lib.retro_serialize_size()
        buffer = C.create_string_buffer(size)
        assert self.emulator.lib.retro_serialize(buffer, size), "Read-only snapshot failed"
        data = buffer.raw
        assert struct.unpack_from("<3I", data) == (0x54535053, 1, 2), "Unknown linked state format"
        lengths = struct.unpack_from("<2I", data, 12)
        states = []
        offset = 20
        for length in lengths:
            state = data[offset:offset + length]
            assert len(state) >= 0x61000 and struct.unpack_from("<I", state)[0] == 0x0100000b
            states.append(state)
            offset += length
        return states

    def memory(self, state, address, size):
        for base, length, offset in ((0x02000000, 0x40000, 0x21000),
                                     (0x03000000, 0x8000, 0x19000),
                                     (0x04000000, 0x400, 0x400),
                                     (0x06000000, 0x18000, 0x1000)):
            if base <= address and address + size <= base + length:
                start = offset + address - base
                return state[start:start + size]
        raise AssertionError(f"Unsupported read-only address {address:#x}")

    def value(self, state, name, signed=False):
        address, size = self.symbols.items[name]
        assert size in (1, 2, 4), (name, size)
        return int.from_bytes(self.memory(state, address, size), "little", signed=signed)

    def array(self, state, name):
        return self.memory(state, *self.symbols.items[name])

    def world(self, state):
        data = {name: self.value(state, name) for name in
                ("mw", "mh", "gt", "frc", "eye_ctl", "eye_ctl2", "lk_err", "over", "st_lost", "eye_tab")}
        data["fox"] = self.value(state, "fox", True)
        data["foy"] = self.value(state, "foy", True)
        data["hgt"] = self.array(state, "hgt")
        data["ant"] = self.array(state, "ant")
        data["nestx"] = self.array(state, "nestx")[:2]
        data["nesty"] = self.array(state, "nesty")[:2]
        data["floor"] = self.memory(state, 0x0600b000, 4096)
        data["display_mode"] = int.from_bytes(self.memory(state, 0x04000000, 2), "little") & 7
        return data


def options_fixture(emulator):
    # Original OP checksum format; custom settings avoid incidental
    # elections and clock endings during the weather stress session.
    options = [0, 0, 1, 1, 1, 0, 2, 3, 0, 1, 1, 0, 0, 0,
               1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0]
    for port in range(2):
        pointer, length = emulator.memory(port)
        C.memset(pointer, 0xff, length)
        C.memmove(pointer + 24, b"OP" + bytes(options), 2 + len(options))
        C.memmove(pointer + 60, bytes([sum(v + i for i, v in enumerate(options)) & 255]), 1)
        C.memmove(pointer + 61, b"C", 1)  # the game's saved master-code unlock
        C.memmove(pointer + 56, bytes(4), 4)
        C.memmove(pointer + 9300, bytes(4) + b"XA", 6)


def enter_linked_weather(emulator, reader):
    emulator.tick(1)
    options_fixture(emulator)
    emulator.tick(850)
    emulator.press(DOWN)
    emulator.press(RIGHT)
    emulator.press(A, released=100)  # EXTRAS
    emulator.press(R, released=50)  # CHEATS
    emulator.press(DOWN)
    emulator.press(DOWN)
    emulator.press(A)
    emulator.press(A)  # WEATHER: CHAOS
    for _ in range(6):
        emulator.press(DOWN)
    emulator.press(A)  # PEACEFUL RIVALS
    emulator.snapshot("01_weather_chaos_selected")
    states = reader.capture()
    for state in states:
        cheats = reader.array(state, "cht")
        assert cheats[2] == 2 and cheats[8] == 1, "Stress options not selected through the UI"
    emulator.press(B, released=100)
    emulator.press(LEFT)
    emulator.press(A, released=100)  # 2 PLAYER
    emulator.press(RIGHT)  # CO-OP CLASSIC
    emulator.press(DOWN)
    emulator.press(RIGHT)  # LINK CABLE
    emulator.press(DOWN)
    for _ in range(6):
        emulator.press(RIGHT)  # THE MAZE: real authored map with shallow paths
    emulator.snapshot("02_link_maze_selected")
    emulator.press(START, released=1300)
    emulator.press(A, released=120)
    emulator.snapshot("03_linked_colony")
    emulator.keys = [{SELECT, START}, set()]
    emulator.tick(4)
    emulator.keys = [set(), set()]
    emulator.tick(120)
    emulator.snapshot("04_linked_ant_eye")
    states = reader.capture()
    for state in states:
        assert reader.value(state, "eye_tab") and reader.value(state, "mp_on") == 2
        assert reader.value(state, "mp_kind") == 4 and reader.array(state, "cht")[2] == 2


def pilots(world):
    result = []
    for name in ("eye_ctl", "eye_ctl2"):
        index = world[name]
        if index >= 48:
            result.append(None)
            continue
        ant = world["ant"][index * 10:index * 10 + 10]
        x, y = ant[:2]
        result.append({"slot": index, "x": x, "y": y, "alive": ant[3],
                       "height": world["hgt"][y * 128 + x]})
    return result


def original_pilot(world, pilot):
    ant = world["ant"][pilot["slot"] * 10:pilot["slot"] * 10 + 10]
    return {"slot": pilot["slot"], "alive": ant[3], "x": ant[0], "y": ant[1],
            "still_controlled": pilot["slot"] in (world["eye_ctl"], world["eye_ctl2"]),
            "colony_losses": world["st_lost"]}


def shared_world_equal(worlds):
    # CPU snapshots are not synchronized instruction boundaries: the
    # decorative ant.age increment can already have run on one core.
    # Compare event/gameplay invariants, then exercise the cartridge's
    # own rolling link checksum after recovery instead of gating that
    # transient animation phase or ordinary AI interpolation bytes.
    return (all(worlds[0][name] == worlds[1][name] for name in
                ("hgt", "eye_ctl", "eye_ctl2", "gt", "st_lost"))
            and pilots(worlds[0]) == pilots(worlds[1]))


def shared_difference(worlds):
    result = {}
    for name in ("hgt", "eye_ctl", "eye_ctl2", "gt", "st_lost"):
        a, b = worlds[0][name], worlds[1][name]
        if a != b:
            result[name] = ([(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y][:12]
                            if isinstance(a, bytes) else [a, b])
    if pilots(worlds[0]) != pilots(worlds[1]):
        result["pilots"] = [pilots(w) for w in worlds]
    return result


def check(report, stage, passed, **details):
    report["checks"].append({"stage": stage, "passed": bool(passed), **details})


def floor_mismatches(world):
    # Compare material classes, independently of the renderer's texture
    # variation arithmetic. Nest aprons are dirt only while above water.
    families = ({177, 178}, {179, 180, 181}, {182, 183, 184, 185}, {186, 187, 188})
    failures = []
    for y in range(world["mh"]):
        for x in range(world["mw"]):
            fx, fy = x - world["fox"], y - world["foy"]
            if not (0 <= fx < 64 and 0 <= fy < 64):
                continue
            h = world["hgt"][y * 128 + x]
            nest = any(x == nx and y == ny for nx, ny in zip(world["nestx"], world["nesty"]))
            apron = any(abs(x - nx) <= 1 and abs(y - ny) <= 1
                        for nx, ny in zip(world["nestx"], world["nesty"]))
            expected = {190} if nest else {189} if apron and h else families[h]
            actual = world["floor"][fy * 64 + fx]
            if actual not in expected:
                failures.append({"x": x, "y": y, "height": h, "cached_tile": actual,
                                 "expected_material_tiles": sorted(expected)})
    return failures


def run(rom, elf, core, out, limit):
    symbols = Symbols(elf, rom)
    emulator = EditorEmulator(rom, core, out)
    reader = ReadOnlyState(emulator, symbols)
    report = emulator.report
    report.update({"elf_sha256": symbols.sha256, "core_sha256": hashlib.sha256(core.read_bytes()).hexdigest(),
                   "scenario": "linked ANT EYE: natural weather, current floor, drowned pilots",
                   "baseline_source_commit": "9412119e7649dc0604b2e1572b8124b773baa9f9",
                   "terrain_events": [], "pilot_floods": [], "floor_failures": []})
    try:
        enter_linked_weather(emulator, reader)
        previous = [reader.world(s) for s in reader.capture()]
        check(report, "matching_elf_load_segments", True)
        check(report, "linked_weather_selected_through_controls", True)
        check(report, "initial_floor_matches_world", not any(floor_mismatches(w) for w in previous))
        check(report, "initial_shared_world", shared_world_equal(previous))
        print("INITIAL", [(w["gt"], w["mw"], w["mh"], pilots(w), len(floor_mismatches(w))) for w in previous], flush=True)
        end_frame = emulator.frame + limit
        while emulator.frame < end_frame:
            # Observe the scheduled real weather event closely enough to
            # distinguish the original ant dying from a later reused slot.
            # No RNG state or timing input is changed by these snapshots.
            step = 1 if 32 - previous[0]["gt"] % 32 <= 4 else 32
            emulator.tick(step)
            worlds = [reader.world(s) for s in reader.capture()]
            assert all(w["eye_tab"] and not w["lk_err"] and not w["over"] for w in worlds), "Weather session ended or lost synchronization"
            changes = [i for i, (a, b) in enumerate(zip(previous[0]["hgt"], worlds[0]["hgt"])) if a != b]
            if "cadence_observation" not in report and not changes:
                report["cadence_observation"] = {
                    "emulated_frames": step,
                    "game_frame_counter_advance": [(w["frc"] - old["frc"]) & 0xffff
                                                   for old, w in zip(previous, worlds)],
                    "game_tick_advance": [(w["gt"] - old["gt"]) & 0xffff
                                          for old, w in zip(previous, worlds)],
                    "scope": "linked-emulator observation before first disaster, not hardware FPS",
                }
            if changes:
                # An emulator frame can end partway through a hit() call.
                # Allow four ordinary frames for both peers to finish the
                # event and perform their next pilot-safety/input checks.
                emulator.tick(4)
                worlds = [reader.world(s) for s in reader.capture()]
                for _ in range(4):
                    if shared_world_equal(worlds):
                        break
                    emulator.tick(1)
                    worlds = [reader.world(s) for s in reader.capture()]
                changes = [i for i, (a, b) in enumerate(zip(previous[0]["hgt"], worlds[0]["hgt"])) if a != b]
                event = {"frame": emulator.frame, "game_tick": worlds[0]["gt"], "changed_cells": len(changes)}
                report["terrain_events"].append(event)
                check(report, "shared_world_after_disaster", shared_world_equal(worlds),
                      difference=shared_difference(worlds), **event)
                for port, world in enumerate(worlds):
                    failures = floor_mismatches(world)
                    check(report, "floor_after_live_terrain_change", not failures, port=port + 1,
                          frame=emulator.frame, mismatched_cells=len(failures))
                    if failures:
                        report["floor_failures"].append({"frame": emulator.frame, "port": port + 1,
                                                         "count": len(failures), "examples": failures[:8]})
                if len(report["terrain_events"]) == 1:
                    emulator.snapshot("05_first_live_disaster")
                print("TERRAIN", event, "floor-errors", [len(floor_mismatches(w)) for w in worlds], flush=True)
            flooded = []
            for role, pilot in enumerate(pilots(previous[0])):
                if pilot and pilot["alive"] and pilot["height"] and worlds[0]["hgt"][pilot["y"] * 128 + pilot["x"]] == 0:
                    flooded.append({"role": role + 1, **pilot})
            if flooded:
                originals = [[original_pilot(w, p) for p in flooded] for w in worlds]
                event = {"frame": emulator.frame, "flooded_pilots": flooded,
                         "original_ants_after": originals,
                         "after": [pilots(w) for w in worlds],
                         "colony_losses_before": [w["st_lost"] for w in previous]}
                report["pilot_floods"].append(event)
                emulator.snapshot("06_pilot_flood_response")
                print("PILOT FLOOD", event, flush=True)
                for port, items in enumerate(originals):
                    check(report, "original_flooded_pilot_dies_and_is_released",
                          all(not p["alive"] and not p["still_controlled"] for p in items),
                          port=port + 1, original_ants=items)
                for port, world in enumerate(worlds):
                    check(report, "replacement_pilots_are_dry_and_distinct",
                          all(not p or (p["alive"] and p["height"]) for p in pilots(world))
                          and (world["eye_ctl"] >= 48 or world["eye_ctl"] != world["eye_ctl2"]),
                          port=port + 1, pilots=pilots(world))
                    before = previous[port]
                    deaths = sum(before["ant"][i * 10 + 3] != 0
                                 and before["ant"][i * 10 + 2] == 0
                                 and world["ant"][i * 10 + 3] == 0
                                 for i in range(48))
                    loss_change = (world["st_lost"] - before["st_lost"]) & 0xffff
                    check(report, "lost_workers_counted_once", loss_change == deaths >= len(flooded),
                          port=port + 1, newly_dead_black_ants=deaths, recorded_losses=loss_change)
                break
            previous = worlds
        check(report, "natural_disaster_exercised", bool(report["terrain_events"]))
        check(report, "natural_pilot_flood_exercised", bool(report["pilot_floods"]))
        failures = [c["stage"] for c in report["checks"] if not c["passed"]]
        assert not failures, "Failed: " + ", ".join(sorted(set(failures)))

        # The existing protocol sends two hash bits per exchange and
        # rotates through all 32 bits in sixteen exchanges. Continue for
        # substantially longer than that with normal linked simulation.
        counters = [w["frc"] for w in worlds]
        emulator.tick(96)
        worlds = [reader.world(s) for s in reader.capture()]
        advances = [(w["frc"] - old) & 0xffff for old, w in zip(counters, worlds)]
        check(report, "continued_link_checksum_after_recovery",
              all(w["eye_tab"] and not w["lk_err"] and not w["over"] for w in worlds)
              and min(advances) > 16,
              emulated_frames=96, game_frame_counter_advance=advances)

        # Exercise the cache's lifetime after recovery: either player can
        # leave, and both can re-enter against the changed shared terrain.
        emulator.keys = [set(), {START}]
        emulator.tick(4)
        emulator.keys = [set(), set()]
        emulator.tick(100)
        emulator.snapshot("07_recovered_colony")
        worlds = [reader.world(s) for s in reader.capture()]
        check(report, "leave_eye_after_disaster", all(not w["eye_tab"] and w["display_mode"] == 0
                                                      and not w["over"] and not w["lk_err"] for w in worlds))
        emulator.keys = [{SELECT, START}, set()]
        emulator.tick(4)
        emulator.keys = [set(), set()]
        emulator.tick(100)
        emulator.snapshot("08_reentered_changed_world")
        worlds = [reader.world(s) for s in reader.capture()]
        check(report, "reentered_eye", all(w["eye_tab"] and w["display_mode"] == 1
                                            and not w["over"] and not w["lk_err"] for w in worlds))
        for port, world in enumerate(worlds):
            check(report, "rebuilt_floor_matches_changed_world", not floor_mismatches(world), port=port + 1)
        failures = [c["stage"] for c in report["checks"] if not c["passed"]]
        assert not failures, "Failed: " + ", ".join(sorted(set(failures)))
        report["passed"] = True
        print("ANT EYE terrain PASS: natural disasters update both caches, original pilots drown, shared recovery and re-entry remain playable", flush=True)
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        if emulator.last:
            emulator.snapshot("failure")
        raise
    finally:
        report["frames"] = emulator.frame
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        emulator.lib.retro_unload_game()
        emulator.lib.retro_deinit()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=pathlib.Path)
    parser.add_argument("core", type=pathlib.Path)
    parser.add_argument("--elf", type=pathlib.Path)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("build/eye-terrain"))
    parser.add_argument("--frames", type=int, default=32000)
    args = parser.parse_args()
    run(args.rom.resolve(), (args.elf or args.rom.with_suffix(".elf")).resolve(),
        args.core.resolve(), args.out.resolve(), args.frames)


if __name__ == "__main__":
    main()
