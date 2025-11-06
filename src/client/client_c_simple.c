#include "c_api.h"
#include "c_utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ulog.h>
#ifndef DEFAULT_ULOG_LEVEL
#define DEFAULT_ULOG_LEVEL ULOG_LEVEL_INFO
#endif
enum { MAX_ATTEMPTS = 200 };

/**
 * Decide next move.
 *
 * @param state              Current game state (read-only)
 * @param me                 This player (read-only)
 * @param previous_direction Last direction we moved
 * @param inertia            Bias for continuing the same direction (>= 0)
 * @param rng_state          RNG state (seeded by caller)
 *
 * @return a valid cycles_direction
 */
cycles_direction decide_move(const cycles_game_state *state,
                             const cycles_player *me,
                             cycles_direction previous_direction, float inertia,
                             uint64_t *rng_state) {
  // If we don't have a valid player (e.g., kicked/disconnected), avoid UB.
  if (me == NULL) {
    ulog_error(
        "decide_move called with NULL player; returning default direction");
    return cycles_north;
  }
  int attempts = 0;
  float inertial_damping = 1.0f;

  const cycles_vec2i position = {(int)me->x, (int)me->y};
  const uint32_t frame_number = state->frame_number;

  cycles_direction direction;

  do {
    if (attempts >= MAX_ATTEMPTS) {
      ulog_error("%s: Failed to find a valid move after %d attempts",
                 (me && me->name) ? me->name : "player", MAX_ATTEMPTS);
      return cycles_north; // Give up and return a default direction
    }
    int upper = (NUM_DIRECTIONS - 1) +
                (int)floorf(inertia * inertial_damping + 0.0001f);
    if (upper < (NUM_DIRECTIONS - 1))
      upper = (NUM_DIRECTIONS - 1);

    int proposal = rand_int_inclusive(rng_state, upper);

    if (proposal >= NUM_DIRECTIONS) {
      proposal = (int)previous_direction;
      inertial_damping = 0.0f;
    }
    direction = cycles_get_direction_from_value(proposal);
    attempts++;
  } while (!cycles_is_valid_move(state, position, direction));
  cycles_vec2i dv = cycles_get_direction_vector(direction);
  ulog_debug(
      "%s: Valid move after %d attempt%s, from (%d,%d) to (%d,%d) in frame "
      "%u\n",
      (me && me->name) ? me->name : "player", attempts,
      attempts == 1 ? "" : "s", position.x, position.y, position.x + dv.x,
      position.y + dv.y, frame_number);
  return direction;
}

uint32_t hash_color(cycles_rgb color) {
  return (uint32_t)color.r << 16 | (uint32_t)color.g << 8 | (uint32_t)color.b;
}

int main(int argc, char *argv[]) {
  // Get the port from the env variable CYCLES_PORT
  const char *PORT = getenv("CYCLES_PORT");
  if (argc < 3) {
    ulog_error("Usage: %s <host_address> <name>", argv[0]);
    return EXIT_FAILURE;
  }
  if (PORT == NULL) {
    ulog_error("Environment variable CYCLES_PORT not set.");
    return EXIT_FAILURE;
  }
  const char *HOST = argv[1];
  const char *name = argv[2];

  // Configure default logging verbosity from CMake
  ulog_output_level_set_all(DEFAULT_ULOG_LEVEL);
  ulog_debug("Ready to use Sockets");

  cycles_connection conn;
  if (cycles_connect(name, HOST, PORT, &conn) < 0) {
    ulog_error("connect() failed. (%d)", GETSOCKETERRNO());
    return EXIT_FAILURE;
  }
  // Now the client can enter the game loop
  ulog_debug("Client connected as %s with color R=%d G=%d B=%d", conn.name,
             conn.color.r, conn.color.g, conn.color.b);
  uint32_t my_hash = hash_color(conn.color);
  uint64_t rng_state = ((uint64_t)my_hash << 32) | (uint64_t)time(NULL);
  float inertia = rand_int_inclusive(&rng_state, 50);
  int32_t direction = -1;
  uint frame = 0;
  for (;;) {
    cycles_game_state gs;
    if (cycles_recv_game_state(conn.sock, &gs) < 0) {
      ulog_error("recv_game_state() failed. (%d)", GETSOCKETERRNO());
      break;
    }
    ulog_debug("Frame %d: grid %ux%u with %u players", frame, gs.grid_width,
               gs.grid_height, gs.player_count);
    cycles_player *me = NULL;
    for (uint32_t i = 0; i < gs.player_count; ++i) {
      cycles_player *p = &gs.players[i];
      if (hash_color(p->color) == my_hash) {
        me = p;
      }
      ulog_debug("Player %u: '%s' at (%d,%d) color R=%d G=%d B=%d", p->id,
                 p->name, p->x, p->y, p->color.r, p->color.g, p->color.b);
    }
    // If our player is no longer present (e.g., died and was removed),
    // exit gracefully.
    if (me == NULL) {
      ulog_info("Player '%s' is no longer in the game (kicked/disconnected). "
                "Exiting gracefully.",
                conn.name);
      cycles_free_game_state(&gs);
      break;
    }
    direction = decide_move(&gs, me, direction, inertia, &rng_state);
    if (cycles_send_move_i32(&conn, direction) < 0) {
      ulog_error("send() failed. (%d)", GETSOCKETERRNO());
      cycles_free_game_state(&gs);
      break;
    }
    ulog_debug("Sent move direction %d", direction);
    cycles_free_game_state(&gs);
    frame++;
  }
  ulog_debug("Cleaning up...");
  cycles_disconnect(&conn);
  return EXIT_SUCCESS;
}
