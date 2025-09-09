include(FetchContent)

set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

add_library(configuration configuration.cpp configuration.hpp)
target_link_libraries(configuration PUBLIC yaml-cpp::yaml-cpp)

add_library(game_logic game_logic.cpp game_logic.h)
add_library(renderer renderer.cpp)
target_link_libraries(renderer PRIVATE resources::rc)

add_library(server_backend server_backend.cpp)
target_link_libraries(server_backend PUBLIC game_logic configuration)


add_executable(server server.cpp)
target_link_libraries(server PUBLIC renderer server_backend)
