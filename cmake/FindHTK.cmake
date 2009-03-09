#
# Find HTK
#
set(HTK_DIR $ENV{HTK_DIR}
  CACHE FILEPATH "Path to HTK directory"
  )

if (EXISTS "${HTK_DIR}")
  message(STATUS "Using HTK dir: ${HTK_DIR}")
  set(HTK_FOUND 1)
  set(HTK_INCLUDE_DIRS
    ${HTK_DIR}/HTKLib
    )
  set(HTK_LIBRARIES
    ${HTK_DIR}/HTKLib/HTKLib.a
    )
else (EXISTS "${HTK_DIR}")
  message(STATUS "HTK not found")
  set(HTK_FOUND 0)
endif (EXISTS "${HTK_DIR}")
