#pragma once

#include "player.h"
#include "player_map.h"
#include "server_utils.h"
#include "types.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file game_logic.h
 * @brief Core game logic for the Cycles game server (C port).
 */

/**
 * @brief Game structure, main state holder
 */
typedef struct {
  GameConfig config;  ///< Game configuration
  PlayerMap *players; ///< Map of players
  uint8_t *grid;      ///< Game grid
  uint32_t frame;     ///< Current frame number

  pthread_mutex_t game_mutex;
  size_t max_tail_length;
  uint64_t rng_state;
  PlayerId id_counter;
  bool game_started;
} Game;

/**
 * @brief Create a new game instance
 */
Game *game_create(const GameConfig *config);

/**
 * @brief Destroy game instance and free resources
 */
void game_destroy(Game *game);

/**
 * @brief Add a player at a random position
 * @return Player ID (0 on failure)
 */
PlayerId game_add_player(Game *game, const char *name);

/**
 * @brief Remove a player and clear their trail
 */
void game_remove_player(Game *game, PlayerId id);

/**
 * @brief Move all players, detect collisions, update grid
 * @param directions Array indexed by player ID (size MAX_PLAYERS)
 */
void game_move_players(Game *game, const Direction *directions);

/**
 * @brief Get read-only access to grid data
 * @return Grid pointer (row-major, size = width * height)
 */
const uint8_t *game_get_grid(const Game *game);

/**
 * @brief Get grid dimensions
 */
void game_get_grid_size(const Game *game, uint32_t *width, uint32_t *height);

/**
 * @brief Get read-only access to a player
 * @param id Player ID
 * @return Pointer to player structure, or NULL if not found
 */
const Player *game_get_player(Game *game, PlayerId id);

/**
 * @brief
 */
/**
 * @brief Get all active players
 * @param players Output array (allocated by caller)
 * @return Number of active players
 */
uint32_t game_get_players(Game *game, Player **players);

/**
 * @brief Check if game is over (0 or 1 players remaining)
 */
bool game_is_over(const Game *game);

/**
 * @brief Get/set current frame number
 */
uint32_t game_get_frame(const Game *game);
void game_set_frame(Game *game, uint32_t frame);

/**
 * @brief Load configuration from YAML file
 * @return 0 on success, -1 on failure
 */
int game_config_load(const char *path, GameConfig *config);

#ifdef __cplusplus
}
#endif
