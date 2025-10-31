#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fill a GameConfig with default values used by the server.
 * @param config Output configuration struct to populate.
 */
void fill_default_configuration(GameConfig *config);

/**
 * @brief Convert direction to movement vector
 */
Vec2i direction_to_vector(Direction dir);

#ifdef __cplusplus
}
#endif
