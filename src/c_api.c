#include "c_api.h"
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// Windows doesn't have ssize_t, define it as a signed type matching size_t
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ulog.h>
// Max payload guard to avoid pathological lengths.
enum { CYCLES_MAX_PAYLOAD = 64 * 1024 };
// Upper bounds to keep things sane in case of malformed packets
enum { CYCLES_MAX_PACKET = 32 * 1024 * 1024 };
enum { CYCLES_MAX_STRING = 16 * 1024 * 1024 };

static int send_all(SOCKET fd, const void *buf, size_t len) {
  // Ensure fd is valid
  if (fd < 0 || !buf) {
    errno = EINVAL;
    return -1;
  }
  const unsigned char *p = (const unsigned char *)buf;
  while (len) {
    ssize_t n = send(fd, p, len, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      return -1; // peer closed unexpectedly
    p += (size_t)n;
    len -= (size_t)n;
  }
  return 0;
}

static int send_cycles_string_packet(int sock, const char *s) {
  uint32_t name_len = (uint32_t)strlen(s);      // no NUL in payload
  uint32_t payload_len = 4u + name_len;         // [len_be][bytes]
  uint32_t packet_size_be = htonl(payload_len); // outer size (CYCLES frame)
  uint32_t name_len_be = htonl(name_len);       // inner string length

  // Build a single contiguous buffer: [packet_size][name_len][name_bytes]
  // (You could also send in 3 calls; a single buffer is simpler.)
  size_t total = 4u + payload_len;
  unsigned char *buf = (unsigned char *)malloc(total);
  if (!buf) {
    errno = ENOMEM;
    return -1;
  }

  memcpy(buf + 0, &packet_size_be, 4);
  memcpy(buf + 4, &name_len_be, 4);
  memcpy(buf + 8, s, name_len);

  int rc = send_all(sock, buf, total);
  free(buf);
  return rc;
}

static int send_cycles_i32_packet(SOCKET sock, int32_t value) {
  uint32_t payload_len_be = htonl(4u);
  uint32_t v_be = htonl((uint32_t)value);
  unsigned char buf[8];
  memcpy(buf, &payload_len_be, 4);
  memcpy(buf + 4, &v_be, 4);
  return send_all(sock, buf, sizeof buf);
}

static int recv_all(SOCKET fd, void *buf, size_t len) {
  // Ensure fd is valid
  if (fd < 0 || !buf) {
    errno = EINVAL;
    return -1;
  }
  uint8_t *p = (uint8_t *)buf;
  while (len) {
    ssize_t n = recv(fd, p, len, 0);
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

static int recv_cycles_packet_len(SOCKET fd, uint32_t *out_len) {
  uint32_t be = 0;
  if (recv_all(fd, &be, sizeof(be)) < 0)
    return -1;
  *out_len = ntohl(be);
  return 0;
}

static int recv_cycles_color(SOCKET fd, cycles_rgb *out) {
  if (!out) {
    errno = EINVAL;
    return -1;
  }
  uint32_t payload_len = 0;
  if (recv_cycles_packet_len(fd, &payload_len) < 0)
    return -1;
  if (payload_len == 0 || payload_len > CYCLES_MAX_PAYLOAD) {
    errno = EPROTO;
    return -1;
  }
  if (payload_len != 3) {
    char *drain = (char *)malloc(payload_len);
    if (drain) {
      (void)recv_all(fd, drain, payload_len);
      free(drain);
    }
    errno = EPROTO;
    return -1;
  }
  uint8_t buf[3];
  if (recv_all(fd, buf, sizeof(buf)) < 0)
    return -1;
  out->r = buf[0];
  out->g = buf[1];
  out->b = buf[2];
  return 0;
}

static int recv_cycles_packet(SOCKET fd, uint8_t **out, uint32_t *out_len) {
  uint32_t be = 0;
  if (recv_all(fd, &be, sizeof(be)) < 0) // First 4 bytes: message length
    return -1;
  uint32_t len = ntohl(be);
  if (len == 0 || len > CYCLES_MAX_PACKET) {
    errno = EPROTO;
    return -1;
  }
  uint8_t *buf = (uint8_t *)malloc(len);
  if (!buf) {
    errno = ENOMEM;
    return -1;
  }
  if (recv_all(fd, buf, len) < 0) {
    free(buf);
    return -1;
  }
  *out = buf;
  *out_len = len;
  return 0;
}

// ---------- raw readers over a memory buffer ----------
static int rd_bytes(const uint8_t **p, uint32_t *rem, void *dst, uint32_t n) {
  if (*rem < n) {
    errno = EPROTO;
    return -1;
  }
  memcpy(dst, *p, n);
  *p += n;
  *rem -= n;
  return 0;
}
static int rd_u32(const uint8_t **p, uint32_t *rem, uint32_t *out) {
  uint32_t be;
  if (rd_bytes(p, rem, &be, 4) < 0)
    return -1;
  *out = ntohl(be);
  return 0;
}
static int rd_i32(const uint8_t **p, uint32_t *rem, int32_t *out) {
  uint32_t u;
  if (rd_u32(p, rem, &u) < 0)
    return -1;
  *out = (int32_t)u;
  return 0;
}
static int rd_u8(const uint8_t **p, uint32_t *rem, uint8_t *out) {
  return rd_bytes(p, rem, out, 1);
}
static int rd_string(const uint8_t **p, uint32_t *rem, char **out) {
  uint32_t n = 0;
  if (rd_u32(p, rem, &n) < 0)
    return -1;
  if (n > CYCLES_MAX_STRING) {
    errno = EPROTO;
    return -1;
  }
  char *s = (char *)malloc((size_t)n + 1);
  if (!s) {
    errno = ENOMEM;
    return -1;
  }
  if (rd_bytes(p, rem, s, n) < 0) {
    free(s);
    return -1;
  }
  s[n] = '\0';
  *out = s;
  return 0;
}

static SOCKET cycles_create_socket(const char *host, const char *port) {
  ulog_debug("Configuring remote address...");
  // Configure a remote address
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM; // TCP, for UDP use SOCK_DGRAM
  struct addrinfo *peer_address;
  if (getaddrinfo(host, port, &hints, &peer_address) != 0) {
    ulog_error("getaddrinfo() failed. (%d)", GETSOCKETERRNO());
    return EXIT_FAILURE;
  }
  char address_buffer[NI_MAXHOST];
  char service_buffer[NI_MAXSERV];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST | NI_NUMERICSERV);
  ulog_debug("Remote address is %s:%s", address_buffer, service_buffer);
  ulog_debug("Creating socket...");
  SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_peer)) {
    ulog_error("socket() failed. (%d)", GETSOCKETERRNO());
    return EXIT_FAILURE;
  }
  ulog_debug("Connecting to remote...");
  if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen) !=
      0) {
    ulog_error("connect() failed. (%d)", GETSOCKETERRNO());
    ulog_error("%s", strerror(GETSOCKETERRNO()));
    return EXIT_FAILURE;
  }
  freeaddrinfo(peer_address);
  return socket_peer;
}

