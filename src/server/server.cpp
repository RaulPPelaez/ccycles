#include "server.h"
#include "game_logic.h"
#include "renderer.h"
#include <SFML/Network.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>



using namespace cycles_server;

int main(int argc, char *argv[]) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::debug);
#endif
  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  const std::string config_path = argc > 1 ? argv[1] : "config.yaml";
  const Configuration conf(config_path);
  auto game = std::make_shared<Game>(conf);
  GameServer server(game, conf);
  GameRenderer renderer(conf);
  std::thread acceptThread(&GameServer::acceptClients, &server);
  bool acceptingClients = true;
  auto spaceEvent = [&acceptingClients](auto &event) {
    if (event.type == sf::Event::KeyPressed &&
        event.key.code == sf::Keyboard::Space) {
      spdlog::info("Space pressed, stopping client acceptance");
      acceptingClients = false;
    }
  };
  while (acceptingClients && renderer.isOpen()) {
    renderer.handleEvents({spaceEvent});
    renderer.renderSplashScreen(game);
  }
  server.setAcceptingClients(false);
  acceptThread.join();
  std::thread serverThread(&GameServer::run, &server);
  while (renderer.isOpen()) {
    renderer.handleEvents();
    renderer.render(game);
  }
  server.stop();
  serverThread.join();
  return 0;
}
