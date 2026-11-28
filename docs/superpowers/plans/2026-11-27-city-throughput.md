# City simulation and classic colony throughput pass

Baseline 9f909eba6418d8f387abe4fcb86028b86683312d. Preserve existing campaign, map and save formats, multiplayer deterministic behavior, sprite budget and graphics.
- City pollution: reuse vertical source sums through a rolling three-column stencil; keep previous weighting, edges, rounding and policy order. Cache immutable law and martial flags for a whole block calculation. The native test compares all five 24x24 arrays and aggregate smog/crime against the previous full algorithm across 1,152 cities and verifies the policy lookup budget.
- City scan: compute the block row once per scan row, avoid unpacking levels of unzoned or tunnel cells; retain post-growth level accounting.
- Classic fights: exclude both pilot slots before starting the inner loop and reject pilot targets before coordinate comparisons. Collision ordering, RNG and fight effects remain unchanged.
- Gate delivery on native suite, devkitARM ROM build, linked mGBA and all existing visual/safety tests. Do not infer physical hardware frame rates from this work.
