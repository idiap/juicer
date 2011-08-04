#
# Find Tracter
#
# Phil Garner
# March 2009
#
# Tracter uses (and has always used) pkgconfig...
#
find_package(PkgConfig)
pkg_check_modules(TRACTER REQUIRED tracter)