int cycles_connect(const char *name, const char *host, const char *port,
                   cycles_connection *conn) {
#if defined(_WIN32)
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    ulog_error("WSAStartup failed: %d", iResult);
    return 1;
  }
#endif
  conn->sock = cycles_create_socket(host, port);
  if (!ISVALIDSOCKET(conn->sock)) {
    ulog_error("Failed to create socket and connect.");
    return -1;
  }
  ulog_trace("Sending player name: %s", name);
  if (send_cycles_string_packet(conn->sock, name) != 0) {
    ulog_error("send() failed. (%d)", GETSOCKETERRNO());
    return -1;
  }
  ulog_trace("Player name sent.");
  cycles_rgb color;
  if (recv_cycles_color(conn->sock, &color) != 0) {
    ulog_error("recv() failed. (%d)", GETSOCKETERRNO());
    return -1;
  }
  ulog_trace("Received color: R=%d G=%d B=%d", color.r, color.g, color.b);
  conn->color = color;
  strncpy(conn->name, name, MAX_NAME_LEN);
  conn->name[MAX_NAME_LEN] = '\0';
  return 0;
}

void cycles_disconnect(cycles_connection *conn) {
  if (conn && ISVALIDSOCKET(conn->sock)) {
    CLOSESOCKET(conn->sock);
    conn->sock = -1;
  }
#ifdef _WIN32
  WSACleanup();
#endif
}

void cycles_free_game_state(cycles_game_state *gs) {
  if (!gs)
    return;
  if (gs->players) {
    for (uint32_t i = 0; i < gs->player_count; ++i)
      free(gs->players[i].name);
    gs->player_count = 0;
    free(gs->players);
    gs->players = NULL;
  }
  free(gs->grid);
  gs->grid = NULL;
  gs->grid_width = 0;
  gs->grid_height = 0;
  gs->frame_number = 0;
}

