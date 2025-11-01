#include "renderer.h"
#include "game_logic.h"
#include "player.h"
#include "resource_loader.hpp"
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ulog.h>

// TailNode is defined in player.h
typedef struct TailNode TailNode;

/**
 * @brief Create a new SDL renderer
 */
GameRenderer *renderer_create(const GameConfig *config) {
  if (!config) {
    ulog_error("renderer_create: config is NULL");
    return NULL;
  }
  GameRenderer *r = (GameRenderer *)calloc(1, sizeof(GameRenderer));
  if (!r) {
    ulog_error("renderer_create: failed to allocate memory");
    return NULL;
  }
  r->config = *config;
  r->window_width = config->game_width;
  r->window_height = config->game_height + 100; // 100 for banner
  r->is_open = true;
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    ulog_error("SDL_Init failed: %s", SDL_GetError());
    free(r);
    return NULL;
  }
  if (TTF_Init() < 0) {
    ulog_error("TTF_Init failed: %s", TTF_GetError());
    SDL_Quit();
    free(r);
    return NULL;
  }
  r->window = SDL_CreateWindow("Cycles", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, r->window_width,
                               r->window_height, SDL_WINDOW_SHOWN);
  if (!r->window) {
    ulog_error("SDL_CreateWindow failed: %s", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    free(r);
    return NULL;
  }
  r->renderer = SDL_CreateRenderer(
      r->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!r->renderer) {
    ulog_error("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(r->window);
    TTF_Quit();
    SDL_Quit();
    free(r);
    return NULL;
  }
  r->font =
      (TTF_Font *)resource_load_font_from_memory("resources/SAIBA-45.ttf", 24);
  if (!r->font) {
    ulog_warn("Failed to load font from embedded resources: %s",
              TTF_GetError());
    // Proceeding without font
  }
  return r;
}

/**
 * @brief Destroy renderer and free resources
 */
void renderer_destroy(GameRenderer *r) {
  if (!r)
    return;
  if (r->font) {
    TTF_CloseFont(r->font);
  }
  if (r->renderer) {
    SDL_DestroyRenderer(r->renderer);
  }
  if (r->window) {
    SDL_DestroyWindow(r->window);
  }
  TTF_Quit();
  SDL_Quit();
  free(r);
}

/**
 * @brief Check if window is open
 */
bool renderer_is_open(const GameRenderer *r) { return r && r->is_open; }

/**
 * @brief Poll for events
 * @param out_space_pressed Set to true if space bar was pressed
 * @return false if quit event received, true otherwise
 */
bool renderer_poll_events(GameRenderer *r, bool *out_space_pressed) {
  if (!r)
    return false;
  if (out_space_pressed) {
    *out_space_pressed = false;
  }
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      r->is_open = false;
      return false;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        r->is_open = false;
        return false;
      }
      if (event.key.keysym.sym == SDLK_SPACE && out_space_pressed) {
        *out_space_pressed = true;
      }
      break;
    }
  }
  return true;
}

/**
 * @brief Draw a filled circle using SDL_gfx
 */
static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy,
                               int radius, uint8_t r, uint8_t g, uint8_t b) {
  filledCircleRGBA(renderer, cx, cy, radius, r, g, b, 255);
}

/**
 * @brief Draw a circle outline using SDL_gfx
 */
static void draw_circle_outline(SDL_Renderer *renderer, int cx, int cy,
                                int radius, int thickness, uint8_t r, uint8_t g,
                                uint8_t b) {
  // Draw multiple circles for thickness
  for (int t = 0; t < thickness; t++) {
    circleRGBA(renderer, cx, cy, radius + t, r, g, b, 255);
  }
}

/**
 * @brief Render text at position
 */
static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        const char *text, int x, int y, SDL_Color color,
                        SDL_Color outline_color, int outline_thickness) {
  if (!font || !text)
    return;
  SDL_Surface *surface = NULL;
  SDL_Texture *texture = NULL;
  // Render outline if thickness > 0
  if (outline_thickness > 0) {
    // Simple outline: render text in outline color at offsets
    for (int ox = -outline_thickness; ox <= outline_thickness; ox++) {
      for (int oy = -outline_thickness; oy <= outline_thickness; oy++) {
        if (ox == 0 && oy == 0)
          continue;
        surface = TTF_RenderText_Blended(font, text, outline_color);
        if (surface) {
          texture = SDL_CreateTextureFromSurface(renderer, surface);
          if (texture) {
            SDL_Rect rect = {x + ox, y + oy, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &rect);
            SDL_DestroyTexture(texture);
          }
          SDL_FreeSurface(surface);
        }
      }
    }
  }
  // Render main text
  surface = TTF_RenderText_Blended(font, text, color);
  if (surface) {
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
      SDL_Rect rect = {x, y, surface->w, surface->h};
      SDL_RenderCopy(renderer, texture, NULL, &rect);
      SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
  }
}

/**
 * @brief Render players (heads, tails, names)
 */
