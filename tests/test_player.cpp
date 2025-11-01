#include <gtest/gtest.h>

extern "C" {
#include "server/player.h"
#include "server/types.h"
}

TEST(PlayerTest, CreatePlayer) {
  Vec2i pos = {10, 20};
  Rgb color = {255, 0, 0};
  Player p;

  int result = player_create(1, "TestPlayer", pos, color, &p);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(p.id, 1);
  EXPECT_STREQ(p.name, "TestPlayer");
  EXPECT_EQ(p.position.x, 10);
  EXPECT_EQ(p.position.y, 20);
  EXPECT_EQ(p.color.r, 255);
  EXPECT_EQ(p.color.g, 0);
  EXPECT_EQ(p.color.b, 0);
  EXPECT_EQ(p.tail_linked_list, nullptr);
}

TEST(PlayerTest, CreatePlayerWithLongName) {
  Vec2i pos = {5, 15};
  Rgb color = {0, 255, 0};
  Player p;

  char long_name[100];
  memset(long_name, 'A', sizeof(long_name) - 1);
  long_name[sizeof(long_name) - 1] = '\0';

  int result = player_create(2, long_name, pos, color, &p);

  EXPECT_EQ(result, 0);
  EXPECT_EQ(p.id, 2);
  EXPECT_EQ(strlen(p.name), MAX_PLAYER_NAME_LEN - 1);
  EXPECT_EQ(p.name[MAX_PLAYER_NAME_LEN - 1], '\0');
}

TEST(PlayerTest, CreatePlayerWithNullName) {
  Vec2i pos = {0, 0};
  Rgb color = {0, 0, 255};
  Player p;

  int result = player_create(3, nullptr, pos, color, &p);

  EXPECT_EQ(result, -1);
}

TEST(PlayerTest, CreatePlayerWithNullOutput) {
  Vec2i pos = {0, 0};
  Rgb color = {0, 0, 255};

  int result = player_create(3, "Player", pos, color, nullptr);

  EXPECT_EQ(result, -1);
}

TEST(PlayerTest, DestroyPlayerWithoutTail) {
  Vec2i pos = {1, 2};
  Rgb color = {100, 100, 100};
  Player p;

  player_create(4, "Player4", pos, color, &p);
  player_destroy(&p);

  EXPECT_EQ(p.id, 0);
  EXPECT_EQ(p.name[0], '\0');
  EXPECT_EQ(p.position.x, 0);
  EXPECT_EQ(p.position.y, 0);
  EXPECT_EQ(p.tail_linked_list, nullptr);
}

TEST(PlayerTest, DestroyPlayerWithTail) {
  Vec2i pos = {10, 10};
  Rgb color = {50, 50, 50};
  Player p;

  player_create(5, "Player5", pos, color, &p);

  TailNode *node1 = (TailNode *)malloc(sizeof(TailNode));
  node1->position = {9, 10};
  node1->next = nullptr;

  TailNode *node2 = (TailNode *)malloc(sizeof(TailNode));
  node2->position = {8, 10};
  node2->next = node1;

  p.tail_linked_list = node2;

  player_destroy(&p);

  EXPECT_EQ(p.id, 0);
  EXPECT_EQ(p.tail_linked_list, nullptr);
}

TEST(PlayerTest, DestroyNullPlayer) { player_destroy(nullptr); }
