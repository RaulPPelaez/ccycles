#include "c_api.h"
#include <ulog.h>

#define HOST ("127.0.0.1")


int32_t choose_direction(const GameState *gs) {
	// This is a simple example that always returns the same direction (east).
	// You can implement your own logic here based on the game state.
	return 1; // east
}

int main(int argc, char *argv[]) {
  // Get the port from the env variable CYCLES_PORT
  const char *PORT = getenv("CYCLES_PORT");
  if (PORT == NULL) {
    ulog_error("[ERROR] Environment variable CYCLES_PORT not set.");
    return EXIT_FAILURE;
  }

#if defined(_WIN32)
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    ulog_error("WSAStartup failed: %d", iResult);
    return 1;
  }
#endif
  ulog_debug("Ready to use Sockets");

  const char *name = "CClient";
  Connection conn;
  if (cycles_connect(name, HOST, PORT, &conn) < 0) {
    return EXIT_FAILURE;
  }
  // Now the client can enter the game loop

  ulog_debug("Client connected as %s with color R=%d G=%d B=%d", conn.name,
             conn.color.r, conn.color.g, conn.color.b);

  uint frame = 0;
  for (;;) {
    GameState gs;
    if (recv_game_state(conn.sock, &gs) < 0) {
      ulog_error("recv_game_state() failed. (%d)", GETSOCKETERRNO());
      break;
    }
    ulog_debug("Frame %d: grid %ux%u with %u players", frame, gs.grid_width,
               gs.grid_height, gs.player_count);
    for (uint32_t i = 0; i < gs.player_count; ++i) {
      Player *p = &gs.players[i];
      ulog_debug("Player %u: '%s' at (%d,%d) color R=%d G=%d B=%d", p->id,
                 p->name, p->x, p->y, p->color.r, p->color.g, p->color.b);
    }
    int32_t direction = choose_direction(&gs);
    if (cycles_send_move_i32(&conn, direction) < 0) {
      ulog_error("send() failed. (%d)", GETSOCKETERRNO());
      free_game_state(&gs);
      break;
    }
    ulog_debug("Sent move direction %d", direction);
    free_game_state(&gs);
    frame++;
  }
  ulog_debug("Cleaning up...");
  cycles_disconnect(&conn);
#ifdef _WIN32
  WSACleanup();
#endif
  return EXIT_SUCCESS;
}
