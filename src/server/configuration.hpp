#pragma once

#include <string>
namespace cycles_server {

struct Configuration {

  int maxClients = 60;
  int gridWidth = 100;
  int gridHeight = 100;
  int gameWidth = 1000;
  int gameHeight = 1000;
  int gameBannerHeight = 100;
  float cellSize = 10;
  bool enablePostProcessing = false;
  Configuration(std::string configPath);
};

} // namespace cycles_server
