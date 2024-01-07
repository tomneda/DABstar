# - Try to find Portaudio
# Once done this will define
#
#  PORTAUDIO_FOUND - system has Portaudio
#  PORTAUDIO_INCLUDE_DIRS - the Portaudio include directory
#  PORTAUDIO_LIBRARIES - Link these to use Portaudio

include(FindPkgConfig)
pkg_check_modules(PC_PORTAUDIO portaudio-2.0)

find_path(PORTAUDIO_INCLUDE_DIRS
        NAMES
        portaudio.h
        PATHS
        /usr/local/include
        /usr/include
        HINTS
        ${PC_PORTAUDIO_INCLUDEDIR}
)

find_library(PORTAUDIO_LIBRARIES
        NAMES
        portaudio
        PATHS
        /usr/local/lib
        /usr/lib
        /usr/lib64
        HINTS
        ${PC_PORTAUDIO_LIBDIR}
)

mark_as_advanced(PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES)
#message(STATUS "PORTAUDIO_INCLUDE_DIRS: ${PORTAUDIO_INCLUDE_DIRS}")
#message(STATUS "PORTAUDIO_LIBRARIES: ${PORTAUDIO_LIBRARIES}")
#message(STATUS "PC_PORTAUDIO_INCLUDEDIR: ${PC_PORTAUDIO_INCLUDEDIR}")
#message(STATUS "PC_PORTAUDIO_LIBDIR: ${PC_PORTAUDIO_LIBDIR}")

if (EXISTS ${PORTAUDIO_INCLUDE_DIRS}/portaudio.h)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(PORTAUDIO DEFAULT_MSG PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES)
endif ()
