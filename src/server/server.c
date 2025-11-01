#include "server.h"
#include "player.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ulog.h>
#include <unistd.h>

// Length-prefixed framing constants
enum { NET_MAX_PACKET = 32 * 1024 * 1024 };
enum { NET_MAX_STRING = 16 * 1024 * 1024 };

static int set_nonblocking(int sock) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0)
    return -1;
  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    return -1;
  return 0;
}

static int send_all(int sock, const void *buf, size_t len) {
  const unsigned char *p = (const unsigned char *)buf;
  while (len) {
    ssize_t n = send(sock, p, len, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      return -1;
    p += (size_t)n;
    len -= (size_t)n;
  }
  return 0;
}

static int recv_all(int sock, void *buf, size_t len) {
  unsigned char *p = (unsigned char *)buf;
  while (len) {
    ssize_t n = recv(sock, p, len, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      return -1; // peer closed
    p += (size_t)n;
    len -= (size_t)n;
  }
  return 0;
}

static int recv_packet_len(int sock, uint32_t *out_len) {
  uint32_t be = 0;
  if (recv_all(sock, &be, 4) < 0)
    return -1;
  *out_len = ntohl(be);
  return 0;
}

static int recv_string_packet(int sock, char **out) {
  if (!out)
    return -1;
  uint32_t outer = 0;
  if (recv_packet_len(sock, &outer) < 0)
    return -1;
  if (outer < 4 || outer > NET_MAX_PACKET)
    return -1;
  uint32_t name_len_be = 0;
  if (recv_all(sock, &name_len_be, 4) < 0)
    return -1;
  uint32_t name_len = ntohl(name_len_be);
  if (name_len > NET_MAX_STRING)
    return -1;
  char *buf = (char *)malloc((size_t)name_len + 1);
  if (!buf)
    return -1;
  if (recv_all(sock, buf, name_len) < 0) {
    free(buf);
    return -1;
  }
  buf[name_len] = '\0';
  *out = buf;
  return 0;
}

static int send_color_packet(int sock, Rgb color) {
  uint32_t payload_len_be = htonl(3u); // Send 3 bytes
  unsigned char buf[7];
  memcpy(buf + 0, &payload_len_be, 4);
  buf[4] = color.r;
  buf[5] = color.g;
  buf[6] = color.b;
  return send_all(sock, buf, sizeof buf);
}

#define member_size(type, member) (sizeof(((type *)0)->member))

static uint32_t compute_game_server_size(const GameServer *s,
                                         uint32_t player_count,
                                         Player **players) {
  uint32_t w = 0, h = 0;
  game_get_grid_size(s->game, &w, &h);
  // Estimate size
  uint32_t packet_size = 0;
  packet_size += member_size(GameConfig, grid_width);
  packet_size += member_size(GameConfig, grid_height);
  packet_size += sizeof(uint32_t); // player count
  for (uint32_t i = 0; i < player_count; ++i) {
    size_t name_len = strlen(players[i]->name);
    packet_size += 4 * 2;            // x, y
    packet_size += sizeof(Rgb);      // r,g,b
    packet_size += 4 + name_len;     // string length + bytes
    packet_size += sizeof(PlayerId); // id
  }
  packet_size += 4; // frame

  packet_size += (size_t)w * (size_t)h * sizeof(PlayerId); // grid bytes
  return packet_size;
}

/**
 * @brief Send the current game state to a client
 *
 * This function constructs and sends a packet containing the current game state
 * in the following format:
 * - Payload length (4 bytes, big-endian) - total length of the rest of the
 * packet
 * - Grid width (4 bytes)
 * - Grid height (4 bytes)
 * - Player count (4 bytes)
 * - For each player:
 *   - Position (2 * 4 bytes)
 *   - Color (3 bytes)
 *   - Name length (4 bytes) + Name (variable length)
 *   - Player ID (4 bytes)
 * - Frame (4 bytes)
 * - Grid (width * height * 1 byte)
 *
 * @param s Game server instance
 * @param sock Client socket
 */
static int send_game_state_packet(GameServer *s, int sock) {
  Player *player_ptrs[MAX_PLAYERS];
  uint32_t player_count = game_get_players(s->game, player_ptrs);
  uint32_t packet_size = compute_game_server_size(s, player_count, player_ptrs);
  ulog_debug("send_game_state_packet: computed packet size %u bytes",
             packet_size);
  uint32_t payload_len_be = htonl(packet_size);
  // Send payload length
  if (send_all(sock, &payload_len_be, 4) < 0)
    return -1;
  // Send grid w,h, player count
  uint32_t w = 0, h = 0;
  game_get_grid_size(s->game, &w, &h);
  uint32_t be;
  be = htonl(w);
  if (send_all(sock, &be, 4) < 0)
    return -1;
  be = htonl(h);
  if (send_all(sock, &be, 4) < 0)
    return -1;
  be = htonl(player_count);
  if (send_all(sock, &be, 4) < 0)
    return -1;

  // Players
  for (uint32_t i = 0; i < player_count; ++i) {
    const Player *p = player_ptrs[i];
    uint32_t x_be = htonl((uint32_t)p->position.x);
    uint32_t y_be = htonl((uint32_t)p->position.y);
    uint32_t name_len = (uint32_t)strlen(p->name);
    uint32_t name_len_be = htonl(name_len);
    PlayerId idb = p->id;
    int failed = 0;
    failed |= send_all(sock, &x_be, 4) < 0;
    failed |= send_all(sock, &y_be, 4) < 0;
    failed |= send_all(sock, &p->color, sizeof(Rgb)) < 0;
    failed |= send_all(sock, &name_len_be, 4) < 0;
    failed |= send_all(sock, p->name, name_len) < 0;
    failed |= send_all(sock, &idb, sizeof(PlayerId)) < 0;
    if (failed) {
      return -1;
    }
  }
  uint32_t frame_be = htonl(server_get_frame(s));
  if (send_all(sock, &frame_be, 4) < 0) {
    return -1;
  }
  // Grid
  const uint8_t *grid = game_get_grid(s->game);
  size_t grid_sz = (size_t)w * (size_t)h * sizeof(PlayerId);
  if (grid_sz && send_all(sock, grid, grid_sz) < 0)
    return -1;

  return 0;
}

static int recv_move_direction(int sock, int32_t *out_dir) {
  if (!out_dir)
    return -1;
  uint32_t len = 0;
  if (recv_packet_len(sock, &len) < 0)
    return -1;
  if (len != 4)
    return -1;
  uint32_t v_be = 0;
  if (recv_all(sock, &v_be, 4) < 0)
    return -1;
  *out_dir = (int32_t)ntohl(v_be);
  return 0;
}

GameServer *server_create(Game *game, const GameConfig *config) {
  if (!game || !config)
    return NULL;
  GameServer *s = (GameServer *)calloc(1, sizeof(GameServer));
  if (!s)
    return NULL;
  s->game = game;
  s->conf = *config;
  s->listen_socket = -1;
  for (int i = 0; i < MAX_PLAYERS; ++i)
    s->client_sockets[i] = -1;
  s->running = false;
  s->accepting = true;
  s->frame = 0;
  s->max_comm_ms = 100;
  return s;
}

void server_destroy(GameServer *server) {
  if (!server)
    return;
  if (server->listen_socket >= 0)
    close(server->listen_socket);
  for (int i = 0; i < MAX_PLAYERS; ++i) {
    if (server->client_sockets[i] >= 0)
      close(server->client_sockets[i]);
  }
  free(server);
}

int server_listen(GameServer *server, uint16_t port) {
  if (!server)
    return -1;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;
  int on = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }
  if (listen(sock, 16) < 0) {
    close(sock);
    return -1;
  }
  if (set_nonblocking(sock) < 0) {
    close(sock);
    return -1;
  }
  server->listen_socket = sock;
  return 0;
}

void server_accept_clients(GameServer *s) {
  // Continuously accept clients while accepting flag is true
  // This is meant to run in a dedicated thread
  ulog_info("accept_clients: starting accept loop");
  while (s->accepting) {
    // Count current clients
    int client_count = 0;
    for (int i = 1; i < MAX_PLAYERS; i++) {
      if (s->client_sockets[i] >= 0) {
        client_count++;
      }
    }
    // Check if we've reached max clients
    if (client_count >= s->conf.max_clients) {
      ulog_trace("accept_clients: max clients reached (%d), waiting",
                 client_count);
      usleep(10000); // 10ms sleep
      continue;
    }
    int client_sock = accept(s->listen_socket, NULL, NULL);
    if (client_sock < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No pending connections, yield CPU briefly
        usleep(10000); // 10ms sleep
        continue;
      } else {
        ulog_error("accept_clients: accept error: %d", errno);
        usleep(10000);
        continue;
      }
    }
    ulog_debug("accept_clients: accepted new client socket %d", client_sock);
    // Set to blocking for handshake
    int flags = fcntl(client_sock, F_GETFL, 0);
    fcntl(client_sock, F_SETFL, flags & ~O_NONBLOCK);
    // Receive name
    char *name = NULL;
    ulog_trace("accept_clients: receiving player name");
    if (recv_string_packet(client_sock, &name) == 0) {
      if (name == NULL) {
        ulog_error("accept_clients: received null player name");
        close(client_sock);
        continue;
      }
      ulog_info("accept_clients: received player name: %s", name);
      // Add player
      PlayerId id = game_add_player(s->game, name);
      free(name);
      if (id != 0) {
        ulog_debug("accept_clients: added player with ID %d", id);
        const Player *p = game_get_player(s->game, id);
        if (p && send_color_packet(client_sock, p->color) == 0) {
          ulog_info("accept_clients: sent color R=%d G=%d B=%d to player %d",
                    p->color.r, p->color.g, p->color.b, id);
          // Set back to non-blocking for game loop
          set_nonblocking(client_sock);
          s->client_sockets[id] = client_sock;
          ulog_info("accept_clients: client %d fully connected", id);
          continue; // accept more
        } else {
          ulog_error("accept_clients: failed to send color to player %d", id);
        }
      } else {
        ulog_error("accept_clients: failed to add player to game");
      }
    } else {
      ulog_debug("accept_clients: failed to receive player name");
    }
    // Handshake failed
    ulog_debug("accept_clients: handshake failed, closing socket %d",
               client_sock);
    close(client_sock);
  }
  ulog_info("accept_clients: exiting accept loop");
}

