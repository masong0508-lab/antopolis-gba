## Bonus campaign expeditions (GBA)

The MISSIONS menu now offers **56 playable missions**. Four optional
append-only expeditions bring more approachable objectives to existing
modes: **SCOUTS FIRST** (deliver three food loads in ANT EYE),
**NEST GUARD** (win Classic without black queen damage),
**ROOT TOWN** (construct 12 Empire buildings in 20 minutes, no rivals)
and **PARKS AND PATHS** (reach 750 city ants with smog below 20).

Use **UP/DOWN** to choose, **L/R** to jump five missions, **A** to play
and **B** to return. The original mission indexes 1-52 remain intact,
including GRAND FINALE at #52; the new missions occupy four previously
unused completion bits in the same seven-byte cartridge record.
Existing games, achievements, and completed-mission flags are preserved.

## One-button Guided and Wild game starts (GBA)

The NEW GAME menu now offers **L: GUIDE** and **R: WILD**.
They are optional quick-start configurations and use the existing,
fully tested game rules—not extra game modes or hidden cheats.

- **L / GUIDED START:** EASY, procedural MEADOW, 48×48 world,
  interactive LESSONS ON, SANDBOX OFF. A manageable world with explicit
  lessons about moving, food, terrain ramps and the queen.
- **R / WILD SURVIVAL:** HARD, procedural CANYON, 64×64 world,
  LESSONS OFF, SANDBOX OFF. Larger terrain and tougher strategy for
  returning players.

Both immediately regenerate the right-hand map preview, visibly update
all settings and select START GAME. You can still adjust any setting
afterward, or use SELECT to change the procedural map seed. The NEW
GAME screen also explains the controls and shows whether the current
settings enable guided lessons, free practice or hard survival.

These quick starts do **not** overwrite custom MY MAP data, records,
options, achievements or saves. They add no new persistent buffers or
gameplay-frame processing; a small pure preset helper is validated by
host tests and the existing two-console ROM release workflows.

## Faster Map Maker menus on large authored maps

Previously **every movement of the selection cursor in Map Maker's
pause menu** rescanned all food in a 32×32 to 96×96 map and performed a
complete nest-to-nest breadth-first path search. A 96×96 map has 9,216
cells, so merely navigating to WIDTH or SAVE could repeatedly do work
whose results had not changed.

The editor now caches just **the berry count and reachability result**.
On opening the menu after painting/moving nests, it recomputes both
once. Moving the menu highlight or confirming a quit no longer scans
the world. Changing WIDTH/HEIGHT or using CLEAR TO GRASS invalidates
the summary, which is recomputed before the next screen draw. Every
legal tile, food marker and queen placement is still taken from the
current map, and the existing editor-save/emulator tests remain gates.

This adds only a 16-bit count and two single-byte cache flags (four
bytes total), not another world map, heap buffer, ray cast or simulation
cost. The save format and Map Maker's displayed messages are unchanged.
The CI integration check ensures both full-map operations remain under
the dirty guard, and the original 96×96-to-32×32 queen-resize test
continues to exercise cache invalidation through the real ROM.

## Tutorial/sandbox queens: no false knockouts

The overhead simulation and ANT EYE previously applied the last point
of damage to a protected practice queen, then relied on a later
match-result check to restore her health. This could momentarily play
the queen-death sound, show a false defeat message, mark a queen loss,
or award the RED QUEEN defeat achievement during a practice game.

The actual soldier attack and first-person A-button bite now share one
small damage-permission rule. In tutorials and sandbox, queens retain
their last heart even under repeated attacks; normal matches and
linked versus still allow queen defeat. A last-heart practice bite
responds **PRACTICE QUEEN SAFE** rather than announcing a false win.

The change preserves the attack RNG sequence, existing saves,
achievement IDs, all initial queen health, and the original
simulation-frame budget. It adds no world buffers, heap memory,
rendering passes or permanent RAM. Native tests cover 128 combinations
of hearts/modes with repeated attacks; CI additionally checks that
both real production damage sites use the tested guard, and builds/
runs the full linked-emulator release suite before merge.

