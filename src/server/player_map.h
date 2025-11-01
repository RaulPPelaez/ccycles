#pragma once

#include "player.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file player_map.h
 * @brief Simple hash map for storing players by ID.
 *
 * Maps uint8_t keys (player IDs) to Player structures.
 * Maximum capacity: MAX_PLAYERS players (0..MAX_PLAYERS-1).
 */

/** Map key type (player ID) */
typedef uint8_t MapKey;

/**
 * @brief Internal map entry
 */
typedef struct MapEntry {
  MapKey key;
  Player player;
  bool occupied;
} MapEntry;

/**
 * @brief Player map structure
 */
typedef struct {
  MapEntry entries[MAX_PLAYERS]; /* One slot per possible key */
  uint32_t size;                 /* Number of active entries */
} PlayerMap;

/**
 * @brief Create a new player map
 * @return Pointer to map, or NULL on failure
 */
PlayerMap *map_create(void);

/**
 * @brief Insert a player in the map
 * @param map Player map
 * @param key Player ID key
 * @param player Player data to copy into map
 * @return 0 on success, -1 if key already exists or on error
 */
int map_insert(PlayerMap *map, MapKey key, const Player *player);

/**
 * @brief Find a player in the map
 * @param map Player map
 * @param key Player ID key
 * @return Pointer to player, or NULL if not found
 */
Player *map_find(PlayerMap *map, MapKey key);

/**
 * @brief Delete a player from the map
 * @param map Player map
 * @param key Player ID key
 */
void map_delete(PlayerMap *map, MapKey key);

/**
 * @brief Get all players in the map
 * @param map Player map
 * @param out_players Output array to fill with player pointers (allocated by
 * caller, size at least map_size(map))
 * @return Number of players written to out_players
 */
uint32_t map_get_all(PlayerMap *map, Player **out_players);

/**
 * @brief Get the number of players in the map
 * @param map Player map
 * @return Number of active players
 */
uint32_t map_size(const PlayerMap *map);

/**
 * @brief Destroy the map and free resources
 * @param map Player map
 */
void map_destroy(PlayerMap *map);

#ifdef __cplusplus
}
#endif
