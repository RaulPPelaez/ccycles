#include "renderer.h"
#include "resources.h"
#include <SFML/Graphics.hpp>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>

using namespace cycles_server;

void PostProcess::create(sf::Vector2i windowSize) {
  if (!sf::Shader::isAvailable()) {
    spdlog::critical("Shaders are not available in this system. Please run "
                     "again without post processing enabled.");
    exit(1);
  }
  auto postProcessShaderSource =
      cycles_resources::getResourceFile("resources/shaders/postprocess.frag");
  postProcessShader.loadFromMemory(std::string(postProcessShaderSource.begin(),
                                               postProcessShaderSource.end()),
                                   sf::Shader::Fragment);
  auto bloomShaderSource =
      cycles_resources::getResourceFile("resources/shaders/bloom.frag");
  bloomShader.loadFromMemory(
      std::string(bloomShaderSource.begin(), bloomShaderSource.end()),
      sf::Shader::Fragment);
  if (!postProcessShader.isAvailable()) {
    spdlog::critical("Shader not available");
    exit(1);
  }
  if (!bloomShader.isAvailable()) {
    spdlog::critical("Bloom shader not available");
    exit(1);
  }
  renderTexture.create(windowSize.x, windowSize.y);
  renderTexture.setSmooth(true);
  channel1.create(windowSize.x, windowSize.y);
  channel1.setSmooth(true);
}

void PostProcess::apply(sf::RenderWindow &window, sf::RenderTexture &channel0) {
  auto windowSize = sf::Glsl::Vec2(window.getSize().x, window.getSize().y);
  postProcessShader.setUniform("iResolution", windowSize);
  bloomShader.setUniform("iResolution", windowSize);
  renderTexture.clear(sf::Color::Black);
  channel1.clear(sf::Color::Black);
  sf::RectangleShape bkg(windowSize);
  bkg.setFillColor(sf::Color::Black);
  renderTexture.draw(bkg);
  channel1.draw(bkg);
  postProcessShader.setUniform("iChannel0", channel0.getTexture());
  channel1.draw(sf::Sprite(channel0.getTexture()), &postProcessShader);
  channel1.display();
  bloomShader.setUniform("iChannel0", channel0.getTexture());
  bloomShader.setUniform("iChannel1", channel1.getTexture());
  window.draw(sf::Sprite(renderTexture.getTexture()), &bloomShader);
}

// Rendering Logic
GameRenderer::GameRenderer(Configuration conf)
    : window(sf::VideoMode(conf.gameWidth,
                           conf.gameHeight + conf.gameBannerHeight),
             "Cycles++"),
      conf(conf) {
  window.setFramerateLimit(60);
  try {
    auto fs = cycles_resources::getResourceFile("resources/SAIBA-45.ttf");
    font.loadFromMemory(fs.begin(), fs.size());

  } catch (const std::runtime_error &e) {
    spdlog::warn("No font loaded. Text rendering may not work correctly.");
  }
  renderTexture.create(window.getSize().x, window.getSize().y);
  if (conf.enablePostProcessing) {
    postProcess = std::make_unique<PostProcess>();
    postProcess->create(sf::Vector2i(window.getSize().x, window.getSize().y));
  }
}

void GameRenderer::render(std::shared_ptr<Game> game) {
  window.clear(sf::Color::Black);
  // // Draw grid
  // sf::RectangleShape cell(sf::Vector2f(conf.cellSize - 1, conf.cellSize -
  // 1)); cell.setFillColor(sf::Color::Black); for (int y = 0; y <
  // conf.gridHeight; ++y) {
  //   for (int x = 0; x < conf.gridWidth; ++x) {
  // 	cell.setPosition(x * conf.cellSize, y * conf.cellSize);
  // 	window.draw(cell);
  //   }
  // }
  renderPlayers(game);
  if (game->isGameOver()) {
    renderGameOver(game);
  }
  renderBanner(game);
  window.display();
}

void GameRenderer::handleEvents(
    std::vector<std::function<void(sf::Event &)>> extraEventsHandlers) {
  sf::Event event;
  while (window.pollEvent(event)) {
    if (event.type == sf::Event::Closed) {
      window.close();
    }
    if (event.type == sf::Event::KeyPressed &&
        event.key.code == sf::Keyboard::Escape) {
      window.close();
    }
    for (auto &extraEvent : extraEventsHandlers) {
      extraEvent(event);
    }
  }
}