## Map Maker queen protection after changing world size

Shrinking a custom map previously clipped each queen's x/y coordinates
independently to the same last usable location. If both original nests
fell outside the smaller width and height, the editor could place
**both colonies on one square**, report an invalid path as connected,
and silently discard one queen when saving MY MAP (the red marker
overwrote the black marker).

The resize operation now preserves every still-valid nest, clamps
cropped coordinates as before, and, if the two would coincide, shifts
only the second queen **four cells sideways** to a valid distinct tile.
The new nest locations are always dry and cannot hide berries or
pheromone underneath. This runs only when the player edits the map
dimensions: no persistent arrays, heap memory, random draws, or
per-frame simulation work. Existing SRAM map records are unchanged.

The native C99 suite checks **33,696** layouts across all 81 standard
world sizes, including repeated resizes and out-of-range coordinates.
The ROM-emulator suite starts with 96×96 authored saves containing two
offscreen nests, uses the editor's own menu to shrink to 32×32, and
checks that **both queen markers survive independently** in valid SRAM
with correct checksum and intact unrelated berries.

## ANT EYE movement recovery and reliable compass goals

A worker whose center is still on dry ground can now recover when a
neighboring flood or cliff change overlaps its feet. After a fully blocked
movement request, an already invalid footprint is moved into the safe
interior of that same cell. The correction is at most 40/256 of a cell per
axis. It cannot cross a cell boundary or rescue an underwater center;
ordinary wall collision, corner sliding and one-level ramps keep their
previous behavior. Solo, linked ANT EYE and DUAL VIEW share this helper.
The link compatibility revision is now A77, so pairing with an older
cartridge shows UPDATE BOTH GBAS before a match can start.
The native production-code regression covers eight hazard directions,
three movement speeds, world edges, no-input behavior and normal collision.

Compass goals now remain visible when several markers share a bearing.
Food has priority while foraging and home has priority while carrying.
Co-op teammate markers preserve that active goal. DUAL VIEW keeps the
guide's waypoint prominent while exploring and home prominent while
carrying, using one food-goal bearing. Marker positions are unchanged;
the fix uses the existing tile map without another world scan or buffer.

## ANT EYE terrain and worker safety

Floods and earthquakes now update the first-person floor where terrain
changes, without waiting for the player to move far enough to rebuild
the camera's map. The update changes only the affected packed tile byte
and preserves its neighbor. It reuses the existing 64x64 floor cache;
there is no additional world array or full-map rebuild on ordinary frames.
Flooded ground beside a nest also renders as water, while the nest itself
and its dry surrounding dirt retain their appearance.

Solo and linked pilots use the same drowning check as the colony
simulation. A submerged ant cannot walk away, collect or deliver food,
lay scent, or bite after its death. Linked worker selection skips flooded
ants and uses the existing replacement-worker or spectator behavior.
Death statistics are counted once, and both linked consoles process the
same world events without new random draws or protocol messages.

The native regression checks all 81 supported world sizes and packed
cache boundaries, compares incremental updates with complete rebuilds,
and verifies unchanged dry terrain art. The real-ROM weather regression
selects THE MAZE and WEATHER CHAOS through ordinary controls, then reads
emulator snapshots to compare each console's terrain and floor cache and
check the flooded pilot's death and replacement. It never loads a
snapshot or patches live game memory. The matching ELF is checked against
the cartridge bytes before its symbols are used; CI retains both files.

## ANT EYE objectives and match endings

Classic and ANT EYE now share the same outcome checks. Finishing a
first-person mission objective opens the mission result automatically;
players no longer have to leave ANT EYE to receive completion credit.
The Classic match clock and campaign deadline also keep working in
first person, including linked Classic co-op. Timed results count the
current surviving ants before deciding the winner.

Tutorial and sandbox games retain their queen protection in ANT EYE,
including after simulation damage. Ordinary combat keeps its existing
queen-defeat precedence, and an already decided result is preserved.
The checks use existing state and add no save fields or world buffers.

