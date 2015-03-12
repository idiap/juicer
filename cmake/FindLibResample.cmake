#
# Find LibResample
#
set(LIBRESAMPLE_DIR $ENV{LIBRESAMPLE_DIR}
  CACHE FILEPATH "Path to libresample directory"
  )

if (EXISTS "${LIBRESAMPLE_DIR}")
  message(STATUS "Using libresample dir: ${LIBRESAMPLE_DIR}")
  set(LIBRESAMPLE_FOUND 1)
  set(LIBRESAMPLE_INCLUDE_DIRS
    ${LIBRESAMPLE_DIR}/src
    ${LIBRESAMPLE_DIR}/include
    )
  set(LIBRESAMPLE_LIBRARIES
    ${LIBRESAMPLE_DIR}/libresample.a
    )
endif (EXISTS "${LIBRESAMPLE_DIR}")

if (EXISTS "/usr/lib/libresample.a")
  set(LIBRESAMPLE_FOUND 1)
  set(LIBRESAMPLE_INCLUDE_DIRS /usr/include)
  set(LIBRESAMPLE_LIBRARIES /usr/lib/libresample.a)
  message(STATUS "Using libresample from system")
endif (EXISTS "/usr/lib/libresample.a")
  

if (NOT LIBRESAMPLE_FOUND)
  message(STATUS "libresample not found")
  set(LIBRESAMPLE_FOUND 0)
endif (NOT LIBRESAMPLE_FOUND)
