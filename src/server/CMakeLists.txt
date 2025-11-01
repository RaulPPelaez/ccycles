cmake_minimum_required(VERSION 3.14)


find_package(SDL2_ttf REQUIRED)
find_package(SDL2 REQUIRED)
include(FindPkgConfig)
pkg_check_modules(SDL2_GFX REQUIRED SDL2_gfx)
# C server library (with C++ resource loader)
add_library(cserver_lib
    player.c
    player_map.c
    game_logic.c
    server.c
    server_utils.c
    renderer.c
    resource_loader.cpp
)

target_include_directories(cserver_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_GFX_INCLUDE_DIRS}
)

target_link_directories(cserver_lib PUBLIC
    ${SDL2_GFX_LIBRARY_DIRS}
)

target_link_libraries(cserver_lib PUBLIC 
    pthread 
    m 
    yaml
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    ${SDL2_GFX_LIBRARIES}
    resources::rc
)

# C server executable
add_executable(server server_main.c)
target_link_libraries(server PRIVATE cserver_lib)
