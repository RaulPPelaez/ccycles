include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Find Threads package for cross-platform thread support
find_package(Threads REQUIRED)

add_executable(test_c_api test_c_api.cpp)
target_include_directories(test_c_api PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(
  test_c_api
  GTest::gtest_main
  c_api
  cserver_lib
  Threads::Threads
)
gtest_discover_tests(test_c_api)

# C server tests
add_executable(test_player test_player.cpp)
target_include_directories(test_player PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
  test_player
  GTest::gtest_main
  cserver_lib
)
gtest_discover_tests(test_player)

add_executable(test_player_map test_player_map.cpp)
target_include_directories(test_player_map PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
  test_player_map
  GTest::gtest_main
  cserver_lib
)
gtest_discover_tests(test_player_map)

add_executable(test_c_game_logic test_c_game_logic.cpp)
target_include_directories(test_c_game_logic PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
  test_c_game_logic
  GTest::gtest_main
  cserver_lib
)
gtest_discover_tests(test_c_game_logic)
