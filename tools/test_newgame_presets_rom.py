#!/usr/bin/env python3
"""Exercise both GBA presets in the actual linked-capable mGBA cartridge.

Only normal buttons are written. Read-only emulator snapshots recover
the real compiler/ELF symbols, verifying preview and gameplay settings
without patching game state, ROM or battery SRAM.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from linked_smoke import A, L, R
from test_editor_roundtrip import EditorEmulator
from test_eye_terrain import Symbols, ReadOnlyState
from test_campaign_menu import expect_text

NAMES=("diff","mapt","tw","th","ng_learn","sandbox","tsel")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("rom",type=Path)
    ap.add_argument("core",type=Path)
    ap.add_argument("--elf",type=Path,required=True)
    ap.add_argument("--out",type=Path,default=Path("build/newgame-presets"))
    args=ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    emu=EditorEmulator(args.rom.resolve(),args.core.resolve(),args.out.resolve())
    symbols=Symbols(args.elf.resolve(),args.rom.resolve())
    read=ReadOnlyState(emu,symbols)
    emu.report["elf_sha256"]=symbols.sha256
    emu.report["core_sha256"]=hashlib.sha256(args.core.read_bytes()).hexdigest()
    try:
        emu.tick(850)
        emu.press(A,released=180)  # PLAY on home hub opens NEW GAME
        emu.snapshot("01_newgame_controls")
        expect_text(emu,13,19,"L GUIDE", "guided_button_visible")
        expect_text(emu,21,19,"R WILD", "wild_button_visible")
        expect_text(emu,2,3,"SEL: RESEED","seed_button_visible")
        for key,label,want in [
            (L,"guided",(0,0,48,48,1,0,6)),
            (R,"wild",(2,3,64,64,0,0,6)),
        ]:
            emu.press(key,released=160)
            emu.snapshot(f"02_{label}_preview")
            values=[]
            for state in read.capture():
                fields=tuple(read.value(state,name) for name in NAMES)
                values.append(fields)
                assert fields==want, f"{label}: expected {want}, got {fields}"
            expect_text(emu,3,15,
                        "GUIDED LESSONS" if label=="guided" else "HARD SURVIVAL",
                        f"{label}_description_visible")
            emu.report["checks"].append({"preset":label,"values":values,"passed":True})
        emu.press(A,released=1200)  # start Wild: HARD CANYON, 64x64
        emu.snapshot("03_wild_colony_playable")
        for state in read.capture():
            assert read.value(state,"mw")==64 and read.value(state,"mh")==64
            assert read.value(state,"diff")==2
            assert read.value(state,"tut")==0
        emu.report["checks"].append({"playback":"Wild starts real 64x64 HARD game","passed":True})
        emu.report["passed"]=True
        print("GBA preset ROM PASS: visible buttons, both settings, real wild gameplay")
    except Exception as exc:
        emu.report["passed"]=False
        emu.report["error"]=str(exc)
        if emu.last: emu.snapshot("failure")
        raise
    finally:
        emu.report["frames"]=emu.frame
        (args.out/"report.json").write_text(json.dumps(emu.report,indent=2)+"\n")
        emu.lib.retro_unload_game()
        emu.lib.retro_deinit()

if __name__=="__main__":
    main()
