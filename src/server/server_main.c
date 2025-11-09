#include "game_logic.h"
#include "renderer.h"
#include "server.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ulog.h>

#ifdef _WIN32
#pragma comment(lib, "pthread.lib")
#endif

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
  // Configure default logging verbosity from CMake
  ulog_output_level_set_all(DEFAULT_ULOG_LEVEL);
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
  int port = -1;
  const char *env_port = getenv("CYCLES_PORT");
  if (env_port && *env_port) {
    char *endptr = NULL;
    errno = 0;
    long val = strtol(env_port, &endptr, 10);
    if (errno == 0 && endptr && *endptr == '\0' && val > 0 && val <= 65535) {
      port = (int)val;
    } else {
      fprintf(stderr, "Warning: invalid CYCLES_PORT='%s'.\n", env_port);
      exit(1);
    }
  }

  if (server_listen(server, (uint16_t)port) != 0) {
    fprintf(stderr, "Failed to start server on port %d\n", port);
    server_destroy(server);
    game_destroy(game);
    return 1;
  }
  ulog_info("Server listening on port %d", port);
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
  ulog_info("Waiting for players... Press SPACE to start the game.");
  while (accepting_clients && renderer_is_open(renderer)) {
    bool space_pressed = false;
    if (!renderer_poll_events(renderer, &space_pressed)) {
      // Quit event received
      renderer_destroy(renderer);
      server_destroy(server);
      game_destroy(game);
      exit(0);
    }
    if (space_pressed) {
      ulog_info("Space pressed, stopping client acceptance");
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
  ulog_info("Game started!");
  while (renderer_is_open(renderer)) {
    bool space_pressed = false;
    if (!renderer_poll_events(renderer, &space_pressed)) {
      // Quit event received
      break;
    }
    renderer_render(renderer, game);
  }

  // Cleanup
  ulog_info("Shutting down...");
  server_stop(server);
  pthread_join(server_thread, NULL);
  renderer_destroy(renderer);
  server_destroy(server);
  game_destroy(game);
  ulog_info("Server stopped");
  return 0;
}
