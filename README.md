# EMPIRE-ANTS (GBA)
Native Game Boy Advance port of the GBC game: raw registers, no libraries. Build: `make` (ARM toolchain + devkitPro `gbafix`) -> `build/empire-ants.gba`.

New on GBA: 30x18 tile view, 24 ants per colony, EASY/NORM/HARD, game timer, battery-save records (wins + best time per difficulty), L/R shortcuts. Same controls as the GBC version (A/B raise/lower, SELECT flood, SELECT+A embezzle, SELECT+B fast forward, START pause/help).

**Status:** written and syntax-checked on a PC only. Never ARM-compiled or run on an emulator. Includes the interactive tutorial (title: A) and the first-person ANT EYE (SELECT+START). The Danny Steel boot logo (src/logo.c, START skips) now plays at power-on. Also ported from GBC: roguelike perks after elections, flash floods / earthquakes, and win score + high score per level (the SRAM layout changed, so old saves reset). Check first: sound, BG1 overlay transparency, sprite priority, ANT EYE speed.
