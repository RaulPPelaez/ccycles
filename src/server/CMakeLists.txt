cmake_minimum_required(VERSION 3.14)

# Find required packages
find_package(Threads REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_gfx REQUIRED)
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
)

target_link_libraries(cserver_lib PUBLIC 
    Threads::Threads
    yaml
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_gfx::SDL2_gfx
    resources::rc
)

# Link math library on Unix-like systems (not needed on Windows)
if(UNIX)
  target_link_libraries(cserver_lib PUBLIC m)
endif()

# C server executable
add_executable(server server_main.c)
target_link_libraries(server PRIVATE cserver_lib)
