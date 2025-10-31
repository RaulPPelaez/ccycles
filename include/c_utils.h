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
static inline bool is_inside_grid(const GameState *gs, Vec2i p) {
  return (p.x >= 0) && (p.y >= 0) && ((uint32_t)p.x < gs->grid_width) &&
         ((uint32_t)p.y < gs->grid_height);
}

/**
 * Get the contents of a grid cell.
 * @param gs Pointer to the game state
 * @param p Cell coordinates
 * @return Cell contents (0 = empty, >0 = player ID)
 * @note Behavior is undefined if p is out of bounds. Use is_inside_grid()
 * first.
 */
static inline uint8_t get_grid_cell(const GameState *gs, Vec2i p) {
  return gs->grid[(uint32_t)p.y * gs->grid_width + (uint32_t)p.x];
}

/**
 * Get the unit vector corresponding to a direction.
 * @param d Direction
 * @return Unit vector (x,y) where each component is in {-1,0,1}
 */
static inline Vec2i get_direction_vector(Direction d) {
  switch (d) {
  case north:
    return (Vec2i){0, -1};
  case east:
    return (Vec2i){1, 0};
  case south:
    return (Vec2i){0, 1};
  case west:
    return (Vec2i){-1, 0};
  default:
    return (Vec2i){0, 0};
  }
}

enum { NUM_DIRECTIONS = 4 }; ///< Number of valid directions

/**
 * Normalize an integer to a valid Direction value.
 * @param v Input integer (can be negative or out of range)
 * @return Corresponding Direction value
 */
static inline Direction get_direction_from_value(int v) {
  /* Normalize to [0, NUM_DIRECTIONS-1] */
  int n = v % NUM_DIRECTIONS;
  if (n < 0)
    n += NUM_DIRECTIONS;
  return (Direction)n;
}

/**
 * Check if a proposed move is valid (inside grid and target cell empty).
 * @param state Pointer to the game state
 * @param my_pos Current player position
 * @param direction Proposed move direction
 * @return true if the move is valid, false otherwise
 */
static inline bool is_valid_move(const GameState *state, Vec2i my_pos,
                                 Direction direction) {
  Vec2i d = get_direction_vector(direction);
  Vec2i new_pos = (Vec2i){my_pos.x + d.x, my_pos.y + d.y};

  if (!is_inside_grid(state, new_pos)) {
    return false;
  }
  if (get_grid_cell(state, new_pos) != 0) {
    return false;
  }
  return true;
}
