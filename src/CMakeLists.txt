# Fix a bug in fmt that prevents it from compiling in Windows
set(FMT_UNICODE OFF CACHE BOOL "Enable Unicode support" FORCE)
find_package(spdlog REQUIRED)
find_package(SFML 2.6 COMPONENTS graphics network system REQUIRED)

link_libraries(spdlog::spdlog ulog)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_library(utils utils.cpp)
link_libraries(utils)

add_library(api api.cpp)
target_link_libraries(api PUBLIC sfml-graphics sfml-network sfml-system)

add_library(c_api c_api.c)

add_executable(client client/client_randomio.cpp)
target_link_libraries(client api)
add_subdirectory(server)
add_subdirectory(cserver)

add_executable(client_c_simple client/client_c_simple.c)
target_link_libraries(client_c_simple c_api)
