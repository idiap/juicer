#
# Find Tracter
#
# Phil Garner
# March 2009
#
# Tracter uses (and has always used) pkgconfig...
#
include(FindPkgConfig)

pkg_check_modules(TRACTER REQUIRED tracter)
