
find_path(QWT_INCLUDE_DIRS
        NAMES qwt_global.h
        HINTS
        ${CMAKE_INSTALL_PREFIX}/include/qwt
        ${CMAKE_INSTALL_PREFIX}/include/qwt-qt5
        $ENV{QWT_ROOT}
        PATHS
        /usr/local/include/qwt-qt4
        /usr/local/include/qwt
        /usr/include/qwt6
        /usr/include/qwt-qt5
        /usr/include/qwt-qt4
        /usr/include/qwt
        /usr/include/qwt5
        /usr/include/qwt6-qt5
        /usr/include/qt5/qwt
        /opt/local/include/qwt
        /sw/include/qwt
        /usr/local/lib/qwt.framework/Headers
        /usr/local/opt/qwt-qt5/lib/qwt.framework/Headers
        /usr/local/qwt-6.2.0/include
)

if (APPLE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES " " " .dylib" ".so" ".a ")
endif (APPLE)

find_library(QWT_LIBRARIES
        NAMES qwt qwt6 qwt6-qt5 qwt-qt5 qwt6-qt4 qwt-qt4
        HINTS
        $ENV{QWT_ROOT}
        ${CMAKE_INSTALL_PREFIX}/lib
        ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS
        /usr/local/lib
        /usr/lib
        /opt/local/lib
        /sw/lib
        /usr/local/lib/qwt.framework
        /usr/local/opt/qwt-qt5/lib/qwt.framework
        /usr/local/qwt-6.2.0/lib
)

# extract version number out from file qwt_global.h
if (QWT_INCLUDE_DIRS)
  file(STRINGS "${QWT_INCLUDE_DIRS}/qwt_global.h" QWT_STRING_VERSION REGEX "QWT_VERSION_STR")
  string(REGEX MATCH "[0-9]+.[0-9]+.[0-9]+" QWT_VERSION ${QWT_STRING_VERSION})
endif ()

# do some outputs to the console
include(${CMAKE_CURRENT_LIST_DIR}/CMakeFunctions.cmake)
print(QWT_VERSION)
print(QWT_INCLUDE_DIRS)
print(QWT_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qwt DEFAULT_MSG QWT_LIBRARIES QWT_INCLUDE_DIRS)
mark_as_advanced(QWT_LIBRARIES QWT_INCLUDE_DIRS)

#print(QWT_FOUND)