#include "c_api.h"
#include "cserver/game_logic.h"
#include "cserver/server.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

// Test fixture that manages a C server instance
class CApiTest : public ::testing::Test {
protected:
  Game *game;
  GameServer *server;
  std::thread acceptThread;
  std::thread serverThread;
  std::string configFile;
  std::string port;
  GameConfig config;

  void SetUp() override {
    configFile = createTempConfig();
    unsigned seed =
        std::chrono::system_clock::now().time_since_epoch().count() +
        reinterpret_cast<uintptr_t>(this);
    port = std::to_string(20000 + (seed % 40000));
    setenv("CYCLES_PORT", port.c_str(), 1);
    if (game_config_load(configFile.c_str(), &config) != 0) {
      throw std::runtime_error("Failed to load config");
    }
    game = game_create(&config);
    if (!game) {
      throw std::runtime_error("Failed to create game");
    }
    server = server_create(game, &config);
    if (!server) {
      game_destroy(game);
      throw std::runtime_error("Failed to create game");
    }
    if (server_listen(server, std::stoi(port)) != 0) {
      server_destroy(server);
      game_destroy(game);
      throw std::runtime_error("Failed to start server");
    }
    // Start accept thread to allow clients to connect
    // server_accept_clients loops internally while accepting is true
    acceptThread = std::thread([this]() { server_accept_clients(server); });
  }

  void TearDown() override {
    if (server) {
      server_set_accepting_clients(server, false);
      if (acceptThread.joinable()) {
        acceptThread.join();
      }
      server_stop(server);
      if (serverThread.joinable()) {
        serverThread.join();
      }
      server_destroy(server);
    }
    if (game) {
      game_destroy(game);
    }
    if (!configFile.empty()) {
      std::remove(configFile.c_str());
    }
    unsetenv("CYCLES_PORT");
  }

  // Helper to start the game loop after clients have connected
  void startGameLoop() {
    server_set_accepting_clients(server, false);
    if (acceptThread.joinable()) {
      acceptThread.join();
    }
    serverThread = std::thread([this]() { server_run(server); });
    // Give server thread a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::string createTempConfig() {
    std::string conf_yaml = R"(
gameHeight: 600
gameWidth: 600
gameBannerHeight: 100
gridHeight: 50
gridWidth: 50
maxClients: 10
enablePostProcessing: false
)";
    char temp_template[] = "/tmp/ccycles_test_XXXXXX";
    int fd = mkstemp(temp_template);
    if (fd == -1) {
      throw std::runtime_error("Failed to create temporary config file");
    }
    std::string temp_file(temp_template);
    close(fd);
    std::ofstream out(temp_file);
    out << conf_yaml;
    out.close();
    return temp_file;
  }
};

TEST_F(CApiTest, ConnectAndDisconnect) {
  cycles_connection conn;
  int result = cycles_connect("TestPlayer", "127.0.0.1", port.c_str(), &conn);
  ASSERT_EQ(result, 0) << "Failed to connect to server";
  EXPECT_TRUE(ISVALIDSOCKET(conn.sock));
  EXPECT_STREQ(conn.name, "TestPlayer");
  EXPECT_LE(conn.color.r, 255);
  EXPECT_LE(conn.color.g, 255);
  EXPECT_LE(conn.color.b, 255);
  cycles_disconnect(&conn);
}

TEST_F(CApiTest, MultipleClientsConnect) {
  cycles_connection conn1, conn2, conn3;
  ASSERT_EQ(cycles_connect("Player1", "127.0.0.1", port.c_str(), &conn1), 0);
  ASSERT_EQ(cycles_connect("Player2", "127.0.0.1", port.c_str(), &conn2), 0);
  ASSERT_EQ(cycles_connect("Player3", "127.0.0.1", port.c_str(), &conn3), 0);
  EXPECT_STREQ(conn1.name, "Player1");
  EXPECT_STREQ(conn2.name, "Player2");
  EXPECT_STREQ(conn3.name, "Player3");
  Player *player_ptrs[256];
  uint32_t player_count = game_get_players(game, player_ptrs);
  EXPECT_EQ(player_count, 3u);

  // Start game loop (no longer accepting clients, server begins game)
  startGameLoop();

  cycles_disconnect(&conn1);
  cycles_disconnect(&conn2);
  cycles_disconnect(&conn3);
}