The native regression exercises the production outcome function at
deadline boundaries and in protected modes. The real-ROM regression
boots two mGBA instances, selects SCENT MASTER with controller input,
lays scent, and requires the result card and mission-9 save bit without
pressing START to leave first person. It then checks the campaign menu
reports 1/52 completed. The previous release fails that regression at
the automatic-result check. Existing linked co-op, DUAL VIEW, Map Maker,
campaign-menu and HUD checks remain part of the release workflow.

## Playability pass: readable HUDs, reliable maps, complete campaign

- **Readable during play:** the overhead HUD, ANT EYE status panel and
  first-person compass have a solid dark backdrop. Text no longer competes
  with moving soil and landscape texture. The existing reserved panel tile
  is reused; no framebuffer, larger atlas or additional world array is needed.
- **Clear resources and actions:** Classic labels FOOD and MOOD explicitly.
  ANT EYE labels colony FOOD, AP and HELD food, keeps walk/turn and sidestep
  controls visible, and gives a forage or return-home task. Water, cliffs,
  world edges and enemies take priority over carrying or scent reminders.
  Food compass hints use the renderer's existing nearby-food result; they
  indicate a target bearing, not a guaranteed walkable route.
- **Map Maker keeps your work:** opening an authored map loads its saved
  cells directly. Reopening no longer reduces berries to 16 regrowth centers
  or silently cuts paths through a disconnected shoreline. Route repair and
  additional food scatter still happen when a match starts.
- **Both final missions restored:** the campaign contained 52 authored entries
  but declared only 50, so the compiler discarded EVERYTHING and GRAND FINALE.
  The total now comes from the catalog, and menus use that total. All previous
  mission indexes and the seven-byte completion bitmap remain compatible.

Validation includes native logic/art checks, an ARM cartridge build, real
two-console mGBA Classic co-op and DUAL VIEW sessions, and an actual-ROM
Map Maker regression. The editor regression compares saved cells/checksums
through repeated saves and an intentional one-berry erase in 32x32, 96x32,
32x96 and 96x96 worlds. Campaign checks load an old 50-completion record,
complete the restored missions, and guard adjacent SRAM bytes. Emulator
results do not certify physical link-cable timing or hardware frame rates.

## Map Maker: painted berry tiles now appear in gameplay (November 2026)

Hand-made maps (including saved MY MAP) previously treated each `f`
letter as a *candidate regrowth center* only. Random scatter might
place berries near that tile but not on the painted tile itself. On
maps with more than 16 painted berry cells, the 16-center limit could
silently discard the other marked positions from the starting world.

Now **every painted `f` tile is an actual initial food tile on that
exact square**. The existing 16-center distribution remains in place
for randomized neighboring patches and future regrowth. Even a world
with hundreds of authored berries starts with every marked berry
present. Existing unmarked terrain, nest coordinates, fixed food
storage and random-number sequence are preserved. The import assigns
the food flag during the existing map-cell pass: no extra heap, extra
persistent buffers or per-frame scans.

The native regression checks actual `food_patch_authored_tile`
cartridge logic across all 81 map dimensions, sparse and >16-berry
worlds, impassable water, and nest symbols. Linked two-console
emulation and ARM compilation remain required before merge.

## Starter forage reliability — river, islands and narrow maps

Procedurally generated worlds place a food patch near each starting
colony. Previously each colony tried twenty random coordinates and
stored the final coordinate even when it was water or unreachable. Even
valid patch centers could generate no nearby food after the five-plus
random placement attempts; some fresh games could begin with neither
side able to find food.

World construction now falls back to a **traversable, two-cell nest
perimeter** when the random placement budget fails and also seeds one
guaranteed usable berry on that accessible perimeter for **each** colony.
The scan uses the existing height map only, checks water/cliffs and both
queen exclusions, and runs only when the world is created. It adds no
new persistent map, allocations, random draws or per-frame work.
Hand-made maps and Map Maker berry placement are unchanged.

