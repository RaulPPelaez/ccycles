#pragma once

#include "game_logic.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file renderer.h
 * @brief SDL-based renderer for the Cycles game server (C port).
 */

/**
 * @brief Internal renderer structure
 */
typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  TTF_Font *font;
  GameConfig config;
  int window_width;
  int window_height;
  bool is_open;
} GameRenderer;
/**
 * @brief Create a new SDL renderer
 */
GameRenderer *renderer_create(const GameConfig *config);

/**
 * @brief Destroy renderer and free resources
 */
void renderer_destroy(GameRenderer *renderer);

/**
 * @brief Render current game state
 */
void renderer_render(GameRenderer *renderer, const Game *game);

/**
 * @brief Check if window is open
 */
bool renderer_is_open(const GameRenderer *renderer);

/**
 * @brief Poll for events
 * @param out_space_pressed Set to true if space bar was pressed
 * @return false if quit event received, true otherwise
 */
bool renderer_poll_events(GameRenderer *renderer, bool *out_space_pressed);

/**
 * @brief Render splash screen (waiting for players)
 */
void renderer_render_splash(GameRenderer *renderer, const Game *game);

#ifdef __cplusplus
}
#endif
