cmake_minimum_required(VERSION 3.20)
project(CCycles VERSION 1.0.0 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Fetch libyaml v0.2.5 and allow older policies
include(FetchContent)
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

set(CMAKE_C_STANDARD 23)
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
add_subdirectory(src)
enable_testing() # This line allows to call ctest after compilation
add_subdirectory(tests)
add_subdirectory(docs)