// --- Server loop helpers -------------------------------------------------

// Mark currently active clients and return their count
static int mark_active_clients(GameServer *s,
                               bool clients_unsent[MAX_PLAYERS]) {
  int active_clients = 0;
  for (int id = 1; id < MAX_PLAYERS; ++id) {
    if (s->client_sockets[id] >= 0) {
      clients_unsent[id] = true;
      active_clients++;
    }
  }
  return active_clients;
}

// Attempt to send game state to all clients still pending a send.
// Marks clients as ready to receive on success. Drops clients on failure.
// Returns number of clients successfully sent in this pass.
static int attempt_send_to_clients(GameServer *s,
                                   bool clients_unsent[MAX_PLAYERS],
                                   bool to_recv[MAX_PLAYERS]) {
  int sent_count = 0;
  for (int id = 1; id < MAX_PLAYERS; ++id) {
    if (clients_unsent[id]) {
      int sock = s->client_sockets[id];
      if (send_game_state_packet(s, sock) == 0) {
        ulog_trace("server_run: sent game state to client %d", id);
        clients_unsent[id] = false;
        to_recv[id] = true;
        sent_count++;
      } else {
        ulog_warn("server_run: failed to send to client %d, dropping", id);
        // drop client on send failure
        if (sock >= 0)
          close(sock);
        s->client_sockets[id] = -1;
        clients_unsent[id] = false;
        to_recv[id] = false;
        game_remove_player(s->game, (PlayerId)id);
      }
    }
  }
  if (sent_count > 0) {
    ulog_trace("server_run: sent game state to %d clients", sent_count);
  }
  return sent_count;
}

