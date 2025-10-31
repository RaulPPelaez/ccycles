#include "game_logic.h"
#include "renderer.h"
#include "server.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @brief Thread argument for running the server
 */
typedef struct {
  GameServer *server;
} ServerThreadArg;

/**
 * @brief Server thread function
 */
static void *server_thread_func(void *arg) {
  ServerThreadArg *thread_arg = (ServerThreadArg *)arg;
  if (thread_arg && thread_arg->server) {
    server_run(thread_arg->server);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));
  const char *config_path = argc > 1 ? argv[1] : "config.yaml";
  GameConfig config;
  if (game_config_load(config_path, &config) != 0) {
    fprintf(stderr, "Failed to load configuration from %s\n", config_path);
    return 1;
  }
  Game *game = game_create(&config);
  if (!game) {
    fprintf(stderr, "Failed to create game instance\n");
    return 1;
  }
  GameServer *server = server_create(game, &config);
  if (!server) {
    fprintf(stderr, "Failed to create server\n");
    game_destroy(game);
    return 1;
  }
  if (server_listen(server, 12345) != 0) {
    fprintf(stderr, "Failed to start server on port 12345\n");
    server_destroy(server);
    game_destroy(game);
    return 1;
  }
  printf("Server listening on port 12345\n");
  GameRenderer *renderer = renderer_create(&config);
  if (!renderer) {
    fprintf(stderr, "Failed to create renderer\n");
    server_destroy(server);
    game_destroy(game);
    return 1;
  }

  // Phase 1: Start accept thread and wait for space bar
  pthread_t accept_thread;
  ServerThreadArg accept_arg = {server};
  if (pthread_create(&accept_thread, NULL,
                     (void *(*)(void *))server_accept_clients, server) != 0) {
    fprintf(stderr, "Failed to create accept thread\n");
    renderer_destroy(renderer);
    server_destroy(server);
    game_destroy(game);
    return 1;
  }

  bool accepting_clients = true;
  printf("Waiting for players... Press SPACE to start the game.\n");
  while (accepting_clients && renderer_is_open(renderer)) {
    bool space_pressed = false;
    if (!renderer_poll_events(renderer, &space_pressed)) {
      break;
    }
    if (space_pressed) {
      printf("Space pressed, stopping client acceptance\n");
      accepting_clients = false;
    }
    renderer_render_splash(renderer, game);
  }

  // Stop accepting clients and wait for accept thread to finish
  server_set_accepting_clients(server, false);
  pthread_join(accept_thread, NULL);

  // Phase 2: Start game loop thread and render game
  pthread_t server_thread;
  ServerThreadArg thread_arg = {server};
  if (pthread_create(&server_thread, NULL, server_thread_func, &thread_arg) !=
      0) {
    fprintf(stderr, "Failed to create server thread\n");
    renderer_destroy(renderer);
    server_destroy(server);
    game_destroy(game);
    return 1;
  }
  printf("Game started!\n");
  while (renderer_is_open(renderer)) {
    bool space_pressed = false;
    if (!renderer_poll_events(renderer, &space_pressed)) {
      // Quit event received
      break;
    }
    renderer_render(renderer, game);
  }

  // Cleanup
  printf("Shutting down...\n");
  server_stop(server);
  pthread_join(server_thread, NULL);
  renderer_destroy(renderer);
  server_destroy(server);
  game_destroy(game);

  printf("Server stopped\n");
  return 0;
}
