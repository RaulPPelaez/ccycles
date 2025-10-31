#include "c_api.h"
#include "server/configuration.hpp"
#include "server/game_logic.hpp"
#include "server/server.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace cycles_server;

// Test fixture that manages a server instance
class CApiTest : public ::testing::Test {
protected:
  std::shared_ptr<Game> game;
  std::unique_ptr<GameServer> server;
  std::thread serverThread;
  std::thread acceptThread;
  std::string configFile;
  std::string port;
  std::shared_ptr<Configuration> conf;

  void SetUp() override {
    spdlog::set_level(spdlog::level::debug);
    // Create a temporary config file
    configFile = createTempConfig();

    // Choose a random port for testing (use a high port to avoid conflicts)
    // Use time and test instance pointer for more randomness
    unsigned seed =
        std::chrono::system_clock::now().time_since_epoch().count() +
        reinterpret_cast<uintptr_t>(this);
    port = std::to_string(20000 + (seed % 40000));

    // Set the port environment variable
    setenv("CYCLES_PORT", port.c_str(), 1);

    // Create game and server
    conf = std::make_shared<Configuration>(configFile);
    game = std::make_shared<Game>(*conf);
    server = std::make_unique<GameServer>(game, *conf);

    // Start accepting clients
    acceptThread = std::thread(&GameServer::acceptClients, server.get());

    // Give the server a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    // Stop the server
    if (server) {
      server->setAcceptingClients(false);
      if (acceptThread.joinable()) {
        acceptThread.join();
      }
      server->stop();
      if (serverThread.joinable()) {
        serverThread.join();
      }
    }

    // Clean up temp config file
    if (!configFile.empty()) {
      std::remove(configFile.c_str());
    }

    // Clean up environment
    unsetenv("CYCLES_PORT");
  }

  void startServerGameLoop() {
    serverThread = std::thread(&GameServer::run, server.get());
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
  Connection conn;
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
  Connection conn1, conn2, conn3;
  ASSERT_EQ(cycles_connect("Player1", "127.0.0.1", port.c_str(), &conn1), 0);
  ASSERT_EQ(cycles_connect("Player2", "127.0.0.1", port.c_str(), &conn2), 0);
  ASSERT_EQ(cycles_connect("Player3", "127.0.0.1", port.c_str(), &conn3), 0);
  EXPECT_STREQ(conn1.name, "Player1");
  EXPECT_STREQ(conn2.name, "Player2");
  EXPECT_STREQ(conn3.name, "Player3");
  auto players = game->getPlayers();
  EXPECT_EQ(players.size(), 3);
  cycles_disconnect(&conn1);
  cycles_disconnect(&conn2);
  cycles_disconnect(&conn3);
}

bool compare_grids(const uint8_t *grid1, int width1, int height1,
                   const std::vector<sf::Uint8> &grid2) {
  if (width1 * height1 != static_cast<int>(grid2.size())) {
    spdlog::error("Grid size mismatch: {}x{} vs {}", width1, height1,
                  grid2.size());
    return false;
  }
  for (int y = 0; y < height1; ++y) {
    for (int x = 0; x < width1; ++x) {
      if (grid1[y * width1 + x] != grid2[y * width1 + x]) {
        spdlog::error("Grid mismatch at {}x{}: {} vs {}", x, y,
                      grid1[y * width1 + x], grid2[y * width1 + x]);
        return false;
      }
    }
  }
  return true;
}
// Receive the game state structure and verify its contents
TEST_F(CApiTest, GameStateStructure) {
  // add a player
  Connection conn[2]; // Two players, so that the game is not over immediately
  for (int i = 0; i < 2; i++) {
    std::string name = "TestPlayer" + std::to_string(i);
    const char *name_c = name.c_str();
    int result = cycles_connect(name_c, "127.0.0.1", port.c_str(), &conn[i]);
    ASSERT_EQ(result, 0) << "Failed to connect to server";
  }
  server->setAcceptingClients(false);
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
  // Get a copy of the Game grid
  auto grid_copy = std::vector(game->getGrid());
  startServerGameLoop();
  // Check the correctness of the game state
  GameState gs = {};
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state";
    EXPECT_EQ(gs.grid_width, (uint32_t)conf->gridWidth);
    EXPECT_EQ(gs.grid_height, (uint32_t)conf->gridHeight);
    EXPECT_EQ(gs.player_count, 2u);
    EXPECT_NE(gs.players, nullptr);
    EXPECT_NE(gs.grid, nullptr);
    Vec2i player_pos = {gs.players[i].x, gs.players[i].y};
    // Assert that the grid is correct at the player position
    EXPECT_EQ(gs.grid[player_pos.y * gs.grid_width + player_pos.x],
              gs.players[i].id);
    // Assert that the server grid also matches at the player's position
    EXPECT_EQ(grid_copy[player_pos.y * conf->gridWidth + player_pos.x],
              gs.players[i].id);
    // Compare the entire grid
    ASSERT_TRUE(
        compare_grids(gs.grid, conf->gridWidth, conf->gridHeight, grid_copy));
    EXPECT_EQ(gs.frame_number, 0u);
  }
  free_game_state(&gs);
  for (int i = 0; i < 2; i++) {
    cycles_disconnect(&conn[i]);
  }
  server->stop();
  if (serverThread.joinable()) {
    serverThread.join();
  }
}

