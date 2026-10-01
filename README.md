# EMPIRE-ANTS (GBA)
Native Game Boy Advance port of the GBC game: raw registers, no libraries. Build: `make` (ARM toolchain + devkitPro `gbafix`) -> `build/empire-ants.gba`.

New on GBA: 30x18 tile view, 24 ants per colony, EASY/NORM/HARD, game timer, battery-save records (wins + best time per difficulty), L/R shortcuts. Same controls as the GBC version (A/B raise/lower, SELECT flood, SELECT+A embezzle, SELECT+B fast forward, START pause/help).

**Status:** written and syntax-checked on a PC only. Never ARM-compiled or run on an emulator. Includes the interactive tutorial (title: A) and the first-person ANT EYE (SELECT+START), now a Mode-7 hybrid: BG2 affine floor with per-scanline HBlank-DMA perspective, smooth turning / gliding, distance fog (BLDALPHA), plus the old ray-marched columns on BG1 for hills, water and ants (hot code runs from IWRAM). The Danny Steel boot logo (src/logo.c, START skips) now plays at power-on. The GBC title theme is back (stereo, see below) and the ANT EYE status panel was rewritten. Also ported from GBC: roguelike perks after elections, flash floods / earthquakes, and win score + high score per level (the SRAM layout changed, so old saves reset). Check first: sound, BG1 overlay transparency, sprite priority, ANT EYE (Mode 7): floor/overlay alignment, first scanline of the DMA table, fog, and whether the overlay returns quickly after a turn.

## Title music (ported from the GBC build)
The title screen has its own stereo theme again (E phrygian-dominant, ~81 BPM, 16 bars): echo of the lead hard left, lead with vibrato hard right, a plucked drone bass in the centre on the wave channel, and tiny footsteps that ping-pong between the speakers. It fades in with the picture and out on START; the in-game loop takes over afterwards. The routing uses NR51 (high byte of SOUNDCNT_L), which is reset to centred sound when the title ends. Code: `tm_start` / `tm_update` / `tm_stop` in `src/main.c`, driven once per frame from `vsync()`.

Wave channel fix: the old code wrote `NR30 = 0xA0` (two-bank 64-sample mode). It is now `0xC0` (`WAVE_ON`: DAC on, play bank 1, the bank that was just loaded). Listen to the bass on a real build or an accurate emulator.

## ANT EYE panel
The four rows under the view now read:
```
FROM ANT 3/12            HEIGHT 1
FACING SOUTH WEST   AHEAD CLEAR
PAD: WALK/TURN  A: NEXT ANT
B: CURSOR HERE  START: LEAVE
```
`FROM` is which of your ants you ride (or `YOUR NEST`), `HEIGHT` the ground level under it, `FACING` the compass heading in full words, and `AHEAD` what one step forward meets: `CLEAR`, `CLIFF`, `WATER`, `EDGE`, `FRIEND`, `ENEMY`, `HOME` or `R NEST` (the red nest). The controls stay on screen. L / R now turn as well as LEFT / RIGHT, matching the tutorial card.
