#pragma once
#include "api.h"
#include "configuration.hpp"
#include "game_logic.h"
#include <SFML/Main.hpp>
#include <spdlog/spdlog.h>
namespace cycles_server {
using cycles::Direction;
using cycles::Id;

// Server Logic
class GameServer {
  sf::TcpListener listener;
  std::map<Id, std::shared_ptr<sf::TcpSocket>> clientSockets;
  std::mutex serverMutex;
  std::shared_ptr<Game> game;
  const Configuration conf;
  bool running;

public:
  GameServer(std::shared_ptr<Game> game, Configuration conf);

  ~GameServer() {
    stop();
    spdlog::info("Server stopped.");
  }

  void run();

  inline void stop() { running = false; }

  inline int getFrame() const { return frame; }

  inline void setAcceptingClients(bool accepting) {
    acceptingClients = accepting;
  }

  void acceptClients();

private:
  int frame = 0;
  const int max_client_communication_time = 100; // ms

  bool acceptingClients = true;

  void checkPlayers();

  auto receiveClientInput(auto clientSockets);

  auto sendGameState(auto clientSockets);

  void gameLoop();
};

} // namespace cycles_server
