#include "game_logic.h"
#include "server.h"
#include <SFML/Network.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

using namespace cycles_server;

GameServer::GameServer(std::shared_ptr<Game> game, Configuration conf)
    : game(game), conf(conf), running(false) {
  const char *portenv = std::getenv("CYCLES_PORT");
  if (portenv == nullptr) {
    spdlog::critical("Please set the CYCLES_PORT environment variable");
    exit(1);
  }
  spdlog::info("Listening on port {}", portenv);
  const unsigned short PORT = std::stoi(portenv);
  listener.listen(PORT);
  listener.setBlocking(false);
  if (listener.getLocalPort() == 0) {
    spdlog::critical("Failed to bind to port {}", PORT);
    exit(1);
  }
}

void GameServer::run() {
  running = true;
  std::thread gameLoopThread(&GameServer::gameLoop, this);
  gameLoopThread.join();
}

void GameServer::acceptClients() {
  while (acceptingClients &&
         static_cast<int>(clientSockets.size()) < conf.maxClients) {
    auto clientSocket = std::make_shared<sf::TcpSocket>();
    if (listener.accept(*clientSocket) == sf::Socket::Done) {
      spdlog::debug("New connection from {}",
		    clientSocket->getRemoteAddress().toString());
      // Set to blocking for initial communication
      clientSocket->setBlocking(true);
      // Receive player name
      sf::Packet namePacket;
      if (clientSocket->receive(namePacket) == sf::Socket::Done) {
	spdlog::debug("Received name packet from client");
        std::string playerName;
        namePacket >> playerName;
	spdlog::info("Received player name: {}", playerName);
        auto id = game->addPlayer(playerName);
        // Send color to the client
        sf::Packet colorPacket;
        const auto player = game->getPlayers().at(id);
        colorPacket << player.color.r << player.color.g << player.color.b;
        if (clientSocket->send(colorPacket) != sf::Socket::Done) {
          spdlog::critical("Failed to send color to client: {}", playerName);
        } else {
          spdlog::info("Color sent to client: {}", playerName);
        }
        clientSocket->setBlocking(
            false); // Set back to non-blocking for game loop
        clientSockets[id] = clientSocket;
        spdlog::info("New client connected: {} with id {}", playerName, id);
      }
      else {
	spdlog::debug("Failed to receive name from client");
      }
    }
  }
}

void GameServer::checkPlayers() {
  // Remove sockets from players that have died or disconnected
  spdlog::debug("Server ({}): Checking players", frame);
  auto players = game->getPlayers();
  std::vector<Id> toRemove;
  toRemove.reserve(clientSockets.size());
  for (const auto &kv : clientSockets) {
    const auto id = kv.first;
    const auto &socket = kv.second;
    bool remove = false;
    if (players.find(id) == players.end()) {
      spdlog::info("Player {} has died", id);
      remove = true;
    }
    if (socket->getRemoteAddress() == sf::IpAddress::None) {
      spdlog::info("Player {} has disconnected", id);
      remove = true;
    }
    if (remove) {
      toRemove.push_back(id);
    }
  }
  for (auto id : toRemove) {
    game->removePlayer(id);
    clientSockets.erase(id);
  }
}

auto GameServer::receiveClientInput(auto clientSockets) {
  spdlog::debug("Server ({}): Receiving client input from {} clients", frame,
                clientSockets.size());
  if (clientSockets.size() == 0) {
    return std::map<Id, Direction>();
  }
  std::map<Id, Direction> successful;
  for (const auto &[id, clientSocket] : clientSockets) {
    auto name = game->getPlayers().at(id).name;
    spdlog::debug("Server ({}): Receiving input from player {} ({})", frame, id,
                  name);
    sf::Packet packet;
    auto status = clientSocket->receive(packet);
    if (status == sf::Socket::Done) {
      int direction;
      packet >> direction;
      spdlog::debug("Received direction {} from player {} ({})", direction, id,
                    name);
      successful[id] = static_cast<Direction>(direction);
    }
  }
  return successful;
}

auto GameServer::sendGameState(auto clientSockets) {
  spdlog::debug("Server ({}): Sending game state to {} clients", frame,
                clientSockets.size());
  if (clientSockets.size() == 0) {
    return std::vector<Id>();
  }
  sf::Packet packet;
  packet << conf.gridWidth << conf.gridHeight;
  const auto &grid = game->getGrid();
  auto players = game->getPlayers();
  packet << static_cast<sf::Uint32>(players.size());
  for (const auto &[id, player] : players) {
    packet << player.position.x << player.position.y << player.color.r
           << player.color.g << player.color.b << player.name << id << frame;
  }
  for (auto &cell : grid) {
    packet << cell;
  }
  std::vector<Id> successful;
  for (const auto &[id, clientSocket] : clientSockets) {
    if (clientSocket->send(packet) != sf::Socket::Done) {
      spdlog::debug("Server ({}): Failed to send game state to player {}",
                    frame, id);
    } else {
      successful.push_back(id);
      spdlog::debug("Server ({}): Game state sent to player {}", frame, id);
    }
  }
  return successful;
}

void GameServer::gameLoop() {
  sf::Clock clock;
  sf::Clock clientCommunicationClock;
  while (running && !game->isGameOver()) {
    if (clock.getElapsedTime().asMilliseconds() >= 33) { // ~30 fps
      clock.restart();
      std::scoped_lock lock(serverMutex);
      game->setFrame(frame);
      checkPlayers();
      auto clientsUnsent = clientSockets;
      decltype(clientSockets) toRecieve;
      std::map<Id, Direction> newDirs;
      std::set<Id> timedOutPlayers;
      clientCommunicationClock.restart();
      while (clientsUnsent.size() > 0 || toRecieve.size() > 0) {
        auto successful = sendGameState(clientsUnsent);
        for (auto s : successful) {
          clientsUnsent.erase(s);
          toRecieve[s] = clientSockets[s];
        }
        auto succesfulrec = receiveClientInput(toRecieve);
        for (auto s : succesfulrec) {
          toRecieve.erase(s.first);
          newDirs[s.first] = s.second;
        }
        spdlog::debug("Server ({}): Clients unsent: {}", frame,
                      clientsUnsent.size());
        spdlog::debug("Server ({}): Clients to recieve: {}", frame,
                      toRecieve.size());
        // Check for clients that have not sent input for a long time
        if (clientCommunicationClock.getElapsedTime().asMilliseconds() >
            max_client_communication_time) {
          // Mark all remaining clients for removal
          for (auto [id, socket] : clientsUnsent) {
            timedOutPlayers.insert(id);
          }
          for (auto [id, socket] : toRecieve) {
            timedOutPlayers.insert(id);
          }
          break;
        }
      }
      for (auto id : timedOutPlayers) {
        spdlog::info(
            "Server ({}): Client {} has not sent input for a long time", frame,
            id);
        game->removePlayer(id);
        clientSockets.erase(id);
        newDirs.erase(id);
      }
      game->movePlayers(newDirs);
      frame++;
    }
  }
}
