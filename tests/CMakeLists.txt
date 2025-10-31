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

add_executable(test_game_logic  test_game_logic.cpp)
target_include_directories(test_game_logic PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(
  test_game_logic
  GTest::gtest_main
  game_logic
  configuration
)
gtest_discover_tests(test_game_logic)

add_executable(test_c_api test_c_api.cpp)
target_include_directories(test_c_api PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(
  test_c_api
  GTest::gtest_main
  c_api
  server_backend
  game_logic
  configuration
)
gtest_discover_tests(test_c_api)
