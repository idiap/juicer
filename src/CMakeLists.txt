# Just shortcuts to prevent some long lines
set(CBD ${CMAKE_CURRENT_BINARY_DIR})
set(CSD ${CMAKE_CURRENT_SOURCE_DIR})
set(RUN ${CMAKE_COMMAND})

include_directories(${CSD})
include_directories(${TORCH3_INCLUDE_DIRS})
include_directories(${TRACTER_INCLUDE_DIRS})
link_directories(${TORCH3_LIBRARY_DIRS})
link_directories(${TRACTER_LIBRARY_DIRS})

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

# HLKLib has a non-std name
add_custom_command(
  OUTPUT ${CBD}/libhtk.a
  COMMAND ${RUN} -E create_symlink ${HTK_LIBRARIES} ${CBD}/libhtk.a
  DEPENDS ${HTK_LIBRARIES}
  )

# Basic all-the-time sources
set(SOURCES
  htkparse.y.cpp
  htkparse.l.cpp
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
  HTKModels.cpp 
  LogFile.cpp
  MonophoneLookup.cpp
  WFSTCDGen.cpp
  WFSTDecoder.cpp
  WFSTDecoderLite.cpp
  WFSTGramGen.cpp
  WFSTHMMGen.cpp
  WFSTLattice.cpp
  WFSTLexGen.cpp
  WFSTModel.cpp
  WFSTNetwork.cpp
  WordPairLM.cpp
  string_stuff.cpp
  )

# Optional things
if(HTK_FOUND)
  list(APPEND SOURCES HModels.cpp)
  add_definitions(-DHAVE_HTKLIB)
  include_directories(${HTK_INCLUDE_DIRS})
endif(HTK_FOUND)


add_library(shared-lib SHARED ${SOURCES})
set_target_properties(shared-lib
  PROPERTIES OUTPUT_NAME "juicer"
  )

target_link_libraries(shared-lib
  ${TRACTER_LIBRARIES}
  ${TORCH3_LIBRARIES}
  ${HTK_LIBRARIES}
  )

add_executable(juicer juicer.cpp)
add_executable(gramgen gramgen.cpp)
add_executable(lexgen lexgen.cpp)
add_executable(cdgen cdgen.cpp)
add_executable(genwfstseqs genwfstseqs.cpp)

target_link_libraries(juicer shared-lib)
target_link_libraries(gramgen shared-lib)
target_link_libraries(lexgen shared-lib)
target_link_libraries(cdgen shared-lib)
target_link_libraries(genwfstseqs shared-lib)
