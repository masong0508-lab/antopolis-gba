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
- Build/protocol token: `0xA72`; both cartridges must match.
- Only the presentation camera is local; input and world mutations are
  processed in stable **P1 then P2** order on both devices.
- Each player owns a different worker; AI must not move a human-owned worker.
- No dynamic memory allocation or additional framebuffers in the mode.

**Current validation:** ROM compilation and artifact upload via GitHub
Actions. Physical link play, dual-emulator play, timing and disconnect
behavior require independent execution to qualify as tested.
