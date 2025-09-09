# Fix a bug in fmt that prevents it from compiling in Windows
set(FMT_UNICODE OFF CACHE BOOL "Enable Unicode support" FORCE)
find_package(spdlog REQUIRED)
find_package(SFML 2.6 COMPONENTS graphics window system network REQUIRED)

link_libraries(spdlog::spdlog ulog)
link_libraries(sfml-graphics sfml-window sfml-system sfml-network pthread)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_library(utils OBJECT utils.cpp)
link_libraries(utils)
add_library(api OBJECT api.cpp)
link_libraries(api)

add_executable(client client/client_randomio.cpp)
add_subdirectory(server)

add_executable(client_c_simple client/client_c_simple.c)
