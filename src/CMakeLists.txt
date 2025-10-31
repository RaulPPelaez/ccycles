link_libraries(ulog)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

add_library(c_api c_api.c)

add_executable(client client/client_c_simple.c)
target_link_libraries(client api)
add_subdirectory(server)

add_executable(client_c_simple client/client_c_simple.c)
target_link_libraries(client_c_simple c_api)
