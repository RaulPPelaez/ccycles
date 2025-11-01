#ifndef RESOURCE_LOADER_HPP
#define RESOURCE_LOADER_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Load a font from embedded resources into SDL_ttf format
 * @param resource_path Path to the resource (e.g., "resources/SAIBA-45.ttf")
 * @param font_size Font size in points
 * @return Opaque pointer to TTF_Font, or NULL on failure
 */
void *resource_load_font_from_memory(const char *resource_path, int font_size);

/**
 * @brief Get embedded resource data
 * @param resource_path Path to the resource
 * @param out_size Output parameter for data size
 * @return Pointer to resource data, or NULL if not found
 */
const uint8_t *resource_get_data(const char *resource_path, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // RESOURCE_LOADER_HPP