int cycles_recv_game_state(SOCKET sock, cycles_game_state *out) {
  if (!out) {
    errno = EINVAL;
    return -1;
  }
  memset(out, 0, sizeof(*out));
  uint8_t *pkt = NULL;
  uint32_t len = 0;
  if (recv_cycles_packet(sock, &pkt, &len) < 0)
    return -1;
  ulog_debug("recv_game_state: got %u bytes", len);
  const uint8_t *p = pkt;
  uint32_t rem = len;

  // gridWidth, gridHeight, playerCount
  if (rd_u32(&p, &rem, &out->grid_width) < 0 ||
      rd_u32(&p, &rem, &out->grid_height) < 0 ||
      rd_u32(&p, &rem, &out->player_count) < 0) {
    free(pkt);
    cycles_free_game_state(out);
    return -1;
  }
  ulog_trace("recv_game_state: last frame number = %u", out->frame_number);
  ulog_debug("recv_game_state: grid %ux%u with %u players", out->grid_width,
             out->grid_height, out->player_count);
  // players array
  if (out->player_count > (UINT32_MAX / (uint32_t)sizeof(cycles_player))) {
    free(pkt);
    cycles_free_game_state(out);
    errno = EPROTO;
    return -1;
  }
  {
    cycles_player *tmp;
    if (out->players) {
      tmp = (cycles_player *)realloc(out->players,
                                     out->player_count * sizeof(cycles_player));
    } else {
      tmp = (cycles_player *)calloc(out->player_count, sizeof(cycles_player));
    }
    if (tmp) {
      out->players = tmp;
    } else {
      free(pkt);
      cycles_free_game_state(out);
      return -1;
    }
  }
  // players loop
  for (uint32_t i = 0; i < out->player_count; ++i) {
    int32_t x, y;
    uint8_t r, g, b;
    uint8_t id = 0;
    char *name = NULL;

    if (rd_i32(&p, &rem, &x) < 0 || rd_i32(&p, &rem, &y) < 0 ||
        rd_u8(&p, &rem, &r) < 0 || rd_u8(&p, &rem, &g) < 0 ||
        rd_u8(&p, &rem, &b) < 0 || rd_string(&p, &rem, &name) < 0 ||
        rd_u8(&p, &rem, &id) < 0) {
      free(name);
      free(pkt);
      return -1;
    }
    ulog_trace("Player %u: '%s' at (%d,%d) color R=%d G=%d B=%d", id, name, x,
               y, r, g, b);
    out->players[i].x = x;
    out->players[i].y = y;
    out->players[i].color = (cycles_rgb){r, g, b};
    if (out->players[i].name)
      free(out->players[i].name); // in case of realloc
    out->players[i].name = name;
    out->players[i].id = id;
  }
  uint32_t frame = 0;
  if (rd_u32(&p, &rem, &frame) < 0) {
    free(pkt);
    return -1;
  }
  out->frame_number = frame;
  // grid: Uint8[gridWidth * gridHeight]
  // overflow-safe: (size_t) w * h
  size_t grid_sz =
      (size_t)out->grid_width * (size_t)out->grid_height * sizeof(uint8_t);
  if ((out->grid_width != 0 && grid_sz / out->grid_width != out->grid_height) ||
      grid_sz > rem) {
    ulog_error("recv_game_state: invalid grid size, rem=%u grid_sz=%zu", rem,
               grid_sz);
    free(pkt);
    errno = EPROTO;
    return -1;
  }
  if (grid_sz) {
    uint8_t *tmp;
    if (out->grid) {
      tmp = (uint8_t *)realloc(out->grid, grid_sz * sizeof(uint8_t));
    } else {
      tmp = (uint8_t *)malloc(grid_sz * sizeof(uint8_t));
    }
    if (tmp) {
      out->grid = tmp;
    } else {
      free(pkt);
      return -1;
    }
    ulog_trace("recv_game_state: allocated grid");
    if (rd_bytes(&p, &rem, out->grid, (uint32_t)grid_sz) < 0) {
      free(pkt);
      return -1;
    }
    ulog_trace("recv_game_state: grid data read");
  }
  ulog_debug("recv_game_state: %u bytes remaining after parse", rem);
  // final sanity check: must have consumed everything
  if (rem != 0) {
    free(pkt);
    errno = EPROTO;
    return -1;
  }
  free(pkt);
  return 0;
}

int cycles_send_move_i32(cycles_connection *conn, int32_t dir) {
  ulog_trace("Sending move direction: %d", dir);
  if (!conn) {
    errno = EINVAL;
    return -1;
  }
  return send_cycles_i32_packet(conn->sock, dir);
}