TEST_F(CApiTest, SendMove) {
  // add a player
  Connection conn[2]; // Two players, so that the game is not over immediately
  for (int i = 0; i < 2; i++) {
    std::string name = "TestPlayer" + std::to_string(i);
    const char *name_c = name.c_str();
    int result = cycles_connect(name_c, "127.0.0.1", port.c_str(), &conn[i]);
    ASSERT_EQ(result, 0) << "Failed to connect to server";
  }
  server->setAcceptingClients(false);
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
  startServerGameLoop();
  // Check the correctness of the game state
  GameState gs = {};
  std::vector<Vec2i> initial_positions;
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state";
    EXPECT_EQ(gs.frame_number, 0u);
    Vec2i player_pos = {gs.players[i].x, gs.players[i].y};
    initial_positions.push_back(player_pos);
    if (player_pos.y == 0 ||
        gs.grid[(player_pos.y - 1) * gs.grid_width + player_pos.x] != 0) {
      // Move south instead
      ASSERT_EQ(cycles_send_move_i32(&conn[i], south), 0);
    } else {
      // Move north
      ASSERT_EQ(cycles_send_move_i32(&conn[i], north), 0);
    }
  }
  for (int i = 0; i < 2; i++) {
    int result = cycles_recv_game_state(conn[i].sock, &gs);
    ASSERT_EQ(result, 0) << "Failed to receive game state after move";
    EXPECT_EQ(gs.frame_number, 1u);
    Vec2i player_pos = {gs.players[i].x, gs.players[i].y};
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

  free_game_state(&gs);
  for (int i = 0; i < 2; i++) {
    cycles_disconnect(&conn[i]);
  }
  server->stop();
  if (serverThread.joinable()) {
    serverThread.join();
  }
}

TEST_F(CApiTest, InvalidConnection) {
  Connection conn;
  int result = cycles_connect("TestPlayer", "127.0.0.1", "99999", &conn);
  EXPECT_NE(result, 0) << "Connection to invalid port should fail";
}

TEST_F(CApiTest, LongPlayerName) {
  Connection conn;
  std::string long_name(MAX_NAME_LEN + 50, 'X');
  int result =
      cycles_connect(long_name.c_str(), "127.0.0.1", port.c_str(), &conn);
  ASSERT_EQ(result, 0);
  EXPECT_EQ(strlen(conn.name), MAX_NAME_LEN);
  cycles_disconnect(&conn);
}
