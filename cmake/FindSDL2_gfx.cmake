# FindSDL2_gfx.cmake
# Locate SDL2_gfx library
# This module defines:
#  SDL2_gfx_FOUND - System has SDL2_gfx
#  SDL2_gfx_INCLUDE_DIRS - SDL2_gfx include directories
#  SDL2_gfx_LIBRARIES - Libraries needed to use SDL2_gfx
#  SDL2_gfx::SDL2_gfx - Imported target for SDL2_gfx

# Try pkg-config first (works well on Linux/macOS)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SDL2_GFX QUIET SDL2_gfx)
endif()

# Find the include directory
find_path(SDL2_gfx_INCLUDE_DIR
  NAMES SDL2_gfxPrimitives.h
  HINTS
    ${PC_SDL2_GFX_INCLUDEDIR}
    ${PC_SDL2_GFX_INCLUDE_DIRS}
    $ENV{SDL2_GFX_DIR}
    $ENV{CONDA_PREFIX}
  PATH_SUFFIXES 
    include/SDL2
    include
    SDL2
)

# Find the library
find_library(SDL2_gfx_LIBRARY
  NAMES SDL2_gfx SDL2_gfx-1.0
  HINTS
    ${PC_SDL2_GFX_LIBDIR}
    ${PC_SDL2_GFX_LIBRARY_DIRS}
    $ENV{SDL2_GFX_DIR}
    $ENV{CONDA_PREFIX}
  PATH_SUFFIXES 
    lib
    lib64
    Library/lib
)

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_gfx
  REQUIRED_VARS SDL2_gfx_LIBRARY SDL2_gfx_INCLUDE_DIR
  VERSION_VAR PC_SDL2_GFX_VERSION
)

if(SDL2_gfx_FOUND)
  set(SDL2_gfx_LIBRARIES ${SDL2_gfx_LIBRARY})
  set(SDL2_gfx_INCLUDE_DIRS ${SDL2_gfx_INCLUDE_DIR})

  # Create imported target
  if(NOT TARGET SDL2_gfx::SDL2_gfx)
    add_library(SDL2_gfx::SDL2_gfx UNKNOWN IMPORTED)
    set_target_properties(SDL2_gfx::SDL2_gfx PROPERTIES
      IMPORTED_LOCATION "${SDL2_gfx_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SDL2_gfx_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(SDL2_gfx_INCLUDE_DIR SDL2_gfx_LIBRARY)
