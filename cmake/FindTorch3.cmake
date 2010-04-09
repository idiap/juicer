#
# Find Torch3
#
# Phil Garner
# March 2009
#
# At Idiap, Torch uses pkgconfig.  However, the distribution doesn't.
#
include(FindPkgConfig)

set(TORCH3_DIR $ENV{TORCH3_DIR}
  CACHE FILEPATH "Path to Torch3 directory"
  )

pkg_check_modules(TORCH3 torch)
#if(TORCH3_VERSION GREATER 3 AND TORCH3_VERSION LESS 4)
#  message("And the version is OK: ${TORCH3_VERSION}")
#endif(TORCH3_VERSION GREATER 3 AND TORCH3_VERSION LESS 4)

if (NOT TORCH3_FOUND)
  if (EXISTS "${TORCH3_DIR}")
    message(STATUS "Using torch3 dir: ${TORCH3_DIR}")
    set(TORCH3_FOUND 1)
    set(TORCH3_INCLUDE_DIRS
      ${TORCH3_DIR}/core
      ${TORCH3_DIR}/speech
      ${TORCH3_DIR}/datasets
      ${TORCH3_DIR}/distributions
      ${TORCH3_DIR}/gradients
      )
    set(TORCH3_LIBRARY_DIRS
      ${TORCH3_DIR}/libs/${CMAKE_SYSTEM_NAME}_opt_float
      )
    set(TORCH3_LIBRARIES
      torch
      )
  endif (EXISTS "${TORCH3_DIR}")
endif (NOT TORCH3_FOUND)
