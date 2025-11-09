link_libraries(ulog)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_subdirectory(server)

add_library(c_api c_api.c)
add_executable(client_c_simple client/client_c_simple.c)
target_link_libraries(client_c_simple c_api)

if(UNIX)
  target_link_libraries(client_c_simple m)
endif()
