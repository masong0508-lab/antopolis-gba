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

## Remaster update
- New readable 5x7 font and a 3-row HUD with icons: hearts for both queens, ant counts, AP bar, food, popularity, time.
- Bottom line tells you what is under the cursor and what A / B will do there. Cursor turns red where you can't edit.
- New terrain art: soft edges = steps ants can walk, dark edges = cliffs they can't. Water shimmers.
- 4 maps on the title (UP/DOWN pick a row, LEFT/RIGHT change it): MEADOW, RIVER, LAKES, CANYON. There is always a path between the nests.
- Food grows in berry patches; ant trails show on the ground; nests are big mounds with a 16x16 queen.
- Ants find their way home and to the enemy around water and cliffs (pathfinding), and walk smoothly.
- SELECT jumps the cursor: alert spot, else HOME > ARMY > RED NEST. Alerts when your queen is hit or food is stolen.
- Perks are picked on their own card; a VICTORY / DEFEAT screen with stats; pop-ups (+1, splash, puff, heart) and screen shake.
- ANT EYE rebuilt: sky with clouds, hills and trees that turn with you, smooth terrain-coloured ground, scaled ants / queens / food standing on it, a compass bar (crown = home, red ant = red nest), clear AHEAD warnings, slower calmer walking and turning.

## Maps update
- Map size: pick WIDTH and HEIGHT separately on the title screen (32 to 96 cells each, in steps of 8: 81 sizes). Big maps scroll smoothly.
- Hand-made maps in `src/maps.h`: THE MOAT, TWIN ISLES, THE MAZE. Make your own with the Map Maker app and paste them at the end of the list.
- INFINITE map: a generator driven by a 320-bit seed (about 2 x 10^96 possible seeds, more than the atoms in a trillion universes).
  It mixes hills, dents, lakes, 0-2 rivers with fords, noise, smoothing, cliffs, mirror / point symmetry and 4 nest layouts.
- Easier controls: button pictures in every hint, ANT EYE VIEW and TRADE FOOD FOR AP in the pause menu, navy panels.
- Paths are searched in the background (no stutter on big maps); faster cartridge wait states.

## In-game map editor
- On the title screen tap SELECT. The editor opens with the map you picked (or a fresh grass map for MY MAP).
- D-pad moves, A paints (hold A and move to paint lines), L / R change the tool (water, sand, grass, hill, berries, your nest, red nest),
  SELECT changes the brush (1, 3, 5), B picks the tool from the square under the cursor, START opens the editor menu.
- Editor menu: keep editing, save and play, save and quit, width / height (32 to 96), clear to grass, quit without saving.
  It also shows the berry count and whether ants can walk from nest to nest.
- The map is saved on the cartridge (battery save) and shows up as MY MAP on the title screen.

## Options (title: R, or OPTIONS in the pause menu) - 27 settings, saved on the cartridge
- GAME RULES: preset (KIDS / CLASSIC / HARD / CUSTOM), start ants, start AP, AP refill, red ants, red attacks, my army goes,
  queen hearts, disasters, elections, perks, food, food regrows, time limit (the side with more ants wins when time is up)
- CONTROLS: start speed, cursor speed, button hints, alerts
- LOOK: ant trails, screen shake, pop ups, panel color (navy / black / forest / plum), your ants (black / blue / green / purple)
- SOUND: music, sounds
- ANT EYE: walking speed, sky (day / sunset / night)
- Every option has a two-line explanation. SELECT on the options screen puts everything back to normal.

## Termite look
- Menus are dug-out soil chambers with bark edges; the HUD is the dirt under the map with grass hanging into it.
- The title is an ant farm: tunnels in the soil, the ants march through the bottom tunnel.
- Text has a soft shadow and no box behind it; menus point with a little ant; dividers are twigs with a leaf.
- OPTIONS > MENU STYLE: SOIL, CLAY, MOSS or NIGHT.
