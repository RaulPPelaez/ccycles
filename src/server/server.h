#pragma once

#include "game_logic.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file server.h
 * @brief Network server for the Cycles game (C port).
 */

/**
 * @brief Server state
 */
typedef struct GameServer {
  Game *game;              ///< Game logic instance (owned externally)
  GameConfig conf;         ///< Server/game configuration snapshot
  int listen_socket;       ///< Listening TCP socket
  int client_sockets[256]; ///< Per-player sockets indexed by PlayerId
  bool running;            ///< Main loop flag
  bool accepting;          ///< Whether to accept new clients
  uint32_t frame;          ///< Current frame number
  int max_comm_ms;         ///< Max per-frame comm time budget in ms
} GameServer;

/**
 * @brief Create a new server instance
 */
GameServer *server_create(Game *game, const GameConfig *config);

/**
 * @brief Destroy server and close all connections
 */
void server_destroy(GameServer *server);

/**
 * @brief Start listening on specified port
 * @return 0 on success, -1 on failure
 */
int server_listen(GameServer *server, uint16_t port);

/**
 * @brief Run main server loop (blocking)
 *
 * Handles: accepting clients, receiving input, updating game, broadcasting
 * state
 */
void server_run(GameServer *server);

/**
 * @brief Stop the server (causes server_run to exit)
 */
void server_stop(GameServer *server);

/**
 * @brief Enable/disable accepting new clients
 */
void server_set_accepting_clients(GameServer *server, bool accepting);

/**
 * @brief Accept any pending client connections
 *
 * Call this in a loop during the splash screen phase to allow clients to
 * connect before the game starts. Non-blocking.
 */
void server_accept_clients(GameServer *server);

/**
 * @brief Get current frame number
 */
uint32_t server_get_frame(const GameServer *server);

#ifdef __cplusplus
}
#endif