The native suite exercises all **81** supported procedural map sizes,
both colony starting positions, sparse shoreline fallback, unwalkable
cliffs and safe failure when no land exists. ARM compilation and the
linked two-GBA mGBA playthrough must pass before merging this change.

## Map Maker milestone: spatially fair food patches (November 2026)

Hand-made maps can place berries anywhere, but the cartridge has 16
preallocated patch centers. Previously the loader silently used only the
first sixteen `f` cells in top-to-bottom scan order, so a large custom
96x96 map could place all active food centers in the upper-left area.
The import pass now chooses one authored berry center from each occupied
region of a fixed 4x4 grid first, then fills unused center slots from
other distinct marked cells. This handles 32x96 and 96x32 maps without
bias and uses no heap, RNG, added persistent world arrays, or per-frame
processing. Maps with sixteen or fewer marked patches preserve *all*
their original centers and their previous scan order.

The map editor now reports `99+` when a custom map exceeds 99 berries
instead of silently showing 99, and labels over-cap maps `16 REGROW
SITES`. Dense maps retain their visible berry drawings, but only 16
patch centers can replenish their surrounding tiles in a match.
`make test` exercises the shipped selector with small, dense,
single-sector, rectangular, and 50 deterministic random worlds.
Linked-mGBA and full ARM ROM builds remain required before merging.

## Milestone: terrain-edit clarity and flooded-food safety (November 2026)

The bottom map HUD now warns **B LOSES FOOD** when the cursor is over
berry-bearing sand that would become water. Raising and lowering tiles
show meaningful outcome messages rather than silently changing the
world. Flooding distinguishes ordinary terrain changes from berries
washed away. Water still blocks ants and submerged berries disappear,
including after earthquakes. All feedback is based on the actual
post-edit tile and berry state, not an assumed action.

The same pure food/height rule and native regression as GBC cover
every tile height and sinking transition. This is visual feedback and
data-consistency work only: no new mode, permanent state, save
layout, framebuffer, link-packet fields, or additional rendering pass.
The flood warning is calculated during the existing affected-cell loop.

# EMPIRE-ANTS (GBA)
Native Game Boy Advance port of the GBC game: raw registers, no libraries. Build: `make` (ARM toolchain + devkitPro `gbafix`) -> `build/empire-ants.gba`.

New on GBA: 30x18 tile view, 24 ants per colony, EASY/NORM/HARD, game timer, battery-save records (wins + best time per difficulty), L/R shortcuts. Same controls as the GBC version (A/B raise/lower, SELECT flood, SELECT+A embezzle, SELECT+B fast forward, START pause/help).

**Status:** builds with arm-none-eabi-gcc (`make`, then gbafix) and is tested in the mGBA core, including two linked mGBA cores for the link cable. Includes the interactive tutorial, the first-person ANT EYE, EMPIRE MODE, ANT CITY, ANTVERSE and 2 PLAYERS in every mode.

## Title music (ported from the GBC build)
The title screen has its own stereo theme again (E phrygian-dominant, ~81 BPM, 16 bars): echo of the lead hard left, lead with vibrato hard right, a plucked drone bass in the centre on the wave channel, and tiny footsteps that ping-pong between the speakers. It fades in with the picture and out on START; the in-game loop takes over afterwards. The routing uses NR51 (high byte of SOUNDCNT_L), which is reset to centred sound when the title ends. Code: `tm_start` / `tm_update` / `tm_stop` in `src/main.c`, driven once per frame from `vsync()`.

Wave channel fix: the old code wrote `NR30 = 0xA0` (two-bank 64-sample mode). It is now `0xC0` (`WAVE_ON`: DAC on, play bank 1, the bank that was just loaded). Listen to the bass on a real build or an accurate emulator.

## Quick start: what am I supposed to do?

