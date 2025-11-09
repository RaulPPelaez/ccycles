cmake_minimum_required(VERSION 3.20)
project(CCycles VERSION 1.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Fetch libyaml v0.2.5 and allow older policies
include(FetchContent)

# Disable libyaml tests and examples
set(BUILD_TESTING_LIBYAML OFF CACHE BOOL "Disable libyaml tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

FetchContent_Declare(
  libyaml
  GIT_REPOSITORY https://github.com/yaml/libyaml.git
  GIT_TAG 0.2.5
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for dependencies")
FetchContent_MakeAvailable(libyaml)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


include(CMakeRC)
# Add everything under root_of_project/resources to the resources target
file(GLOB_RECURSE RESOURCES RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_SOURCE_DIR}/resources/*")
list(FILTER RESOURCES EXCLUDE REGEX ".*~")
#List files
message(STATUS "${CMAKE_SOURCE_DIR}")
foreach(RESOURCE ${RESOURCES})
  message(STATUS "Found resource: ${RESOURCE}")
endforeach()
cmrc_add_resource_library(resources ALIAS resources::rc NAMESPACE cycles_resources ${RESOURCES})

add_subdirectory(third_party)
# Set default ulog verbosity: INFO for non-Debug, TRACE for Debug builds
# This defines DEFAULT_ULOG_LEVEL used by the code to configure ulog at runtime
add_compile_definitions(
  DEFAULT_ULOG_LEVEL=$<IF:$<CONFIG:Debug>,ULOG_LEVEL_TRACE,ULOG_LEVEL_INFO>
)
# Make __FILE__ macro use relative paths instead of absolute paths
# This makes log messages show "src/file.c" instead of "/full/path/to/src/file.c"
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)
endif()

add_subdirectory(src)
enable_testing() # This line allows to call ctest after compilation
add_subdirectory(tests)
add_subdirectory(docs)
