#
# Find SPTK
#
find_path(SPTK_INCLUDE_DIR SPTK.h)
set(SPTK_NAMES ${SPTK_NAMES} SPTK)
find_library(SPTK_LIBRARY NAMES ${SPTK_NAMES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  SPTK DEFAULT_MSG SPTK_LIBRARY SPTK_INCLUDE_DIR
  )

if(SPTK_FOUND)
  set(SPTK_INCLUDE_DIRS ${SPTK_INCLUDE_DIR})
  set(SPTK_LIBRARIES ${SPTK_LIBRARY})
endif(SPTK_FOUND)
