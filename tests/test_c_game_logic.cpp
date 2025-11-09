#include <fstream>
#include <gtest/gtest.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include "server/game_logic.h"
#include "server/player.h"
#include "server/types.h"
}

#ifdef _WIN32
static std::string create_temp_file(const char *prefix) {
  char temp_path[MAX_PATH];
  char temp_file[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_path);
  GetTempFileNameA(temp_path, prefix, 0, temp_file);
  return std::string(temp_file);
}
#else
static std::string create_temp_file(const char *tmpl_suffix) {
  std::string tmpl = std::string("/tmp/") + tmpl_suffix;
  std::vector<char> tmpl_buf(tmpl.begin(), tmpl.end());
  tmpl_buf.push_back('\0');
  int fd = mkstemp(tmpl_buf.data());
  if (fd == -1) {
    throw std::runtime_error("mkstemp failed");
  }
  ::close(fd);
  return std::string(tmpl_buf.data());
}
#endif

TEST(GameLogicTest, CreateGame) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};

  Game *game = game_create(&config);

  ASSERT_NE(game, nullptr);
  EXPECT_EQ(game_get_frame(game), 0);
  EXPECT_FALSE(game_is_over(game));

  uint32_t width, height;
  game_get_grid_size(game, &width, &height);
  EXPECT_EQ(width, 100);
  EXPECT_EQ(height, 100);

  game_destroy(game);
}

TEST(GameLogicTest, AddPlayer) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  EXPECT_NE(id, 0);
  Player *players[MAX_PLAYERS];
  uint32_t count = game_get_players(game, players);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(players[0]->id, id);
  EXPECT_STREQ(players[0]->name, "TestPlayer");
  game_destroy(game);
}

TEST(GameLogicTest, AddMultiplePlayers) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id1 = game_add_player(game, "Player1");
  PlayerId id2 = game_add_player(game, "Player2");
  PlayerId id3 = game_add_player(game, "Player3");
  EXPECT_NE(id1, 0);
  EXPECT_NE(id2, 0);
  EXPECT_NE(id3, 0);
  EXPECT_NE(id1, id2);
  EXPECT_NE(id2, id3);
  Player *players[MAX_PLAYERS];
  uint32_t count = game_get_players(game, players);
  EXPECT_EQ(count, 3);
  game_destroy(game);
}

TEST(GameLogicTest, RemovePlayer) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  EXPECT_NE(id, 0);
  Player *players[MAX_PLAYERS];
  EXPECT_EQ(game_get_players(game, players), 1);
  game_remove_player(game, id);
  EXPECT_EQ(game_get_players(game, players), 0);
  game_destroy(game);
}

TEST(GameLogicTest, GridInitiallyEmpty) {
  GameConfig config = {10, 10, 60, 100, 100, 10.0f, false};
  Game *game = game_create(&config);
  const uint8_t *grid = game_get_grid(game);
  ASSERT_NE(grid, nullptr);
  for (int i = 0; i < 100; i++) {
    if (grid[i] != 0) {
      EXPECT_EQ(grid[i], 0) << "Grid position " << i << " should be empty";
    }
  }
  game_destroy(game);
}

TEST(GameLogicTest, PlayerOccupiesGridCell) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  ASSERT_NE(id, 0);
  const uint8_t *grid = game_get_grid(game);
  uint32_t width, height;
  game_get_grid_size(game, &width, &height);
  bool found = false;
  for (uint32_t i = 0; i < width * height; i++) {
    if (grid[i] == id) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
  game_destroy(game);
}

TEST(GameLogicTest, MovePlayerNorth) {
  GameConfig config = {10, 10, 60, 100, 100, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  ASSERT_NE(id, 0);
  Player *players[MAX_PLAYERS];
  game_get_players(game, players);
  Vec2i initial_pos = players[0]->position;
  Direction directions[MAX_PLAYERS] = {north};
  directions[id] = north;
  if (initial_pos.y > 0) {
    game_move_players(game, directions);
    game_get_players(game, players);
    EXPECT_EQ(players[0]->position.y, initial_pos.y - 1);
    EXPECT_EQ(players[0]->position.x, initial_pos.x);
  }
  game_destroy(game);
}

TEST(GameLogicTest, MovePlayerEast) {
  GameConfig config = {10, 10, 60, 100, 100, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  ASSERT_NE(id, 0);
  Player *players[MAX_PLAYERS];
  game_get_players(game, players);
  Vec2i initial_pos = players[0]->position;
  Direction directions[MAX_PLAYERS] = {east};
  directions[id] = east;
  if (initial_pos.x < 9) {
    game_move_players(game, directions);
    game_get_players(game, players);
    EXPECT_EQ(players[0]->position.x, initial_pos.x + 1);
    EXPECT_EQ(players[0]->position.y, initial_pos.y);
  }
  game_destroy(game);
}

TEST(GameLogicTest, PlayerCollisionWithWall) {
  GameConfig config = {10, 10, 60, 100, 100, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  ASSERT_NE(id, 0);
  Player *players[MAX_PLAYERS];
  game_get_players(game, players);
  Direction directions[MAX_PLAYERS] = {north};
  directions[id] = north;
  for (int i = 0; i < 20; i++) {
    game_move_players(game, directions);
  }
  EXPECT_EQ(game_get_players(game, players), 0);
  game_destroy(game);
}

TEST(GameLogicTest, PlayerHasTailAfterMoving) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "TestPlayer");
  ASSERT_NE(id, 0);
  Direction directions[MAX_PLAYERS] = {east};
  directions[id] = east;
  for (int i = 0; i < 5; i++) {
    game_move_players(game, directions);
  }
  Player *players[MAX_PLAYERS];
  game_get_players(game, players);
  ASSERT_EQ(game_get_players(game, players), 1);
  TailNode *tail = players[0]->tail_linked_list;
  int tail_len = 0;
  while (tail) {
    tail_len++;
    tail = tail->next;
  }
  EXPECT_EQ(tail_len, 5);
  game_destroy(game);
}

TEST(GameLogicTest, GameOverWithOnePlayer) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  PlayerId id = game_add_player(game, "Player1");
  ASSERT_NE(id, 0);
  EXPECT_TRUE(game_is_over(game));
  game_destroy(game);
}