static void render_players(GameRenderer *r, const Game *game) {
  if (!r || !game)
    return;
  const int banner_height = 100;
  const int offset_y = banner_height;
  const int offset_x = 0;
  const int cell_size = (int)r->config.cell_size;
  // Get all active players
  Player *player_ptrs[MAX_PLAYERS];
  uint32_t player_count = game_get_players((Game *)game, player_ptrs);
  if (player_count == 0) {
    return;
  }
  for (uint32_t i = 0; i < player_count; i++) {
    const Player *player = player_ptrs[i];
    // Draw tail (iterate through linked list)
    TailNode *tail_node = player->tail_linked_list;
    while (tail_node) {
      Vec2i tail_pos = tail_node->position;
      SDL_Rect rect = {tail_pos.x * cell_size + offset_x,
                       tail_pos.y * cell_size + offset_y, cell_size, cell_size};
      SDL_SetRenderDrawColor(r->renderer, player->color.r, player->color.g,
                             player->color.b, 255);
      SDL_RenderFillRect(r->renderer, &rect);
      tail_node = tail_node->next;
    }
    // Draw head (filled circle with darker color)
    int head_x = player->position.x * cell_size + offset_x;
    int head_y = player->position.y * cell_size + offset_y;
    uint8_t darker_r = player->color.r * 0.8;
    uint8_t darker_g = player->color.g * 0.8;
    uint8_t darker_b = player->color.b * 0.8;
    draw_filled_circle(r->renderer, head_x, head_y, cell_size, darker_r,
                       darker_g, darker_b);
    // Draw head border
    draw_circle_outline(r->renderer, head_x, head_y, cell_size + 1, 3,
                        player->color.r, player->color.g, player->color.b);
    // Draw player name
    if (r->font) {
      SDL_Color white = {255, 255, 255, 255};
      SDL_Color black = {0, 0, 0, 255};
      render_text(r->renderer, r->font, player->name, head_x - 20, head_y - 20,
                  white, black, 2);
    }
  }
}

/**
 * @brief Render banner (top bar with stats)
 */
static void render_banner(GameRenderer *r, const Game *game) {
  if (!r || !game)
    return;
  const int banner_height = 80;
  // Draw black banner background
  SDL_Rect banner_rect = {0, 0, r->window_width, banner_height};
  SDL_SetRenderDrawColor(r->renderer, 0, 0, 0, 255);
  SDL_RenderFillRect(r->renderer, &banner_rect);
  if (!r->font)
    return;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color black = {0, 0, 0, 255};
  // Draw frame number
  char frame_text[64];
  snprintf(frame_text, sizeof(frame_text), "Frame: %u", game_get_frame(game));
  render_text(r->renderer, r->font, frame_text, 10, 10, white, black, 0);
  // Draw player count
  Player *player_ptrs[MAX_PLAYERS];
  uint32_t player_count = game_get_players((Game *)game, player_ptrs);
  char players_text[64];
  snprintf(players_text, sizeof(players_text), "Players: %u", player_count);
  render_text(r->renderer, r->font, players_text, 10, 40, white, black, 0);
}

/**
 * @brief Render game over screen
 */
static void render_game_over(GameRenderer *r, const Game *game) {
  if (!r || !game || !r->font)
    return;
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color black = {0, 0, 0, 255};
  // Draw "Game Over" text
  render_text(r->renderer, r->font, "Game Over", r->window_width / 2 - 100,
              r->window_height / 2 - 30, black, white, 3);
  // Draw winner if there's one player left
  Player *player_ptrs[MAX_PLAYERS];
  uint32_t player_count = game_get_players((Game *)game, player_ptrs);
  if (player_count > 0) {
    char winner_text[128];
    snprintf(winner_text, sizeof(winner_text), "Winner: %s",
             player_ptrs[0]->name);
    render_text(r->renderer, r->font, winner_text, r->window_width / 2 - 100,
                r->window_height / 2 + 30, black, white, 3);
  }
}

/**
 * @brief Render current game state
 */
void renderer_render(GameRenderer *r, const Game *game) {
  if (!r || !game)
    return;
  SDL_SetRenderDrawColor(r->renderer, 0, 0, 0, 255);
  SDL_RenderClear(r->renderer);
  render_players(r, game);
  if (game_is_over(game)) {
    render_game_over(r, game);
  }
  render_banner(r, game);
  SDL_RenderPresent(r->renderer);
}

/**
 * @brief Render splash screen (waiting for players)
 */
void renderer_render_splash(GameRenderer *r, const Game *game) {
  if (!r || !game)
    return;
  SDL_SetRenderDrawColor(r->renderer, 0, 0, 0, 255);
  SDL_RenderClear(r->renderer);
  render_players(r, game);
  render_banner(r, game);
  if (r->font) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    render_text(r->renderer, r->font, "Waiting for players",
                r->window_width / 2 - 150, r->window_height / 2 - 60, black,
                white, 2);
    render_text(r->renderer, r->font, "press SPACE to start",
                r->window_width / 2 - 150, r->window_height / 2 - 20, black,
                white, 2);
  }
  SDL_RenderPresent(r->renderer);
}