void GameRenderer::renderPlayers(std::shared_ptr<Game> game) {
  const int offset_y = conf.gameBannerHeight + 0;
  const int offset_x = 0;
  auto cellSize = conf.cellSize;
  auto windowSize = sf::Glsl::Vec2(window.getSize().x, window.getSize().y);
  renderTexture.clear(sf::Color::Black);
  sf::RectangleShape bkg(windowSize);
  bkg.setFillColor(sf::Color::Black);
  renderTexture.draw(bkg);

  for (const auto &[id, player] : game->getPlayers()) {
    sf::CircleShape playerShape(cellSize);
    // Make the head of the player darker
    auto darkerColor = player.color;
    darkerColor.r = darkerColor.r * 0.8;
    darkerColor.g = darkerColor.g * 0.8;
    darkerColor.b = darkerColor.b * 0.8;
    playerShape.setFillColor(darkerColor);
    playerShape.setPosition(
        (player.position.x) * cellSize - cellSize / 2 + offset_x,
        (player.position.y) * cellSize - cellSize / 2 + offset_y);
    renderTexture.draw(playerShape);
    // Add a border to the head
    sf::CircleShape borderShape(cellSize + 1);
    borderShape.setFillColor(sf::Color::Transparent);
    borderShape.setOutlineThickness(3);
    borderShape.setOutlineColor(player.color);
    borderShape.setPosition(
        (player.position.x) * cellSize - cellSize / 2 - 1 + offset_x,
        (player.position.y) * cellSize - cellSize / 2 - 1 + offset_y);
    renderTexture.draw(borderShape);
    // Draw tail
    for (auto tail : player.tail) {
      sf::RectangleShape tailShape(sf::Vector2f(cellSize, cellSize));
      tailShape.setFillColor(player.color);
      tailShape.setPosition(tail.x * cellSize + offset_x,
                            tail.y * cellSize + offset_y);
      renderTexture.draw(tailShape);
    }
  }
  renderTexture.display();
  if (postProcess)
    postProcess->apply(window, renderTexture);
  else
    window.draw(sf::Sprite(renderTexture.getTexture()));
  for (const auto &[id, player] : game->getPlayers()) {
    sf::Text nameText(player.name, font, 15);
    nameText.setFillColor(sf::Color::White);
    nameText.setOutlineThickness(2);
    nameText.setOutlineColor(sf::Color::Black);
    nameText.setPosition(player.position.x * cellSize - 20 + offset_x,
                         player.position.y * cellSize - 20 + offset_y);
    window.draw(nameText);
  }
}

void GameRenderer::renderGameOver(std::shared_ptr<Game> game) {
  sf::Text gameOverText("Game Over", font, 60);
  gameOverText.setOutlineThickness(3);
  gameOverText.setOutlineColor(sf::Color::White);
  gameOverText.setFillColor(sf::Color::Black);
  gameOverText.setPosition(conf.gameWidth / 2 - 150, conf.gameHeight / 2 - 30);
  if (game->getPlayers().size() > 0) {
    auto winner = game->getPlayers().begin()->second.name;
    sf::Text winnerText("Winner: " + winner, font, 40);
    winnerText.setFillColor(sf::Color::Black);
    winnerText.setOutlineThickness(3);
    winnerText.setOutlineColor(sf::Color::White);
    winnerText.setPosition(conf.gameWidth / 2 - 150, conf.gameHeight / 2 + 30);
    window.draw(winnerText);
  }
  window.draw(gameOverText);
}

void GameRenderer::renderBanner(std::shared_ptr<Game> game) {
  // Draw a banner at the top
  sf::RectangleShape banner(
      sf::Vector2f(conf.gameWidth, conf.gameBannerHeight - 20));
  banner.setFillColor(sf::Color::Black);
  banner.setPosition(0, 0);
  window.draw(banner);
  // Draw the frame number
  sf::Text frameText("Frame: " + std::to_string(game->getFrame()), font, 22);
  frameText.setPosition(10, 10);
  frameText.setFillColor(sf::Color::White);
  window.draw(frameText);
  // Draw the number of players
  sf::Text playersText("Players: " + std::to_string(game->getPlayers().size()),
                       font, 22);
  playersText.setPosition(10, 40);
  playersText.setFillColor(sf::Color::White);
  window.draw(playersText);
}

void GameRenderer::renderSplashScreen(std::shared_ptr<Game> game) {
  window.clear(sf::Color::Black);
  renderPlayers(game);
  renderBanner(game);
  sf::Text splashText("Waiting for players\npress SPACE to start", font, 30);
  splashText.setFillColor(sf::Color::Black);
  splashText.setOutlineThickness(2);
  splashText.setOutlineColor(sf::Color::White);
  splashText.setPosition(conf.gameWidth / 2 - 150, conf.gameHeight / 2 - 30);
  window.draw(splashText);
  window.display();
}
