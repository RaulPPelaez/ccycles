#pragma once
#include "c_api.h"
#include <stdbool.h>
#include <stdint.h>

uint32_t pcg32(uint64_t *state) {
  // PCG-XSH-RR (32-bit)
  uint64_t oldstate = *state;
  *state = oldstate * 6364136223846793005ULL + 1442695040888963407ULL;
  uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
  uint32_t rot = (uint32_t)(oldstate >> 59u);
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/**
 * Return a random integer in [0, inclusive_max], or 0 if inclusive_max <= 0.
 * Uses bounded rejection to avoid modulo bias.
 * @param state Pointer to the RNG state (updated by this function)
 * @param inclusive_max Upper bound (inclusive)
 * @return Random integer in [0, inclusive_max], or 0 if inclusive_max <= 0
 */
static int rand_int_inclusive(uint64_t *state, int inclusive_max) {
  if (inclusive_max <= 0)
    return 0;
  // Bounded rejection to avoid modulo bias.
  uint32_t bound = (uint32_t)inclusive_max + 1u;
  uint32_t threshold = (uint32_t)(-bound) % bound;
  for (;;) {
    uint32_t r = pcg32(state);
    if (r >= threshold)
      return (int)(r % bound);
  }
}

/**
 * Check if a position is inside the grid boundaries.
 * @param gs Pointer to the game state
 * @param p Cell coordinates
 * @return true if inside, false if outside
 */
static inline bool cycles_is_inside_grid(const cycles_game_state *gs,
                                         cycles_vec2i p) {
  return (p.x >= 0) && (p.y >= 0) && ((uint32_t)p.x < gs->grid_width) &&
         ((uint32_t)p.y < gs->grid_height);
}

/**
 * Get the contents of a grid cell.
 * @param gs Pointer to the game state
 * @param p Cell coordinates
 * @return Cell contents (0 = empty, >0 = player ID)
 * @note Behavior is undefined if p is out of bounds. Use
 * cycles_is_inside_grid() first.
 */
static inline uint8_t cycles_get_grid_cell(const cycles_game_state *gs,
                                           cycles_vec2i p) {
  return gs->grid[(uint32_t)p.y * gs->grid_width + (uint32_t)p.x];
}

/**
 * Get the unit vector corresponding to a direction.
 * @param d Direction
 * @return Unit vector (x,y) where each component is in {-1,0,1}
 */
static inline cycles_vec2i cycles_get_direction_vector(cycles_direction d) {
  switch (d) {
  case cycles_north:
    return (cycles_vec2i){0, -1};
  case cycles_east:
    return (cycles_vec2i){1, 0};
  case cycles_south:
    return (cycles_vec2i){0, 1};
  case cycles_west:
    return (cycles_vec2i){-1, 0};
  default:
    return (cycles_vec2i){0, 0};
  }
}

enum { NUM_DIRECTIONS = 4 }; ///< Number of valid directions

/**
 * Normalize an integer to a valid cycles_direction value.
 * @param v Input integer (can be negative or out of range)
 * @return Corresponding cycles_direction value
 */
static inline cycles_direction cycles_get_direction_from_value(int v) {
  /* Normalize to [0, NUM_DIRECTIONS-1] */
  int n = v % NUM_DIRECTIONS;
  if (n < 0)
    n += NUM_DIRECTIONS;
  return (cycles_direction)n;
}

/**
 * Check if a proposed move is valid (inside grid and target cell empty).
 * @param state Pointer to the game state
 * @param my_pos Current player position
 * @param direction Proposed move direction
 * @return true if the move is valid, false otherwise
 */
static inline bool cycles_is_valid_move(const cycles_game_state *state,
                                        cycles_vec2i my_pos,
                                        cycles_direction direction) {
  cycles_vec2i d = cycles_get_direction_vector(direction);
  cycles_vec2i new_pos = (cycles_vec2i){my_pos.x + d.x, my_pos.y + d.y};

  if (!cycles_is_inside_grid(state, new_pos)) {
    return false;
  }
  if (cycles_get_grid_cell(state, new_pos) != 0) {
    return false;
  }
  return true;
}