**PLAY / Classic:** Your black ants automatically gather food, hatch workers and
march toward the red colony. **Protect your black queen, and help your ants
reach and defeat the red queen.** You do not move every ant manually: you move
the cursor with the D-pad and use **A to raise land** or **B to lower land**,
building safe routes across lakes, cliffs and enemy approaches. **AP**
recharges over time and pays for terrain changes; food is delivered by
workers and can be traded into extra AP with SELECT+A. The HUD now labels
AP, food and popularity directly. On the first ordinary match, a brief
mission card explains the win condition. Press START for HELP/PAUSE, or
SELECT+START to walk around the real world as an ant.

**Two players:** choose the 2 PLAYER title card for VERSUS, CLASSIC CO-OP,
DUAL VIEW and the larger empire/city modes. For simultaneous control,
each GBA must run the **same current ROM** over a working GBA link cable.
The two players see their *own* screens, not one mirrored view.

**DUAL VIEW (GBA link only):** a cooperative 2D/3D expedition in the
same live ant colony world. One GBA player is the **GUIDE**, using the normal
overhead world map. D-pad moves the map cursor, **A** marks reachable terrain
for the scout and **B** jumps the guide cursor to the scout's live position.
The other player is the **SCOUT**, inhabiting a real worker ant in ANT EYE.
Move and turn with the D-pad, strafe with L/R, hold **A** for stamina-limited
sprinting and hold **B** to lay scent. Find berries (the `%` compass cue),
carry **three loads** to the home nest (`$`), and follow a guide waypoint
(`?` compass cue) for a one-time stamina boost. Guide markers must be at
least three tiles from the scout and placed on reachable land. If placement
fails, the guide HUD now distinguishes water, unreachable terrain and being
too close to the scout instead of responding with an unexplained buzzer.
This keeps route planning understandable without granting free stamina.

Each run lasts **90 seconds**. Then the players swap perspectives, so both
get to experience the overhead colony and immersive ant world. Every food
delivery adds one shared team point; completing the three-delivery objective
adds a three-point bonus. Two runs produce a **team score**, not a
hunter-versus-prey winner. Newly available food is replenished deterministically
near home, and the overhead view shows the scout's fractional movement
rather than snapping between tiles. Both linked devices use the same world,
supplies, waypoints and score; only camera presentation differs. This is local
GBA cable/emulator multiplayer, not network play. The internal source name
`src/chase.c` is kept for build compatibility, but it no longer describes the
game mode's rules.

## Dual View camera focus and low-overhead status panels

The guide now **starts focused on the teammate's actual scout position**,
not the far-away enemy queen's nest. The guide still needs to move the
cursor to reachable ground at least three tiles from the scout before
placing a waypoint; B always recenters the cursor on the moving scout.

The linked guide and first-person scout HUDs also cache five tiny
presentation values (seconds, deliveries, marker state, task detail,
carried food). The game redraws its text panel only when a displayed
value changes instead of wiping three or four tile rows every eight
frames. The live 3D compass, camera, world simulation and two-player
checksum continue to update at their original cadence. This eliminates
redundant text-VRAM writes without new framebuffers or network state,
and includes native cache tests plus real linked-mGBA smoke validation.

## Dual View link-screen synchronization (November 2026)

Dual View now treats the rules, between-run results and final score as
**shared two-player Ready gates**. Both players must press A or START after
the screen appears; holding a button from the preceding menu does not
silently confirm. Both linked clients wait until **both** players release
their confirmation buttons before starting the next round or exiting to the
menu. This fixes a possible split-state transition in which one cartridge
would proceed while its partner was still looking at a results screen.
The linked-mGBA smoke test now deliberately staggers both players' Ready
presses and checks that the scout does not enter first-person mode early.

## Multiplayer and ANT EYE refinements (November 2026)

