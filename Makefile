# Needs an ARM toolchain (devkitARM or gcc-arm-none-eabi) and gbafix (devkitPro).
PREFIX ?= arm-none-eabi-
CFLAGS = -mcpu=arm7tdmi -marm -mthumb-interwork -O2 -Wall -ffreestanding -fno-builtin -nostdlib -fno-tree-loop-distribute-patterns -Isrc
all: build/empire-ants.gba
build/empire-ants.gba: src/mp.c src/chase.c src/chase_logic.h src/city.c src/extras.c src/empire.c src/empire_zone_scan.h src/battle.c src/main.c src/logo.c src/logo.h src/mission_catalog.h src/ambient_water.h src/world_variation.h src/eye_floor_detail.h src/eye_scene_motion.h src/terrain_food.h src/food_patch.h src/editor_nest.h src/practice_queen.h src/newgame_presets.h src/art.h src/art2.h src/font8.h src/maps.h src/crt0.s gba.ld
	mkdir -p build
	$(PREFIX)gcc $(CFLAGS) -T gba.ld -Wl,--gc-sections src/crt0.s src/main.c src/logo.c -lgcc -o build/empire-ants.elf
	$(PREFIX)objcopy -O binary build/empire-ants.elf $@
	gbafix $@ -t"EMPIRE-ANTS" -cEANT
clean:
	rm -rf build


# Native host-only regression checks; no ARM SDK or emulator needed.
HOSTCC ?= cc
.PHONY: test
test:
	mkdir -p build
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_terrain_food.c -o build/test_terrain_food
	./build/test_terrain_food
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_food_patch.c -o build/test_food_patch
	./build/test_food_patch
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_editor_nest.c -o build/test_editor_nest
	./build/test_editor_nest
	python3 tools/test_editor_menu_cache.py
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_practice_queen.c -o build/test_practice_queen
	./build/test_practice_queen
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_newgame_presets.c -o build/test_newgame_presets
	./build/test_newgame_presets
	python3 tools/test_newgame_preset_wiring.py
	python3 tools/test_practice_queen_integration.py
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_mission_catalog.c -o build/test_mission_catalog
	./build/test_mission_catalog
	python3 tools/test_mission_progress_source.py
	python3 tools/test_empire_worker_spawn.py
	python3 tools/test_battle_pixels.py
	python3 tools/test_battle_workload.py
	python3 tools/test_empire_worker_delete.py
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_empire_zone_scan.c -o build/test_empire_zone_scan
	./build/test_empire_zone_scan
	python3 tools/test_empire_zone_source.py
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_chase_logic.c -o build/test_chase_logic
	./build/test_chase_logic
	python3 tools/test_world_art.py
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_ambient_water.c -o build/test_ambient_water
	./build/test_ambient_water
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_world_variation.c -o build/test_world_variation
	./build/test_world_variation
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_eye_floor_detail.c -o build/test_eye_floor_detail
	./build/test_eye_floor_detail
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -pedantic tools/test_eye_scene_motion.c -o build/test_eye_scene_motion
	./build/test_eye_scene_motion
	python3 tools/test_eye_art.py
	python3 tools/test_classic_outcomes.py
	python3 tools/test_eye_world_integrity.py
	python3 tools/test_eye_compass.py
