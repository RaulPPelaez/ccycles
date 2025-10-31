#include "player.h"
#include <stdlib.h>
#include <string.h>

int player_create(PlayerId id, const char *name, Vec2i position, Rgb color,
                  Player *out_player) {
  if (!name || !out_player) {
    return -1;
  }

  Player player = {0};
  player.id = id;
  player.position = position;
  player.tail_linked_list = NULL;
  strncpy(player.name, name, MAX_PLAYER_NAME_LEN - 1);
  player.name[MAX_PLAYER_NAME_LEN - 1] = '\0';
  player.color = color;

  *out_player = player;
  return 0;
}

void player_destroy(Player *player) {
  if (!player) {
    return;
  }
  TailNode *current = player->tail_linked_list;
  while (current) {
    TailNode *next = current->next;
    free(current);
    current = next;
  }
  memset(player, 0, sizeof(Player));
}
