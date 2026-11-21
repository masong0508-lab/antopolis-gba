/* Shared terrain-edit invariant: food cannot occupy submerged cells.
   Pure expression macros avoid extra RAM, per-frame work, or banked calls.
   Parameters are values, not side-effect expressions. */
#ifndef ANTOPOLIS_TERRAIN_FOOD_H
#define ANTOPOLIS_TERRAIN_FOOD_H
#define TERRAIN_FOOD_AFTER(height, berries) ((height) ? (berries) : 0u)
#define TERRAIN_FOOD_DROWNED(height, berries) ((height) == 0u && (berries) != 0u)
#endif