// Prepare the read fd set from sockets that we expect directions from.
// Fills rfds, sets maxfd and returns number of waiting clients via *waiting.
static void prepare_read_fds(GameServer *s, const bool *to_recv, fd_set *rfds,
                             int *maxfd, int *waiting) {
  FD_ZERO(rfds);
  int local_maxfd = -1;
  int local_waiting = 0;
  for (int id = 1; id < MAX_PLAYERS; ++id) {
    int sock = s->client_sockets[id];
    if (to_recv[id] && sock >= 0) {
      FD_SET(sock, rfds);
      if (sock > local_maxfd)
        local_maxfd = sock;
      local_waiting++;
    }
  }
  if (maxfd)
    *maxfd = local_maxfd;
  if (waiting)
    *waiting = local_waiting;
}

// Handle ready fds: receive move directions from clients whose sockets are set.
// Returns number of successful receives in this pass.
static int attempt_recv_from_ready(GameServer *s, const fd_set *rfds,
                                   bool *to_recv, Direction *directions) {
  int recv_count = 0;
  for (int id = 1; id < MAX_PLAYERS; ++id) {
    int sock = s->client_sockets[id];
    if (to_recv[id] && sock >= 0 && FD_ISSET(sock, rfds)) {
      int32_t dir = 0;
      if (recv_move_direction(sock, &dir) == 0) {
        ulog_trace("server_run: received direction %d from client %d", dir, id);
        if (dir < 0)
          dir = 0;
        if (dir > 3)
          dir = 3;
        directions[id] = (Direction)dir;
        to_recv[id] = false;
        recv_count++;
      } else {
        ulog_warn("server_run: failed to recv from client %d, dropping", id);
        // Drop client on recv failure
        close(sock);
        s->client_sockets[id] = -1;
        to_recv[id] = false;
        game_remove_player(s->game, (PlayerId)id);
      }
    }
  }
  if (recv_count > 0) {
    ulog_trace("server_run: received moves from %d clients", recv_count);
  }
  return recv_count;
}

