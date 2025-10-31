#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file player.h
 * @brief Player management functions.
 */

typedef struct TailNode {
  Vec2i position;
  struct TailNode *next;
} TailNode;

/*
 * @brief Definition of a Player
 */
typedef struct {
  PlayerId id;
  char name[MAX_PLAYER_NAME_LEN];
  Vec2i position;
  TailNode *tail_linked_list;
  Rgb color;
} Player;

/**
 * @brief Create a new player
 * @param id Player ID
 * @param name Player name (must not be NULL)
 * @param position Initial position
 * @param color Player color
 * @param out_player Output parameter for initialized player
 * @return 0 on success, -1 on failure (e.g., NULL name)
 */
int player_create(PlayerId id, const char *name, Vec2i position, Rgb color,
                  Player *out_player);

/**
 * @brief Destroy a player and free its resources
 *
 * Frees the tail linked list and resets the player structure.
 *
 * @param player Player to destroy
 */
void player_destroy(Player *player);

#ifdef __cplusplus
}
#endif