bool compare_grids(const uint8_t *grid1, int width1, int height1,
                   const uint8_t *grid2) {
  for (int y = 0; y < height1; ++y) {
    for (int x = 0; x < width1; ++x) {
      if (grid1[y * width1 + x] != grid2[y * width1 + x]) {
        return false;
      }
    }
  }
  return true;
}
// Receive the game state structure and verify its contents
TEST_F(CApiTest, GameStateStructure) {
  // add a player
  cycles_connection
      conn[2]; // Two players, so that the game is not over immediately
  for (int i = 0; i < 2; i++) {
    std::string name = "TestPlayer" + std::to_string(i);
    const char *name_c = name.c_str();
    int result = cycles_connect(name_c, "127.0.0.1", port.c_str(), &conn[i]);
    ASSERT_EQ(result, 0) << "Failed to connect to server";
  }

  // Get a copy of the Game grid before starting server loop
  uint32_t grid_width, grid_height;
  game_get_grid_size(game, &grid_width, &grid_height);
  const uint8_t *grid_ptr = game_get_grid(game);
  std::vector<uint8_t> grid_copy(grid_ptr,
                                 grid_ptr + (grid_width * grid_height));
  startGameLoop();
  cycles_game_state gs = {};
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state";
    EXPECT_EQ(gs.grid_width, config.grid_width);
    EXPECT_EQ(gs.grid_height, config.grid_height);
    EXPECT_EQ(gs.player_count, 2u);
    EXPECT_NE(gs.players, nullptr);
    EXPECT_NE(gs.grid, nullptr);
    cycles_vec2i player_pos = {gs.players[i].x, gs.players[i].y};
    // Assert that the grid is correct at the player position
    EXPECT_EQ(gs.grid[player_pos.y * gs.grid_width + player_pos.x],
              gs.players[i].id);
    // Assert that the server grid also matches at the player's position
    EXPECT_EQ(grid_copy[player_pos.y * grid_width + player_pos.x],
              gs.players[i].id);
    // Compare the entire grid
    ASSERT_TRUE(
        compare_grids(gs.grid, grid_width, grid_height, grid_copy.data()));
    EXPECT_EQ(gs.frame_number, 0u);
  }
  cycles_free_game_state(&gs);
  for (int i = 0; i < 2; i++) {
    cycles_disconnect(&conn[i]);
  }
}

TEST_F(CApiTest, SendMove) {
  // add a player
  cycles_connection
      conn[2]; // Two players, so that the game is not over immediately
  for (int i = 0; i < 2; i++) {
    std::string name = "TestPlayer" + std::to_string(i);
    const char *name_c = name.c_str();
    int result = cycles_connect(name_c, "127.0.0.1", port.c_str(), &conn[i]);
    ASSERT_EQ(result, 0) << "Failed to connect to server";
  }
  startGameLoop();
  cycles_game_state gs = {};
  std::vector<cycles_vec2i> initial_positions;
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state";
    EXPECT_EQ(gs.frame_number, 0u);
    cycles_vec2i player_pos = {gs.players[i].x, gs.players[i].y};
    initial_positions.push_back(player_pos);
    if (player_pos.y == 0 ||
        gs.grid[(player_pos.y - 1) * gs.grid_width + player_pos.x] != 0) {
      // Move south instead
      ASSERT_EQ(cycles_send_move_i32(&conn[i], cycles_south), 0);
    } else {
      // Move north
      ASSERT_EQ(cycles_send_move_i32(&conn[i], cycles_north), 0);
    }
  }
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state after move";
    EXPECT_EQ(gs.frame_number, 1u);
    cycles_vec2i player_pos = {gs.players[i].x, gs.players[i].y};
    // Check that the player has moved by one cell in the expected direction
    if (player_pos.y == 0 ||
        gs.grid[(player_pos.y - 1) * gs.grid_width + player_pos.x] != 0) {
      // Moved south
      EXPECT_EQ(player_pos.y, initial_positions[i].y + 1);
    } else {
      // Moved north
      EXPECT_EQ(player_pos.y, initial_positions[i].y - 1);
    }
  }

  cycles_free_game_state(&gs);
  for (int i = 0; i < 2; i++) {
    cycles_disconnect(&conn[i]);
  }
}

TEST_F(CApiTest, InvalidConnection) {
  cycles_connection conn;
  int result = cycles_connect("TestPlayer", "127.0.0.1", "99999", &conn);
  EXPECT_NE(result, 0) << "Connection to invalid port should fail";
}

TEST_F(CApiTest, LongPlayerName) {
  cycles_connection conn;
  std::string long_name(MAX_NAME_LEN + 50, 'X');
  int result =
      cycles_connect(long_name.c_str(), "127.0.0.1", port.c_str(), &conn);
  ASSERT_EQ(result, 0);
  EXPECT_EQ(strlen(conn.name), MAX_NAME_LEN);
  cycles_disconnect(&conn);
}
