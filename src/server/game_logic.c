#include "game_logic.h"
#include "player.h"
#include "player_map.h"
#include "server_utils.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#ifdef _WIN32
#pragma comment(lib, "pthread.lib")
#endif

struct Game {
  GameConfig config;
  PlayerMap *players;
  uint8_t *grid;
  uint32_t frame;
  pthread_mutex_t game_mutex;
  size_t max_tail_length;
  uint64_t rng_state;
  PlayerId id_counter;
  bool game_started;
};

/* Simple xorshift64 RNG */
static uint64_t xorshift64(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

static float rand_float(Game *game) {
  return (float)xorshift64(&game->rng_state) / (float)UINT64_MAX;
}

/* HSL to RGB conversion */
static void hsl_to_rgb(float h, float s, float l, uint8_t *r, uint8_t *g,
                       uint8_t *b) {
  float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = l - c / 2.0f;

  float rf = 0, gf = 0, bf = 0;
  if (h >= 0 && h < 60) {
    rf = c;
    gf = x;
    bf = 0;
  } else if (h >= 60 && h < 120) {
    rf = x;
    gf = c;
    bf = 0;
  } else if (h >= 120 && h < 180) {
    rf = 0;
    gf = c;
    bf = x;
  } else if (h >= 180 && h < 240) {
    rf = 0;
    gf = x;
    bf = c;
  } else if (h >= 240 && h < 300) {
    rf = x;
    gf = 0;
    bf = c;
  } else {
    rf = c;
    gf = 0;
    bf = x;
  }

  *r = (uint8_t)((rf + m) * 255.0f);
  *g = (uint8_t)((gf + m) * 255.0f);
  *b = (uint8_t)((bf + m) * 255.0f);
}

/* Generate color palette using golden ratio */
static void generate_color_palette(Rgb *palette, int num_colors) {
  float golden_ratio = 0.618033988749895f;
  float hue = 0;
  for (int i = 0; i < num_colors; i++) {
    hue = fmodf(hue + golden_ratio, 1.0f);
    float saturation = 0.5f + (sinf(hue * 2.0f * M_PI) * 0.1f);
    float lightness = 0.6f + (cosf(hue * 2.0f * M_PI) * 0.1f);
    hsl_to_rgb(hue * 360.0f, saturation, lightness, &palette[i].r,
               &palette[i].g, &palette[i].b);
  }
}

static uint8_t *get_cell(Game *game, int x, int y) {
  return &game->grid[y * game->config.grid_width + x];
}

static bool is_legal_move(Game *game, Vec2i new_pos) {
  if (new_pos.x < 0 || new_pos.x >= (int)game->config.grid_width ||
      new_pos.y < 0 || new_pos.y >= (int)game->config.grid_height) {
    return false;
  }
  if (*get_cell(game, new_pos.x, new_pos.y) != 0) {
    return false;
  }
  return true;
}

Game *game_create(const GameConfig *config) {
  if (!config) {
    return NULL;
  }
  Game *game = calloc(1, sizeof(Game));
  if (!game) {
    return NULL;
  }
  game->config = *config;
  game->players = map_create();
  if (!game->players) {
    free(game);
    return NULL;
  }
  game->grid =
      calloc(config->grid_width * config->grid_height, sizeof(uint8_t));
  if (!game->grid) {
    map_destroy(game->players);
    free(game);
    return NULL;
  }
  pthread_mutex_init(&game->game_mutex, NULL);
  game->frame = 0;
  game->max_tail_length = 55;
  game->rng_state = 123456789ULL; /* Fixed seed for now */
  game->id_counter = 1;
  game->game_started = false;
  return game;
}

void game_destroy(Game *game) {
  if (!game) {
    return;
  }
  if (game->players) {
    map_destroy(game->players);
  }
  free(game->grid);
  pthread_mutex_destroy(&game->game_mutex);
  free(game);
}

PlayerId game_add_player(Game *game, const char *name) {
  if (!game || !name) {
    return 0;
  }
  static Rgb palette[MAX_PLAYERS];
  static bool palette_initialized = false;
  if (!palette_initialized) {
    generate_color_palette(palette, MAX_PLAYERS);
    palette_initialized = true;
  }
  pthread_mutex_lock(&game->game_mutex);
  game->game_started = true;
  Vec2i position;
  int attempts = 0;
  do {
    position.x = (int)(rand_float(game) * game->config.grid_width);
    position.y = (int)(rand_float(game) * game->config.grid_height);
    attempts++;
    if (attempts > 10000) {
      pthread_mutex_unlock(&game->game_mutex);
      return 0; /* Grid is full */
    }
  } while (*get_cell(game, position.x, position.y) != 0);
  Player player;
  Rgb color = palette[game->id_counter % MAX_PLAYERS];
  if (player_create(game->id_counter, name, position, color, &player) != 0) {
    pthread_mutex_unlock(&game->game_mutex);
    return 0;
  }
  *get_cell(game, position.x, position.y) = game->id_counter;
  if (map_insert(game->players, game->id_counter, &player) != 0) {
    pthread_mutex_unlock(&game->game_mutex);
    return 0;
  }
  PlayerId id = game->id_counter;
  game->id_counter++;
  pthread_mutex_unlock(&game->game_mutex);
  return id;
}

void game_remove_player(Game *game, PlayerId id) {
  if (!game) {
    return;
  }
  pthread_mutex_lock(&game->game_mutex);
  Player *player = map_find(game->players, id);
  if (!player) {
    pthread_mutex_unlock(&game->game_mutex);
    return;
  }
  *get_cell(game, player->position.x, player->position.y) = 0;
  TailNode *current = player->tail_linked_list;
  while (current) {
    *get_cell(game, current->position.x, current->position.y) = 0;
    current = current->next;
  }
  map_delete(game->players, id);
  pthread_mutex_unlock(&game->game_mutex);
}

void game_move_players(Game *game, const Direction *directions) {
  if (!game || !directions) {
    return;
  }
  game->max_tail_length = 55 + game->frame / 100;
  Player *player_ptrs[MAX_PLAYERS];
  uint32_t player_count = map_get_all(game->players, player_ptrs);
  if (player_count == 0) {
    return;
  }
  Vec2i new_positions[MAX_PLAYERS] = {0};
  bool has_direction[MAX_PLAYERS] = {false};
  for (uint32_t i = 0; i < player_count; i++) {
    Player *player = player_ptrs[i];
    PlayerId id = player->id;
    Direction dir = directions[id];
    Vec2i dir_vec = direction_to_vector(dir);
    new_positions[id].x = player->position.x + dir_vec.x;
    new_positions[id].y = player->position.y + dir_vec.y;
    has_direction[id] = true;
  }
  bool colliding[MAX_PLAYERS] = {false};
  for (uint32_t i = 0; i < player_count; i++) {
    PlayerId id1 = player_ptrs[i]->id;
    if (!has_direction[id1])
      continue;
    for (uint32_t j = i + 1; j < player_count; j++) {
      PlayerId id2 = player_ptrs[j]->id;
      if (!has_direction[id2])
        continue;
      if (new_positions[id1].x == new_positions[id2].x &&
          new_positions[id1].y == new_positions[id2].y) {
        colliding[id1] = true;
        colliding[id2] = true;
      }
    }
  }
  for (uint32_t i = 0; i < player_count; i++) {
    PlayerId id = player_ptrs[i]->id;
    if (!has_direction[id])
      continue;
    if (!is_legal_move(game, new_positions[id])) {
      colliding[id] = true;
    }
  }
  for (uint32_t i = 0; i < player_count; i++) {
    PlayerId id = player_ptrs[i]->id;
    if (colliding[id]) {
      game_remove_player(game, id);
    }
  }
  for (uint32_t i = 0; i < player_count; i++) {
    PlayerId id = player_ptrs[i]->id;
    if (!has_direction[id] || colliding[id])
      continue;
    Player *player = map_find(game->players, id);
    if (!player)
      continue;
    Vec2i new_pos = new_positions[id];
    *get_cell(game, new_pos.x, new_pos.y) = id;
    TailNode *new_tail = malloc(sizeof(TailNode));
    if (new_tail) {
      new_tail->position = player->position;
      new_tail->next = player->tail_linked_list;
      player->tail_linked_list = new_tail;
    }
    uint32_t tail_len = 0;
    TailNode *current = player->tail_linked_list;
    TailNode *prev = NULL;
    while (current) {
      tail_len++;
      if (tail_len > game->max_tail_length) {
        *get_cell(game, current->position.x, current->position.y) = 0;
        TailNode *to_free = current;
        current = current->next;
        free(to_free);
        if (prev) {
          prev->next = NULL;
        }
      } else {
        prev = current;
        current = current->next;
      }
    }
    player->position = new_pos;
  }
}

const uint8_t *game_get_grid(const Game *game) {
  return game ? game->grid : NULL;
}

void game_get_grid_size(const Game *game, uint32_t *width, uint32_t *height) {
  if (!game || !width || !height) {
    return;
  }
  *width = game->config.grid_width;
  *height = game->config.grid_height;
}

uint32_t game_get_players(Game *game, Player **players) {
  if (!game || !players) {
    return 0;
  }
  pthread_mutex_lock(&game->game_mutex);
  uint32_t player_count = map_get_all(game->players, players);
  pthread_mutex_unlock(&game->game_mutex);
  return player_count;
}

const Player *game_get_player(Game *game, PlayerId id) {
  if (!game) {
    return NULL;
  }
  pthread_mutex_lock((pthread_mutex_t *)&game->game_mutex);
  Player *p = map_find(game->players, id);
  pthread_mutex_unlock((pthread_mutex_t *)&game->game_mutex);
  return p;
}

bool game_is_over(const Game *game) {
  if (!game) {
    return false;
  }
  return game->game_started && map_size(game->players) <= 1;
}

uint32_t game_get_frame(const Game *game) { return game ? game->frame : 0; }

void game_set_frame(Game *game, uint32_t frame) {
  if (game) {
    game->frame = frame;
  }
}

int game_config_load(const char *path, GameConfig *config) {
  if (!path || !config) {
    return -1;
  }
  FILE *file = fopen(path, "r");
  if (!file) {
    return -1;
  }
  yaml_parser_t parser;
  yaml_event_t event;
  if (!yaml_parser_initialize(&parser)) {
    fclose(file);
    return -1;
  }
  yaml_parser_set_input_file(&parser, file);
  fill_default_configuration(config);
  char *current_key = NULL;
  int in_mapping = 0;
  do {
    if (!yaml_parser_parse(&parser, &event)) {
      free(current_key);
      yaml_parser_delete(&parser);
      fclose(file);
      return -1;
    }
    switch (event.type) {
    case YAML_MAPPING_START_EVENT:
      in_mapping = 1;
      break;
    case YAML_SCALAR_EVENT: {
      const char *value = (const char *)event.data.scalar.value;
      if (in_mapping && !current_key) {
        current_key = strdup(value);
      } else if (in_mapping && current_key) {
        if (strcmp(current_key, "gridWidth") == 0) {
          config->grid_width = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(current_key, "gridHeight") == 0) {
          config->grid_height = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(current_key, "maxClients") == 0) {
          config->max_clients = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(current_key, "gameWidth") == 0) {
          config->game_width = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(current_key, "gameHeight") == 0) {
          config->game_height = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(current_key, "enablePostProcessing") == 0) {
          if (strcmp(value, "true") == 0 || strcmp(value, "True") == 0 ||
              strcmp(value, "1") == 0) {
            config->enable_postprocessing = true;
          } else {
            config->enable_postprocessing = false;
          }
        }
        free(current_key);
        current_key = NULL;
      }
      break;
    }
    case YAML_MAPPING_END_EVENT:
      in_mapping = 0;
      break;
    default:
      break;
    }
    if (event.type != YAML_STREAM_END_EVENT) {
      yaml_event_delete(&event);
    }
  } while (event.type != YAML_STREAM_END_EVENT);
  yaml_event_delete(&event);
  free(current_key);
  yaml_parser_delete(&parser);
  fclose(file);
  if (config->grid_width > 0) {
    config->cell_size = (float)config->game_width / (float)config->grid_width;
  }
  return 0;
}
