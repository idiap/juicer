#
# CMake file for Juicer
#
# Phil Garner
# March 2009
#

# Version 2.4 might work, but it won't find static libraries with odd names
cmake_minimum_required(VERSION 2.6) 

# CMake used to complain without this
#if(COMMAND cmake_policy)
#  cmake_policy(SET CMP0003 NEW)
#endif(COMMAND cmake_policy)

# Top level project information
project(juicer)
set(VERSION 0.9.1)

# Find packages
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Tracter REQUIRED)
find_package(Torch3 REQUIRED)
find_package(HTK)

# Configure other files
configure_file(Doxyfile.in Doxyfile)

add_subdirectory(src)
add_subdirectory(bin)

# This one is for binary distributions
include(CPack)

# For source, it's *much* better to use git
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${VERSION})
add_custom_target(dist
  COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
    | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