// Count remaining clients that either still need sending or receiving.
static int count_remaining_work(const bool *clients_unsent,
                                const bool *to_recv) {
  int remaining = 0;
  for (int id = 1; id < MAX_PLAYERS; ++id) {
    if (clients_unsent[id] || to_recv[id])
      remaining++;
  }
  return remaining;
}

// Milliseconds elapsed since a given start time.
static long elapsed_ms_since(const struct timeval *start) {
  struct timeval now;
  gettimeofday(&now, NULL);
  long elapsed_ms = (now.tv_sec - start->tv_sec) * 1000L +
                    (now.tv_usec - start->tv_usec) / 1000L;
  return elapsed_ms;
}

// Sleep to maintain the target frame time if the frame ran too fast.
static void maintain_fps(const struct timeval *frame_start, long target_ms) {
  struct timeval now;
  gettimeofday(&now, NULL);
  long frame_ms = (now.tv_sec - frame_start->tv_sec) * 1000L +
                  (now.tv_usec - frame_start->tv_usec) / 1000L;
  if (frame_ms < target_ms) {
    long sleep_ms = target_ms - frame_ms;
    ulog_trace("server_run: sleeping %ld ms to maintain fps", sleep_ms);
    usleep((useconds_t)(sleep_ms * 1000));
  }
}

void server_run(GameServer *s) {
  if (!s)
    return;
  ulog_debug("server_run: starting server loop");
  s->running = true;
  struct timeval frame_start;
  while (s->running && !game_is_over(s->game)) {
    gettimeofday(&frame_start, NULL);
    game_set_frame(s->game, s->frame);
    ulog_trace("server_run: frame %u", s->frame);
    // Clone current active clients and initialize per-frame buffers
    bool clients_unsent[MAX_PLAYERS] = {false};
    bool to_recv[MAX_PLAYERS] = {false};
    Direction directions[MAX_PLAYERS] = {0};
    // Send game state to all active clients
    int active_clients = mark_active_clients(s, clients_unsent);
    ulog_trace("server_run: found %d active clients to send state to",
               active_clients);
    struct timeval comm_start;
    gettimeofday(&comm_start, NULL);
    for (;;) {
      attempt_send_to_clients(s, clients_unsent, to_recv);
      // Attempt receives (non-blocking sockets, use select with 0 timeout)
      fd_set rfds;
      int maxfd = -1;
      int waiting = 0;
      prepare_read_fds(s, to_recv, &rfds, &maxfd, &waiting);
      if (waiting == 0) {
        ulog_trace(
            "server_run: no clients waiting for recv, breaking comm loop");
        break;
      }
      struct timeval tv = {0, 0};
      int sel = select(maxfd + 1, &rfds, NULL, NULL, &tv);
      ulog_trace("server_run: select returned %d (waiting for %d clients)", sel,
                 waiting);
      if (sel > 0) {
        (void)attempt_recv_from_ready(s, &rfds, to_recv, directions);
      }
      long elapsed_ms = elapsed_ms_since(&comm_start);
      if (elapsed_ms > s->max_comm_ms) {
        ulog_trace("server_run: communication timeout (%ld ms), breaking",
                   elapsed_ms);
        break;
      }
      // If nothing to send and no fds ready, continue loop until timeout
      if (count_remaining_work(clients_unsent, to_recv) == 0) {
        break;
      }
    }
    ulog_trace("server_run: moving players for frame %u", s->frame);
    game_move_players(s->game, directions);
    s->frame++;
    ulog_trace("server_run: frame %u complete", s->frame - 1);
    // Maintain ~30 fps
    const long target_ms = 33; // ~30fps
    maintain_fps(&frame_start, target_ms);
  }
  ulog_debug("server_run: exiting server loop (running=%d, game_over=%d)",
             s->running, game_is_over(s->game));
}

void server_stop(GameServer *server) {
  if (server) {
    ulog_debug("server_stop: stopping server");
    server->running = false;
  }
}

void server_set_accepting_clients(GameServer *server, bool accepting) {
  if (server)
    server->accepting = accepting;
}

uint32_t server_get_frame(const GameServer *server) {
  return server ? server->frame : 0;
}
