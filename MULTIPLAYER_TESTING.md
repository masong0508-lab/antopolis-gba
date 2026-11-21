# Multiplayer validation: GBA

The normal artifact is **empire-ants.gba**, from GitHub Actions > Build ROM.
Use the **same current ROM bytes** on both connected GBA consoles or two
linked mGBA cores. This is local GBA link-cable multiplayer; not IP networking.

## Smoke test A: existing shared world

1. Start both copies. Choose **2 PLAYERS > CO-OP CLASSIC > LINK CABLE**,
   and START on both consoles.
2. Confirm both participants reach the same map, with independent cursors,
   and that changing land, carrying food, and spawning ants is reflected
   in both games.
3. Play for several minutes. A LINK LOST or OUT OF SYNC screen is a failure
   unless the cable was deliberately disconnected.

## Smoke test B: simultaneous 3D co-op (new)

1. While the linked Classic co-op map is running, press **SELECT+START**
   on either controller. Both screens must enter ANT EYE without requiring
   the other player to press a button.
2. P1 and P2 should receive **different living black workers**, and each
   GBA should show its own camera, not mirror the other player's view.
3. Hold UP on P1 while P2 stays idle. Only P1's worker should travel.
   Now walk and turn with P2; each worker must stay visible to the other
   and neither may pass through impassable water or cliffs.
4. Collect food with either worker and return to the black nest. The food
   tile disappears in both maps, stock/AP increases in the shared game,
   and the carrying indicator clears.
5. Hold B to lay a scent trail and check later that ordinary workers use it.
   Press A next to a red ant or enemy queen; the opponent's health or living
   ant count must reflect the hit on both consoles.
6. Press SELECT to swap workers. Neither player may take control of the
   worker the other is already piloting.
7. Press START on **either** GBA. Both should leave first-person view and
   return to the same cooperative strategy game, with the pilots' final
   positions visible on the map.

## Smoke test C: CAT & MOUSE, asymmetric 2D / 3D

1. Both ROMs select **2 PLAYERS > CAT & MOUSE > LINK CABLE**.
   SAME GBA is intentionally disabled for this simultaneous asymmetric mode.
2. Accept the world loading facts card and the mode's instructional card.
   **Round one:** Player 1 (the **CAT**) stays on the normal overhead map;
   Player 2 (the **MOUSE**) enters the real first-person ANT EYE renderer.
   It is an error if both screens are the same or if both become 3D.
3. CAT moves the map targeting cursor with the D-pad, taps **A** to tag the
   mouse within one map tile, and presses **B** for a short reveal scan
   subject to its cooldown. The map must show the same terrain, berries
   and ants that exist in the mouse's world.
4. MOUSE walks/turns with D-pad, sidesteps with L/R, sprints with A
   (stamina must drain only while moving), and lays scent with B.
   Walk over food, bring it home and deliver **three loads** to escape.
   Winning by delivery should not require hitting the red queen.
5. Let the first **90-second** round expire without delivering all food.
   Both GBAs must show the same cat-win result and per-player scores.
6. Advance together and confirm a **fresh arena** is generated so the
   second runner gets a fair berry supply. **Round two swaps roles:**
   P1 now sees ANT EYE and P2 sees the overhead map.
7. Press START on one device. Both devices must exit safely instead of
   leaving one in an orphaned 3D camera or a hanging link wait.
8. Test actual berry deliveries, capture, stamina regeneration, scanner
   cooldown, result scoring, low-RAM hardware, sound timing and cable
   removal manually as the scripted suite exercises only representative
   movement/controls and the time-out role-swap path.

**Automated evidence:** the new `tools/linked_smoke.py --scenario chase`
is exercised by CI alongside `--scenario co-op`. A two-core mGBA run
successfully tested the asymmetric views, independent input, the complete
first round, result card, second arena, P1/P2 viewpoint swap and a safe
exit, with 12 real paired screenshots; see
https://github.com/masong0508-lab/antopolis-gba/actions/runs/44861116040.
This is emulator evidence, **not** a claim of real-hardware FPS or
complete mechanics/long-session verification.

## Recovery and regression

- Unplug the cable during a 3D session. Both devices should eventually
  abort to LINK LOST; rejoin with a **fresh match** (there is no live resync).
- Test VERSUS and the older EMPIRE/CITY/ANTVERSE linked strategy modes
  separately; 3D co-op is currently Classic-only.
- Test SAME GBA alternating turns, including entering/exiting the ordinary
  ANT EYE view from the pause menu.
- Observe slowdown, audio skipping, 3D legibility and peak memory on the
  real target hardware; passing the GitHub compiler workflow alone does
  not verify these conditions.

## Technical invariants

- Link payload: 10 key bits, two rolling checksum bits, four sequence bits.
- Build/protocol token: `0xA74`; both cartridges must match.
- SIOMLT_SEND is populated **before** starting each master-clocked transfer. A received packet is rejected if its sequence number, local echo, hardware player ID, or SIO error bit is invalid.
- Only the presentation camera is local; input and world mutations are
  processed in stable **P1 then P2** order on both devices.
- Each player owns a different worker; AI must not move a human-owned worker.
- No dynamic memory allocation or additional framebuffers in the mode.

**Current validation:** automated two-instance mGBA integration **PASSED**
(2,845 emulated frames in workflow run
https://github.com/masong0508-lab/antopolis-gba/actions/runs/44859788127).
Both ROMs entered a connected CO-OP CLASSIC match (confirmed by saved
link-achievement flag 48); both subsequently entered live 3D ANT EYE
(confirmed by saved achievement 7), accepted separate P1/P2 controller
inputs, and returned to their map views. Eight genuine dual-emulator
screenshots and the assertion report are preserved as an Actions artifact.
The subsequent run also passed and contains the matching native emulator
core and ROM for repeatable replay:
https://github.com/masong0508-lab/antopolis-gba/actions/runs/44859806374.

**Not yet validated:** physical cable hardware, actual GBA FPS and sound
quality, long-session desync, unplug/replug recovery, and every alternate
GBA multiplayer game mode. The automated first-person test exercises the
entry, input and exit path, not every co-op mechanic or win condition.
