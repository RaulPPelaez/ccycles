#pragma once
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <errno.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// These definitions helps to write cross-platform code
#if !defined(_WIN32)
// In UNIX, sockets are file descriptors (int)
// In Winsock, sockets are of type SOCKET (unsigned int)
typedef int SOCKET;
// In UNIX, the value of an invalid socket is -1, while Winsock uses a value
// called INVALID_SOCKET
#define ISVALIDSOCKET(s) ((s) >= 0)
// In UNIX, the function to close a socket is called close(), while in Winsock
// it's called closesocket()
#define CLOSESOCKET(s) close(s)
// Error handling is also different between UNIX and Winsock. UNIX uses errno
#define GETSOCKETERRNO() (errno)
#else
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())
#endif

#define MAX_NAME_LEN 255

/**
 * RGB color structure
 */
typedef struct {
  uint8_t r; ///< Red component
  uint8_t g; ///< Green component
  uint8_t b; ///< Blue component
} Rgb;

/**
 * Connection with the cycles server
 */
typedef struct {
  SOCKET sock;                 ///< Socket descriptor
  Rgb color;                   ///< Assigned player color
  char name[MAX_NAME_LEN + 1]; ///< Player name (NUL-terminated)
} Connection;

/**
 * Player state as received from the server
 */
typedef struct {
  char *name;  ///< Player name (heap, NUL-terminated)
  Rgb color;   ///< Player color
  int32_t x;   ///< Player head X position
  int32_t y;   ///< Player head Y position
  uint32_t id; ///< Player unique ID
} Player;

/**
 * Game state as received from the server. All heap allocations must be freed
 * using free_game_state().
 */
typedef struct {
  uint32_t grid_width;   ///< Grid width
  uint32_t grid_height;  ///< Grid height
  uint32_t player_count; ///< Number of players
  Player *players;       ///< List of players (size = player_count)
  /**
   * Grid cells, row-major order, size = grid_width * grid_height.
   * Each cell contains either 0 (empty) or the ID of the player occupying it.
   *
   * The value of grid[y * grid_width + x] corresponds to the cell at (x,y).
   */
  uint8_t *grid;
  uint32_t frame_number; ///< Current game time (in frames from start)
} GameState;

/**
 * Directions for player movement
 */
typedef enum {
  north = 0, ///< North
  east = 1,  ///< East
  south = 2, ///< South
  west = 3   ///< West
} Direction;

/**
 * 2D integer vector
 */
typedef struct {
  int x; ///< X component
  int y; ///< Y component
} Vec2i;

/**
 * Connect to the cycles server, send the player name, and receive the assigned
 * color.
 * @param name Player name (NUL-terminated)
 * @param host Server hostname or IP address (NUL-terminated)
 * @param port Server port as a string (NUL-terminated)
 * @param conn Pointer to an empty Connection structure to fill in
 * @return 0 on success, -1 on failure (check errno)
 */
int cycles_connect(const char *name, const char *host, const char *port,
                   Connection *conn);

/**
 * Disconnect from the server and clean up the Connection structure.
 * @param conn Pointer to a Connection structure previously initialized by
 * cycles_connect()
 */
void cycles_disconnect(Connection *conn);

/**
 * Free the contents of a GameState structure, including heap allocations.
 * @param gs Pointer to a GameState variable to free
 */
void free_game_state(GameState *gs);

/**
 * Receive a game state update from the server.
 * @param sock Connected socket
 * @param out Pointer to an empty GameState structure to fill in
 * @return 0 on success, -1 on failure (check errno)
 */
int cycles_recv_game_state(SOCKET sock, GameState *out);

/**
 * Send a move command (direction) to the server.
 * @param conn Pointer to an initialized Connection structure
 * @param dir Direction as an int32_t (0=north, 1=east, 2=south, 3=west)
 * @return 0 on success, -1 on failure (check errno)
 */
int cycles_send_move_i32(Connection *conn, int32_t dir);

#ifdef __cplusplus
}
#endif
