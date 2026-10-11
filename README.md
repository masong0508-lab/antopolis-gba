# EMPIRE-ANTS (GBA)
Native Game Boy Advance port of the GBC game: raw registers, no libraries. Build: `make` (ARM toolchain + devkitPro `gbafix`) -> `build/empire-ants.gba`.

New on GBA: 30x18 tile view, 24 ants per colony, EASY/NORM/HARD, game timer, battery-save records (wins + best time per difficulty), L/R shortcuts. Same controls as the GBC version (A/B raise/lower, SELECT flood, SELECT+A embezzle, SELECT+B fast forward, START pause/help).

**Status:** builds with arm-none-eabi-gcc (`make`, then gbafix) and is tested in the mGBA core, including two linked mGBA cores for the link cable. Includes the interactive tutorial, the first-person ANT EYE, EMPIRE MODE, ANT CITY, ANTVERSE and 2 PLAYERS in every mode.

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

## EMPIRE MODE (title: L) - unlock it by winning one classic game, or with the Konami code on the title
Civilization + Age of Empires + Tropico + SimCity, played on the normal map screen.
- BUILD (\1 on your land): 28 buildings in tabs (L / R): farms, aphid ranches, granaries, chambers, nurseries, dirt pits, amber mines,
  markets, trade posts, vaults, archives, academies, barracks, armories, spitter nests, towers, walls, spike pits, healers, temples,
  gardens, dance halls, statues, palaces, embassies, outposts, bridges, and the GREAT MOUND wonder. Land tools: pile up, dig down.
- ZONES (SimCity): paint homes / farms / work zones; the colony builds there by itself when it needs to (H F W lights on the HUD).
- RESEARCH (Civ): 12 techs, 4 ages (Dirt, Leaf, Amber, Crystal). Borders grow around your buildings; outposts push them out.
- RULE (Tropico): farmers, builders, warriors and mystics each want different things. 8 edicts (feast, overtime, martial law,
  spore festival, conscription, rig election, tax break, propaganda). A vote every 3 minutes; lose it and a coup takes half your stores.
- WAR (Age of Empires): barracks train warriors, spitters and majors into the nest guard. At the nest: FORM AN ARMY. \1 on an army:
  march to a place, attack the nearest rival, go home, hold. Armies fight on the map; press L to zoom in to a pixel battle with up to
  500 ants a side. Towers, spitter nests, walls and spike pits defend. Rivals grow and send bigger and bigger armies.
- STORY: 7 chapters with Counselor Chitin. FREE PLAY: 0-3 rivals (calm / normal / brutal), any map and size from the title.
  Win by conquest, the Great Mound, or prosperity (50 ants and 70 happiness at a vote). SAVE AND QUIT / CONTINUE in free play.

## ANT CITY (home screen: CITY) and ANTVERSE
- ANT CITY is the SimCity mode: zone homes, farms and works, dig tunnels, build services (guard posts, clinics, schools, air shafts,
  dew pumps, parks, the stadium, the sky mound), set taxes and laws, read the advisors, data maps and graphs, and survive fires, floods and giant beetles.
- 10 scenarios (campaigns) plus FREE CITY. Win all 10 to open ANTVERSE: the city, the empire, rivals and god powers all at once.

## EL PRESIDANTE (Tropico mode for ANT CITY and ANTVERSE)
- Switch it on in OPTIONS > EL PRESIDANTE (OFF / FREE CITY / ALWAYS), on the FREE CITY and ANTVERSE setup screens
  (PALACE row), or per mission. The options group also sets how often the colony votes, rebels, coups, the superpowers,
  the hidden hand, palace events, how demanding the groups are, and what happens when you fall (game over or exile).
- The PALACE menu (START > PALACE): 8 GROUPS (workers, traders, mystics, soldiers, scholars, greens, natives, loyalists)
  with support bars, sizes and leaders you can flatter, bribe, jail or exile; LIFE (9 parts of happiness, respect,
  freedom); 15 EDICTS (food for all, literacy, martial law, festival, no spore juice, secret police, propaganda, tax the
  rich, open / closed borders, green law, pensions, crackdown, cult of me, amnesty); the CHARTER (votes, speech, faith,
  army, trade, ruler); VOTES (speeches, promises, rigging, call a vote); the two SUPERPOWERS (aid, trade deals, gifts,
  alliances, raiders); the HONEY POT (skim the taxes, buy loyalty, hire guards, pay the rebels, fund a vote); ORDER
  (rebels, coup defense); and the ADVISOR, who tells you what the groups are really doing.
- The hidden hand: unhappy groups quietly push the city around (strikes trim income, prophets send ants away, greens burn
  the works, natives feed the crime, soldiers plot). Happy groups help. The city never says who did it.
- Votes with speeches and rigging, coups, uprisings, superpower raids, events with two choices, exile and a comeback.

## ANTVERSE MISSIONS
- 50 missions from the ANTVERSE menu. Some use a single mode (classic, ANT EYE, EMPIRE, ANT CITY, EL PRESIDANTE), the
  rest mix everything. Goals, maps, twists (debt, fires, rebels, angry groups, hostile superpowers) and time limits.
  The PALACE toggle is on the mission screen for missions that allow either way. Finished missions are saved.

## Loading screens
- While a world is built, a fact appears: ANT FACT, BUG FACT, LIFE FACT, HUGE NUMBER or LAME FACT (123 of them, all
  checked). OPTIONS > LOADING FACTS switches them off.

## 2 PLAYERS (home screen: 2 PLAYER)
- Every mode works for two: VERSUS (black ants against red ants), CO-OP CLASSIC (two gods, one colony), CO-OP EMPIRE,
  CO-OP CITY and CO-OP ANTVERSE.
- PLAY ON: SAME GBA = take turns with a turn clock (30 to 120 seconds) and a "pass the GBA" card between turns.
  LINK CABLE = two GBAs at the same time. Both pick 2 PLAYERS > LINK CABLE > START; the first GBA (player 1) sends its game,
  map, options and cheats to the other one. Each player has a cursor (the other one shows in red). START opens your menu on
  both screens. If the cable comes out, both GBAs say LINK LOST and go back to the menu.
- The link cable needs real hardware or an emulator with GBA link (multiplayer) support. SAME GBA works everywhere.

## Extras (home screen: EXTRAS)
- ACHIEVEMENTS: 64 trophies for every mode (classic, ANT EYE, Empire, City, Antverse, 2 players), saved on the cartridge.
  Many unlock rewards.
- UNLOCKABLES: ant colours GOLD, WHITE, PINK, SHADOW and menu styles AMBER, CRYSTAL (pick them in OPTIONS once unlocked).
- CHEATS (23, most with levels): infinite AP, god queen, weather, rich colony, all techs, mega army, party ants, turbo,
  peaceful rivals, instant zones, food rain, strong ants, big brood, super ant, iron jaws, money tree, no fires, giant floods,
  loyal masses, rigged votes, and 3 secret cheats with their own button codes. Each is unlocked by a trophy, or all at once with
  UP DOWN LEFT RIGHT UP DOWN LEFT RIGHT SELECT on the cheats tab. Trophies are not earned while a cheat is on (except CHEATER).
- RECORDS: wins, best times and high scores per level, trophies, story progress, city goals, what is open.
## Empire Mode additions
- RIVALS screen (diplomacy): offer peace (20 amber), demand tribute, declare war.
- WORLD MAP: the whole land on one screen with your nest, armies and rivals.
- Random events with Counselor Chitin: traders, rain, drought, locusts, refugees, golden days, cave-ins, amber veins,
  fungus blight, wandering warriors.
