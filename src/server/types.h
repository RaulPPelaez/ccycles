#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PLAYER_NAME_LEN 32

/**
 * @file types.h
 * @brief Common types shared across C server modules.
 */

/** Type for player unique identifiers */
typedef uint8_t PlayerId;

/** RGB color structure */
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} Rgb;

/** 2D integer vector */
typedef struct {
  int x;
  int y;
} Vec2i;

/** Directions for player movement */
typedef enum { north = 0, east = 1, south = 2, west = 3 } Direction;

/**
 * @brief Game configuration
 */
typedef struct {
  uint32_t grid_width;
  uint32_t grid_height;
  uint32_t max_clients;
  uint32_t game_width;
  uint32_t game_height;
  float cell_size;
  bool enable_postprocessing;
} GameConfig;

#ifdef __cplusplus
}
#endif
