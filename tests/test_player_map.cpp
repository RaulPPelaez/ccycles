#include <gtest/gtest.h>

extern "C" {
#include "server/player.h"
#include "server/player_map.h"
#include "server/types.h"
}

static Player createTestPlayer(PlayerId id, const char *name, int x, int y) {
  Vec2i pos = {x, y};
  Rgb color = {(uint8_t)id, (uint8_t)(id * 2), (uint8_t)(id * 3)};
  Player p;
  player_create(id, name, pos, color, &p);
  return p;
}

TEST(PlayerMapTest, CreateMap) {
  PlayerMap *map = map_create();
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map_size(map), 0);
  map_destroy(map);
}

TEST(PlayerMapTest, InsertSinglePlayer) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(1, "Player1", 10, 20);

  EXPECT_EQ(map_insert(map, 1, &p), 0);
  EXPECT_EQ(map_size(map), 1);
  map_destroy(map);
}

TEST(PlayerMapTest, InsertMultiplePlayers) {
  PlayerMap *map = map_create();
  Player p1 = createTestPlayer(1, "Player1", 10, 20);
  Player p2 = createTestPlayer(5, "Player2", 30, 40);
  Player p3 = createTestPlayer(255, "Player3", 50, 60);

  EXPECT_EQ(map_insert(map, 1, &p1), 0);
  EXPECT_EQ(map_insert(map, 5, &p2), 0);
  EXPECT_EQ(map_insert(map, 255, &p3), 0);

  EXPECT_EQ(map_size(map), 3);
  map_destroy(map);
}

TEST(PlayerMapTest, FindExistingPlayer) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(42, "Player42", 100, 200);
  map_insert(map, 42, &p);

  Player *found = map_find(map, 42);

  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->id, 42);
  EXPECT_STREQ(found->name, "Player42");
  EXPECT_EQ(found->position.x, 100);
  EXPECT_EQ(found->position.y, 200);
  map_destroy(map);
}

TEST(PlayerMapTest, FindNonExistingPlayer) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(10, "Player10", 5, 5);
  map_insert(map, 10, &p);

  EXPECT_EQ(map_find(map, 20), nullptr);
  map_destroy(map);
}

TEST(PlayerMapTest, InsertDuplicateKey) {
  PlayerMap *map = map_create();
  Player p1 = createTestPlayer(7, "Player7", 10, 10);
  EXPECT_EQ(map_insert(map, 7, &p1), 0);

  Player p2 = createTestPlayer(7, "UpdatedPlayer7", 50, 50);
  EXPECT_EQ(map_insert(map, 7, &p2), -1);

  EXPECT_EQ(map_size(map), 1);

  Player *found = map_find(map, 7);
  ASSERT_NE(found, nullptr);
  EXPECT_STREQ(found->name, "Player7");
  EXPECT_EQ(found->position.x, 10);
  map_destroy(map);
}

TEST(PlayerMapTest, DeleteExistingPlayer) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(15, "Player15", 25, 35);
  map_insert(map, 15, &p);

  EXPECT_EQ(map_size(map), 1);
  map_delete(map, 15);

  EXPECT_EQ(map_size(map), 0);
  EXPECT_EQ(map_find(map, 15), nullptr);
  map_destroy(map);
}

TEST(PlayerMapTest, DeleteNonExistingPlayer) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(8, "Player8", 1, 2);
  map_insert(map, 8, &p);

  map_delete(map, 99);
  EXPECT_EQ(map_size(map), 1);
  map_destroy(map);
}

TEST(PlayerMapTest, DeletePlayerWithTail) {
  PlayerMap *map = map_create();
  Player p = createTestPlayer(20, "Player20", 10, 10);

  TailNode *node1 = (TailNode *)malloc(sizeof(TailNode));
  node1->position = {9, 10};
  node1->next = nullptr;

  TailNode *node2 = (TailNode *)malloc(sizeof(TailNode));
  node2->position = {8, 10};
  node2->next = node1;

  p.tail_linked_list = node2;
  map_insert(map, 20, &p);
  map_delete(map, 20);

  EXPECT_EQ(map_size(map), 0);
  map_destroy(map);
}

TEST(PlayerMapTest, GetAllPlayers) {
  PlayerMap *map = map_create();
  Player p1 = createTestPlayer(1, "P1", 1, 1);
  Player p2 = createTestPlayer(2, "P2", 2, 2);
  Player p3 = createTestPlayer(3, "P3", 3, 3);

  map_insert(map, 1, &p1);
  map_insert(map, 2, &p2);
  map_insert(map, 3, &p3);

  Player *players[256];
  uint32_t count = map_get_all(map, players);

  EXPECT_EQ(count, 3);

  bool found_p1 = false, found_p2 = false, found_p3 = false;
  for (uint32_t i = 0; i < count; i++) {
    if (players[i]->id == 1)
      found_p1 = true;
    if (players[i]->id == 2)
      found_p2 = true;
    if (players[i]->id == 3)
      found_p3 = true;
  }

  EXPECT_TRUE(found_p1);
  EXPECT_TRUE(found_p2);
  EXPECT_TRUE(found_p3);
  map_destroy(map);
}

TEST(PlayerMapTest, DeleteMultiplePlayers) {
  PlayerMap *map = map_create();
  Player p1 = createTestPlayer(1, "P1", 1, 1);
  Player p2 = createTestPlayer(2, "P2", 2, 2);

  map_insert(map, 1, &p1);
  map_insert(map, 2, &p2);
  EXPECT_EQ(map_size(map), 2);

  map_delete(map, 1);
  map_delete(map, 2);

  EXPECT_EQ(map_size(map), 0);
  EXPECT_EQ(map_find(map, 1), nullptr);
  EXPECT_EQ(map_find(map, 2), nullptr);
  map_destroy(map);
}

TEST(PlayerMapTest, BoundaryKeys) {
  PlayerMap *map = map_create();
  Player p0 = createTestPlayer(0, "P0", 0, 0);
  Player p255 = createTestPlayer(255, "P255", 255, 255);

  map_insert(map, 0, &p0);
  map_insert(map, 255, &p255);

  EXPECT_EQ(map_size(map), 2);

  Player *found0 = map_find(map, 0);
  Player *found255 = map_find(map, 255);

  ASSERT_NE(found0, nullptr);
  ASSERT_NE(found255, nullptr);
  EXPECT_EQ(found0->id, 0);
  EXPECT_EQ(found255->id, 255);
  map_destroy(map);
}

TEST(PlayerMapTest, NullMapOperations) {
  Player p = createTestPlayer(1, "P1", 1, 1);

  EXPECT_EQ(map_insert(nullptr, 1, &p), -1);
  EXPECT_EQ(map_find(nullptr, 1), nullptr);
  map_delete(nullptr, 1);
  EXPECT_EQ(map_size(nullptr), 0);

  Player *players[256];
  EXPECT_EQ(map_get_all(nullptr, players), 0);

  map_destroy(nullptr);
}
