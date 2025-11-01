#include "resource_loader.hpp"
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_ttf.h>
#include <cmrc/cmrc.hpp>
#include <cstring>

CMRC_DECLARE(cycles_resources);

extern "C" {

const uint8_t *resource_get_data(const char *resource_path, size_t *out_size) {
  if (!resource_path || !out_size) {
    return nullptr;
  }
  try {
    auto fs = cmrc::cycles_resources::get_filesystem();
    auto file = fs.open(resource_path);

    *out_size = file.size();
    return reinterpret_cast<const uint8_t *>(file.begin());
  } catch (const std::exception &e) {
    *out_size = 0;
    return nullptr;
  }
}

void *resource_load_font_from_memory(const char *resource_path, int font_size) {
  if (!resource_path) {
    return nullptr;
  }
  try {
    auto fs = cmrc::cycles_resources::get_filesystem();
    auto file = fs.open(resource_path);

    // Create SDL_RWops from memory
    SDL_RWops *rw = SDL_RWFromConstMem(file.begin(), file.size());
    if (!rw) {
      return nullptr;
    }
    // Load font from RWops (SDL_RWops will be automatically freed)
    TTF_Font *font = TTF_OpenFontRW(rw, 1, font_size);
    return static_cast<void *>(font);

  } catch (const std::exception &e) {
    return nullptr;
  }
}

} // extern "C"
