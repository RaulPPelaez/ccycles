#pragma once
#include "api.h"
#include <SFML/Graphics.hpp>
#include <list>
#include <string>
namespace cycles_server {
struct Player {
  sf::Vector2i position;
  std::list<sf::Vector2i> tail;
  sf::Color color;
  std::string name;
  cycles::Id id;
  Player() : id(std::rand()) {}
};

} // namespace cycles_server
