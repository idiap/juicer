#
# CMake file for Juicer
#
# Phil Garner
# March 2009
#

# Version 2.4 might work, but it won't find static libraries with odd names
cmake_minimum_required(VERSION 2.6) 
project(Juicer)
set(VERSION 0.9.1)

# CMake complains without this
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Tracter REQUIRED)
find_package(Torch3 REQUIRED)
find_package(HTK)

configure_file(Doxyfile.in Doxyfile)
configure_file(juicer.pc.in juicer.pc)

add_subdirectory(src)
