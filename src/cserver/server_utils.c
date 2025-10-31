#include "types.h"
#include <stddef.h>

void fill_default_configuration(GameConfig *config) {
  if (config == NULL) {
    return;
  }
  config->grid_width = 100;
  config->grid_height = 100;
  config->max_clients = 60;
  config->game_width = 1000;
  config->game_height = 1000;
  config->enable_postprocessing = false;
  if (config->grid_width > 0) {
    config->cell_size = (float)config->game_width / (float)config->grid_width;
  } else {
    config->cell_size = 10.0f;
  }
}

Vec2i direction_to_vector(Direction dir) {
  Vec2i vec = {0, 0};
  switch (dir) {
  case north:
    vec.y = -1;
    break;
  case east:
    vec.x = 1;
    break;
  case south:
    vec.y = 1;
    break;
  case west:
    vec.x = -1;
    break;
  }
  return vec;
}