- **GBA link protocol:** the compatibility code is now a stable explicit protocol revision, not a compile-time clock stamp. Identical protocol ROMs rebuilt at different times no longer fail the version check solely due to different build timestamps. Test actual cable lockstep and disconnection on two mGBA instances before shipping.
- **ANT EYE collision:** the controlled ant now checks a body-sized footprint against the same water and cliff limits as 2D ants and slides along blocked terrain without falsely reporting a step. Combining forward movement and sidestepping no longer creates an unintended speed boost.
- **Legibility and objectives:** visible food is prioritized ahead of decorative grass/rocks in the 80-object pool; ambient props are sparser. The first-person HUD names real-world obstacles, food, nests, and nearby enemies straight ahead.
- **Two simultaneous 3D pilots over GBA link:** Both players select `2 PLAYERS > CO-OP CLASSIC > LINK CABLE` with matching ROM versions. Once in the normal strategy map, either player presses **SELECT+START** to enter synchronized ANT EYE. Each GBA renders its own first-person camera and steers its own black worker, while both consoles apply P1/P2 input to the same simulation. **D-pad** walks/turns, **L/R** sidestep, **A** bites, **hold B** lays pheromone trails, **SELECT** chooses another unclaimed worker, and **START** returns both to the strategy view. Food collection, queen damage, and worker deaths change the shared world; each pilot's temporary hit points are tracked during the first-person session. Two pilots cannot claim the same ant.
- **Local co-op remains available:** SAME GBA > CO-OP CLASSIC uses alternating turns, with single-ant ANT EYE from the pause menu. GBA LINK CABLE Versus, Empire, City and Antverse retain strategy-map play. Multiplayer is local hardware/emulator cable, **not Internet/Wi-Fi**.
- **Synchronism protection:** Both pilots update in deterministic P1-then-P2 order. The cable checksum includes each pilot's position and carried food. The transfer path now publishes the outgoing word before clocking the cable and checks echo, sequence, hardware player ID and framing errors. Test real-hardware timing before shipping.
- **Co-op content:** Deliver five food loads while piloting ants for a +5 AP shared supply-run bonus. A delivery can also heal a teammate near the nest. Supply-run progress persists between entering and exiting 3D in the same match.
- **Rendering optimization:** ANT EYE reuses precomputed affine matrix scales rather than 16 division calls every frame, and reduces distant ambient decorations while preserving near detail, food and living ants.
- **Automated dual-emulator test:** GitHub Actions builds a linked mGBA Splitscreen core, boots two copies of the ROM, drives the link lobby and attempts two-player ANT EYE. It inspects each cartridge's save-RAM achievement flags to distinguish genuine multiplayer entry from a boot-only test, and uploads paired frame captures. This is not physical hardware certification.

## ANT EYE panel
The regular first-person view shows a compass at the top, with nearby
food, enemy ants and home/enemy nests indicated by distinct markers.
Its four-line status area shows the controlled ant's health, food/AP
resources, nearby hazards, and the actual actions available: A **bite**,
hold B **lay pheromone scent**, L/R **sidestep**, SELECT **switch ant**,
and START **return to the overhead map**. The game displays contextual
obstacle warnings (water, cliffs and enemies) instead of pretending a
blocked step is movement. The cooperative DUAL VIEW version of this
panel deliberately uses **different controls** (A sprints, B leaves
scent) and a prominent food-delivery objective and round timer.

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
- 56 missions from the ANTVERSE menu (52 original + 4 bonus expeditions). Some use a single mode (classic, ANT EYE, EMPIRE, ANT CITY, EL PRESIDANTE), the
  rest mix everything. Goals, maps, twists (debt, fires, rebels, angry groups, hostile superpowers) and time limits.
  The PALACE toggle is on the mission screen for missions that allow either way. Finished missions are saved.

## Loading screens
- While a world is built, a fact appears: ANT FACT, BUG FACT, LIFE FACT, HUGE NUMBER or LAME FACT (123 of them, all
  checked). OPTIONS > LOADING FACTS switches them off.

## 2 PLAYERS (home screen: 2 PLAYER)
- VERSUS, CO-OP CLASSIC, CO-OP EMPIRE, CO-OP CITY, CO-OP ANTVERSE and the new **DUAL VIEW**. DUAL VIEW is exclusively for two linked GBAs; the others support same-console play.
- The previous modes work for two: VERSUS (black ants against red ants), CO-OP CLASSIC (two gods, one colony), CO-OP EMPIRE,
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