TEST(GameLogicTest, GameNotOverWithMultiplePlayers) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  game_add_player(game, "Player1");
  game_add_player(game, "Player2");
  EXPECT_FALSE(game_is_over(game));
  game_destroy(game);
}

TEST(GameLogicTest, FrameCounter) {
  GameConfig config = {100, 100, 60, 1000, 1000, 10.0f, false};
  Game *game = game_create(&config);
  EXPECT_EQ(game_get_frame(game), 0);
  game_set_frame(game, 42);
  EXPECT_EQ(game_get_frame(game), 42);
  game_set_frame(game, 100);
  EXPECT_EQ(game_get_frame(game), 100);
  game_destroy(game);
}

TEST(GameLogicTest, DirectionToVector) {
  Vec2i vec;
  vec = direction_to_vector(north);
  EXPECT_EQ(vec.x, 0);
  EXPECT_EQ(vec.y, -1);
  vec = direction_to_vector(east);
  EXPECT_EQ(vec.x, 1);
  EXPECT_EQ(vec.y, 0);
  vec = direction_to_vector(south);
  EXPECT_EQ(vec.x, 0);
  EXPECT_EQ(vec.y, 1);
  vec = direction_to_vector(west);
  EXPECT_EQ(vec.x, -1);
  EXPECT_EQ(vec.y, 0);
}

TEST(GameLogicTest, NullGameHandling) {
  EXPECT_EQ(game_add_player(nullptr, "Test"), 0);
  game_remove_player(nullptr, 1);
  game_move_players(nullptr, nullptr);
  EXPECT_EQ(game_get_grid(nullptr), nullptr);
  EXPECT_FALSE(game_is_over(nullptr));
  EXPECT_EQ(game_get_frame(nullptr), 0);
  game_set_frame(nullptr, 10);
}

TEST(GameLogicTest, ConfigLoadFromFile) {
  // Write a temporary default config YAML to a file
  const char *yaml = "gameHeight: 1000\n"
                     "gameWidth: 1000\n"
                     "gameBannerHeight: 100\n"
                     "gridHeight: 100\n"
                     "gridWidth: 100\n"
                     "maxClients: 60\n";
  std::string temp_file = create_temp_file("ccycles_config_XXXXXX");
  {
    std::ofstream out(temp_file);
    out << yaml;
  }
  GameConfig config;
  int result = game_config_load(temp_file.c_str(), &config);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(config.grid_width, 100);
  EXPECT_EQ(config.grid_height, 100);
  EXPECT_EQ(config.max_clients, 60);
}

TEST(GameLogicTest, ConfigLoadInvalidFile) {
  GameConfig config;
  int result = game_config_load("/nonexistent/file.yaml", &config);
  EXPECT_EQ(result, -1);
}

TEST(GameLogicTest, ConfigLoadNullPath) {
  GameConfig config;
  int result = game_config_load(nullptr, &config);
  EXPECT_EQ(result, -1);
}

TEST(GameLogicTest, ConfigLoadNullConfig) {
  // Write a temporary default config YAML to a file
  const char *yaml = "gameHeight: 1000\n"
                     "gameWidth: 1000\n"
                     "gameBannerHeight: 100\n"
                     "gridHeight: 100\n"
                     "gridWidth: 100\n"
                     "maxClients: 60\n";
  std::string temp_file = create_temp_file("ccycles_config_XXXXXX");
  {
    std::ofstream out(temp_file);
    out << yaml;
  }
  int result = game_config_load(temp_file.c_str(), nullptr);
  EXPECT_EQ(result, -1);
}
