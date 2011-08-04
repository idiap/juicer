# Optimisations

add_definitions(-DOPT_FLATMODEL)
add_definitions(-DOPT_SINGLE_BEST)
add_definitions(-DPARTIAL_DECODING)

#add_definitions(-DUSE_BINARY_WFST)
#add_definitions(-DUSE_BINARY_MODELS)


#add_definitions(-DOPT_FAST_LOG)
#add_definitions(-DOPT_FAST_EXP)

# use double instead of real for Token.score in WFSTDecoderLite
#add_definitions(-DUSE_DOUBLE_SCORE)

# OPT_ALIGN4 seems to work well with INTEL's IPP lib, but maybe slower
# otherwise so it will be turned on automatically when HAVE_INTEL_IPP
# is defined
#add_definitions(-DOPT_ALIGN4)

# note: NO_BEST_END and NO_BEST_START only apply to WFSTDecoder.cpp,
# it won't affect WFSTDecoderLite
#add_definitions(-DNO_BEST_END)
#add_definitions(-DNO_BEST_START)


# Just shortcuts to prevent some long lines
set(CBD ${CMAKE_CURRENT_BINARY_DIR})
set(CSD ${CMAKE_CURRENT_SOURCE_DIR})
set(RUN ${CMAKE_COMMAND})

include_directories(${CSD})
include_directories(${TORCH3_INCLUDE_DIRS})
include_directories(${TRACTER_INCLUDE_DIRS})
link_directories(${TORCH3_LIBRARY_DIRS})
link_directories(${TRACTER_LIBRARY_DIRS})
add_definitions(-DPACKAGE_VERSION="${VERSION}")

# Bison
add_custom_command(
  OUTPUT ${CBD}/htkparse.y.h ${CBD}/htkparse.y.cpp
  COMMAND bison -y -d -p htk ${CSD}/htkparse.y.ypp
  COMMAND ${RUN} -E create_symlink ${CBD}/y.tab.c ${CBD}/htkparse.y.cpp
  COMMAND ${RUN} -E create_symlink ${CBD}/y.tab.h ${CBD}/htkparse.y.h
  DEPENDS ${CSD}/htkparse.y.ypp
  )

# Flex
add_custom_command(
  OUTPUT ${CBD}/htkparse.l.cpp
  COMMAND flex -Phtk -L ${CSD}/htkparse.l.lpp
  COMMAND ${RUN} -E create_symlink ${CBD}/lex.htk.c ${CBD}/htkparse.l.cpp
  DEPENDS ${CSD}/htkparse.l.lpp ${CBD}/htkparse.y.h
  )

# Basic all-the-time sources
set(SOURCES
  ARPALM.cpp
  BlockMemPool.cpp
  DecHypHistPool.cpp
  DecLexInfo.cpp
  DecoderBatchTest.cpp
  DecoderSingleTest.cpp
  DecPhoneInfo.cpp
  DecVocabulary.cpp
  Histogram.cpp
  HTKFlatModels.cpp
  HTKFlatModelsThreading.cpp
  HTKModels.cpp 
  LogFile.cpp
  MonophoneLookup.cpp
  WFSTCDGen.cpp
  WFSTDecoder.cpp
  WFSTDecoderLite.cpp
  WFSTDecoderLiteThreading.cpp
  WFSTGramGen.cpp
  WFSTHMMGen.cpp
  WFSTLattice.cpp
  WFSTLexGen.cpp
  WFSTModel.cpp
  WFSTNetwork.cpp
  WordPairLM.cpp
  string_stuff.cpp
  )

# Prime the header list with the cpp sources, then add the lone ones
string(REGEX REPLACE ".cpp" ".h" HEADERS "${SOURCES}")
list(APPEND HEADERS
  FrontEnd.h
  htkparse.h
  )

# Some sources without headers to install
list(APPEND SOURCES
  htkparse.y.cpp
  htkparse.l.cpp
  )

# Optional things
if(HTK_FOUND)
  list(APPEND SOURCES HModels.cpp)
  list(APPEND HEADERS HModels.h  )
  add_definitions(-DHAVE_HTKLIB)
  include_directories(${HTK_INCLUDE_DIRS})
endif(HTK_FOUND)

# For projuicer
set(USE_JNI $ENV{USE_JNI}
  CACHE BOOLEAN "Whether to build the JNI interface"
  )
if (USE_JNI)
  list(APPEND SOURCES juicer_wrap.cxx)
  include_directories(${JNI_INCLUDE_DIRS})
endif (USE_JNI)

set(TARGET_LIBS
  ${TRACTER_LIBRARIES}
  ${TORCH3_LIBRARIES}
  ${HTK_LIBRARIES}
  ${SPTK_LIBRARIES}
  ${RTAUDIO_LIBRARIES}
  ${LIBRESAMPLE_LIBRARIES}
  )

set(INSTALL_TARGETS
  juicer
  gramgen
  lexgen
  cdgen
  genwfstseqs
  static-lib
)

# Static library
add_library(static-lib STATIC ${SOURCES})
set_target_properties(static-lib
  PROPERTIES OUTPUT_NAME "juicer"
             PUBLIC_HEADER "${HEADERS}"
  )
target_link_libraries(static-lib ${TARGET_LIBS};pthread)

# Shared library
set(USE_SHARED $ENV{USE_SHARED}
  CACHE BOOLEAN "Whether to build shared library"
  )
if (USE_SHARED)
  add_library(shared-lib SHARED ${SOURCES})
  set_target_properties(shared-lib
    PROPERTIES OUTPUT_NAME "juicer"
    PUBLIC_HEADER "${HEADERS}"
    )
  target_link_libraries(shared-lib ${TARGET_LIBS})
  list(APPEND INSTALL_TARGETS shared-lib)
endif (USE_SHARED)

add_executable(juicer juicer.cpp)
add_executable(gramgen gramgen.cpp)
add_executable(lexgen lexgen.cpp)
add_executable(cdgen cdgen.cpp)
add_executable(genwfstseqs genwfstseqs.cpp)

# These depend on the static lib for now
target_link_libraries(juicer static-lib)
target_link_libraries(gramgen static-lib)
target_link_libraries(lexgen static-lib)
target_link_libraries(cdgen static-lib)
target_link_libraries(genwfstseqs static-lib)

install(
  TARGETS ${INSTALL_TARGETS}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/juicer
  )

# pkgconfig install lines
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include/juicer")
configure_file(juicer.pc.in ${CMAKE_CURRENT_BINARY_DIR}/juicer.pc)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/juicer.pc
  DESTINATION lib/pkgconfig
  )
