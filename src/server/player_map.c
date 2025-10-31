#include "player_map.h"
#include "player.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>

PlayerMap *map_create(void) {
  PlayerMap *map = calloc(1, sizeof(PlayerMap));
  if (!map) {
    return NULL;
  }
  map->size = 0;
  return map;
}

int map_insert(PlayerMap *map, MapKey key, const Player *player) {
  if (!map || !player) {
    return -1;
  }
  MapEntry *entry = &map->entries[key];
  if (entry->occupied) {
    return -1;
  }
  map->size++;
  entry->occupied = true;
  entry->key = key;
  entry->player = *player;
  return 0;
}

Player *map_find(PlayerMap *map, MapKey key) {
  if (!map) {
    return NULL;
  }

  MapEntry *entry = &map->entries[key];
  if (entry->occupied) {
    return &entry->player;
  }

  return NULL;
}

void map_delete(PlayerMap *map, MapKey key) {
  if (!map) {
    return;
  }
  MapEntry *entry = &map->entries[key];
  if (entry->occupied) {
    player_destroy(&entry->player);
    entry->occupied = false;
    map->size--;
    memset(entry, 0, sizeof(MapEntry));
  }
}

uint32_t map_size(const PlayerMap *map) { return map ? map->size : 0; }

uint32_t map_get_all(PlayerMap *map, Player **out_players) {
  if (!map || !out_players) {
    return 0;
  }
  uint32_t count = 0;
  for (int i = 0; i < 256; i++) {
    if (map->entries[i].occupied) {
      out_players[count++] = &map->entries[i].player;
    }
  }
  return count;
}

static void map_clear(PlayerMap *map) {
  if (!map) {
    return;
  }
  for (int i = 0; i < 256; i++) {
    if (map->entries[i].occupied) {
      map_delete(map, i);
    }
  }
}

void map_destroy(PlayerMap *map) {
  if (!map) {
    return;
  }
  map_clear(map);
  free(map);
}
