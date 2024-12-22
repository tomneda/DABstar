cmake_minimum_required(VERSION 3.16)

# set CMAKE_BUILD_TYPE to "Release" if not defined (like call from command line)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Set build type to Release if not specified" FORCE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/CMakeFunctions.cmake)

# First, we need to include vcpkg's auto-loader script, it sets up CMAKE_TOOLCHAIN_FILE
# IMPORTANT: this must be done for the call to project() ?!
try_find_vcpkg_toolset_path()
print(CMAKE_TOOLCHAIN_FILE)

##############################
project(DABstar VERSION 3.2.0)
##############################

set(objectName "dabstar")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_LTO "Set -flto flag" OFF)  # switch off to save compile/link time while development

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# special release version flags
if (USE_LTO)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
endif ()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DQT_NO_DEBUG_OUTPUT") # inhibits qDebug outputs in release version

# common flags
#if (WIN32)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math -DALLOW_SMP_DANGERS -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING")
#else ()
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math")
#endif ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math")


if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/bin" CACHE PATH "default install path" FORCE)
endif ()

print(PROJECT_NAME)
print(PROJECT_VERSION)
print(CMAKE_BUILD_TYPE)

add_definitions(-DAPP_NAME="${objectName}")
add_definitions(-DPRJ_NAME="${PROJECT_NAME}")
add_definitions(-DPRJ_VERS="${PROJECT_VERSION}")

# choosing this optionally is necessary as the repository could be only accessed as read-only while appimage generation
option(CONV_IN_FILES "Convert .in-files" ON)

if (CONV_IN_FILES)
    configure_file("${PROJECT_SOURCE_DIR}/appimage_creator/dabstar.desktop.in"
                   "${PROJECT_SOURCE_DIR}/appimage_creator/dabstar.desktop" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/appimage_creator/create_appimage.sh.in"
                   "${PROJECT_SOURCE_DIR}/appimage_creator/create_appimage.sh" @ONLY)
endif ()

if (MINGW)
    add_definitions(-municode)
endif ()

# handle GIT hash
get_git_commit_hash() # returns GIT_COMMIT_HASH
print(GIT_COMMIT_HASH)
if (GIT_COMMIT_HASH)
    add_definitions("-DGITHASH=\"${GIT_COMMIT_HASH}\"")
else ()
    add_definitions("-DGITHASH=\"(unknown)\"")
endif ()


########################################################################
# select the release build type by default to get optimization flags
########################################################################
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to release.")
endif (NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")
print(CMAKE_BUILD_TYPE)

### make sure our local CMake Modules path comes first
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake/Modules)
print(CMAKE_MODULE_PATH)

######################################################################
######################################################################

# select one or more devices (set to ON)
# attention: option() does not overwrite the CMake cache entries, so use CMake command line or your IDE to overwrite this
# As I (tomneda) possess not all of these devices I place an "untested" behind. They may run, or not run.
option(AIRSPY     "Use AirSpy"     OFF)   # untested
option(SDRPLAY_V2 "Use SdrPlay V2" OFF)   # untested
option(SDRPLAY_V3 "Use SdrPlay V3" OFF)
option(HACKRF     "Use HackRf"     OFF)
option(LIMESDR    "Use LimeSdr"    OFF)   # untested
option(RTL_TCP    "Use RtlTcp"     OFF)   # untested
option(PLUTO      "Use Pluto"      OFF)   # untested
option(UHD        "Use UHD"        OFF)
option(RTLSDR     "Use Rtlsdr"      ON)

# additional options
option(USE_HBF       "Use oversampling with half band filter" OFF)  # this needs LiquidDsp
option(DATA_STREAMER "Use DataStreamer"  OFF)
option(VITERBI_SSE   "Use Viterbi SSE"   OFF)
option(VITERBI_NEON  "Use Viterbi NEON"  OFF)
option(FDK_AAC       "Use FDK-AAC"        ON)

set(USE_FFT FFTW3 CACHE STRING "Choose FFT out of these free: FFTW3 KISS NAYUKI")
set_property(CACHE USE_FFT PROPERTY STRINGS "FFTW3;KISS;NAYUKI")

######################################################################
######################################################################

#add_definitions(-D__THREADED_BACKEND__) # uncomment for use for an RPI
#add_definitions (-D__EPG_TRACE__)

########################################################################
if (USE_HBF)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/support/halfbandfilter.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/support/halfbandfilter.cpp
    )

    list(APPEND extraLibs liquid)

    add_definitions(-DHAVE_HBF)
endif ()

find_package(PkgConfig) # imports pkg_check_modules() macros
find_package(LSBId REQUIRED)
find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)
find_package(Qt6Xml REQUIRED)
find_package(Qt6Sql REQUIRED)
find_package(Qt6Multimedia REQUIRED)

# this should be in FFT sub project but it is important that the compiler switches are already set here
if(USE_FFT STREQUAL FFTW3)
    message("FFTW3 is used for FFT")
    find_package(FFTW3f)
    if (NOT FFTW3F_FOUND)
        message(FATAL_ERROR "Please install FFTW3")
    endif ()
    include_directories(${FFTW3F_INCLUDE_DIRS})
    list(APPEND extraLibs ${FFTW3F_LIBRARIES})
    add_definitions(-D__FFTW3__)
elseif(USE_FFT STREQUAL KISS)
    message("KISS is used for FFT")
    add_definitions(-D__KISS_FFT__)
elseif(USE_FFT STREQUAL NAYUKI)
    message("NAYUKI is used for FFT")
    add_definitions(-D__NAYUKI__)
else()
    #message(FATAL_ERROR "Invalid FFT option in USE_FFT: ${USE_FFT}")
endif()

# Qwt6.3.0 built with Qt6 has an unusual installation path for its Qt6Qwt6.pc file, so add it here
if (WIN32)
    # set(ENV{PKG_CONFIG_PATH} "C:/Qwt-6.3.0/:$ENV{PKG_CONFIG_PATH}")
    # set(Qt6Qwt6_DIR "C:/Qwt-6.3.0/")
    find_package(Qwt REQUIRED) # environment variable QWT_ROOT has to be defined!
    include_directories(${QWT_INCLUDE_DIRS})
    list(APPEND extraLibs ${QWT_LIBRARIES})
else ()
    set(ENV{PKG_CONFIG_PATH} "/usr/local/qwt-6.3.0/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    search_for_library(QWT Qt6Qwt6) # maybe try change this to Qwt instead of Qt6Qwt6
endif ()

search_for_library(ZLIB zlib)
search_for_library(LIBSNDFILE sndfile)
search_for_library(LIBSAMPLERATE samplerate)

if (NOT APPLE AND NOT WIN32)
    find_library(PTHREADS pthread)
    if (NOT (PTHREADS))
        message(FATAL_ERROR "Please install libpthread")
    else (NOT (PTHREADS))
        set(extraLibs ${extraLibs} ${PTHREADS})
    endif (NOT (PTHREADS))
endif ()

#if (WIN32)
#    find_package(dlfcn-win32 REQUIRED)
#    list(APPEND extraLibs dlfcn-win32::dl)
#endif ()

#if (WIN32)
#    find_library(WS2_32_LIBRARY NAMES ws2_32)
#    print(WS2_32_LIBRARY)
#    list(APPEND extraLibs ${WS2_32_LIBRARY})
#endif ()

print(CMAKE_SOURCE_DIR)
print(CMAKE_CURRENT_BINARY_DIR)

include_directories(
        src
        src/main
        src/ofdm
        src/protection
        src/backend
        src/backend/audio
        src/backend/data
        src/backend/data/journaline
        src/backend/data/mot
        src/backend/data/epg-2
        src/backend/data/epg
        src/support
        src/support/tii-library
        src/support/buttons
        src/support/viterbi-spiral
        src/output
        src/scopes
        src/spectrum-viewer
        src/file-devices/xml-filewriter
        src/eti-handler
        src/service-list
        src/configuration
        #/usr/include/
)

set(${objectName}_HDRS
        ${${objectName}_HDRS}
        src/main/radio.h
        src/main/glob_defs.h
        src/main/glob_enums.h
        src/main/dab-processor.h
        src/main/dab-constants.h
        src/main/mot-content-types.h
        src/eti-handler/eti-generator.h
        src/main/data_manip_and_checks.h
        src/ofdm/sample-reader.h
        src/ofdm/phasereference.h
        src/ofdm/ofdm-decoder.h
        src/ofdm/phasetable.h
        src/ofdm/freq-interleaver.h
        src/ofdm/fib-decoder.h
        src/ofdm/dab-config.h
        src/ofdm/fib-table.h
        src/ofdm/fic-handler.h
        src/ofdm/tii-detector.h
        src/ofdm/timesyncer.h
        src/protection/protTables.h
        src/protection/protection.h
        src/protection/uep-protection.h
        src/protection/eep-protection.h
        src/backend/firecode-checker.h
        src/backend/frame-processor.h
        src/backend/charsets.h
        src/backend/galois.h
        src/backend/reed-solomon.h
        src/backend/msc-handler.h
        src/backend/backend.h
        src/backend/backend-deconvolver.h
        src/backend/backend-driver.h
        src/backend/audio/mp4processor.h
        src/backend/audio/bitWriter.h
        src/backend/audio/mp2processor.h
        src/backend/data/ip-datahandler.h
        src/backend/data/tdc-datahandler.h
        src/backend/data/journaline-datahandler.h
        src/backend/data/journaline/dabdatagroupdecoder.h
        src/backend/data/journaline/crc_8_16.h
        src/backend/data/journaline/log.h
        src/backend/data/journaline/newssvcdec_impl.h
        src/backend/data/journaline/Splitter.h
        src/backend/data/journaline/dabdgdec_impl.h
        src/backend/data/journaline/newsobject.h
        src/backend/data/journaline/NML.h
        src/backend/data/epg/epgdec.h
        src/backend/data/epg-2/epg-decoder.h
        src/backend/data/virtual-datahandler.h
        src/backend/data/pad-handler.h
        src/backend/data/mot/mot-handler.h
        src/backend/data/mot/mot-object.h
        src/backend/data/mot/mot-dir.h
        src/backend/data/data-processor.h
        src/output/fir-filters.h
        # src/output/audio-base.h
        src/output/audio-player.h
        src/output/newconverter.h
        src/output/audiofifo.h
        src/output/audiooutput.h
        src/output/audiooutputqt.h
        src/support/ringbuffer.h
        src/support/Xtan2.h
        src/support/dab-params.h
        src/support/band-handler.h
        src/support/dab-tables.h
        src/support/dxDisplay.h
        src/support/viterbi-spiral/viterbi-spiral.h
        src/support/color-selector.h
        src/support/time-table.h
        src/support/openfiledialog.h
        src/support/content-table.h
        src/support/dl-cache.h
        src/support/ITU_Region_1.h
        src/support/coordinates.h
        src/support/mapport.h
        src/support/http-handler.h
        src/support/tii-library/tii-codes.h
        src/support/buttons/newpushbutton.h
        src/support/buttons/normalpushbutton.h
        src/support/buttons/circlepushbutton.h
        src/support/custom_frame.h
        src/support/setting-helper.h
        src/support/wav_writer.h
        src/support/angle_direction.h
        src/scopes/iqdisplay.h
        src/scopes/carrier-display.h
        src/scopes/spectrogramdata.h
        src/scopes/audio-display.h
        src/spectrum-viewer/spectrum-viewer.h
        src/spectrum-viewer/spectrum-scope.h
        src/spectrum-viewer/waterfall-scope.h
        src/spectrum-viewer/correlation-viewer.h
        src/file-devices/xml-filewriter/xml-filewriter.h
        src/service-list/service-list-handler.h
        src/service-list/service-db.h
        src/configuration/configuration.h
        src/output/Qt-audio.h
        src/output/Qt-audiodevice.h
)

set(${objectName}_SRCS
        ${${objectName}_SRCS}
        src/main/main.cpp
        src/main/radio.cpp
        src/main/dab-processor.cpp
        src/support/techdata.cpp
        src/eti-handler/eti-generator.cpp
        src/ofdm/sample-reader.cpp
        src/ofdm/ofdm-decoder.cpp
        src/ofdm/phasereference.cpp
        src/ofdm/phasetable.cpp
        src/ofdm/freq-interleaver.cpp
        src/ofdm/fib-decoder.cpp
        src/ofdm/fic-handler.cpp
        src/ofdm/tii-detector.cpp
        src/ofdm/timesyncer.cpp
        src/protection/protTables.cpp
        src/protection/protection.cpp
        src/protection/eep-protection.cpp
        src/protection/uep-protection.cpp
        src/backend/firecode-checker.cpp
        src/backend/charsets.cpp
        src/backend/galois.cpp
        src/backend/reed-solomon.cpp
        src/backend/msc-handler.cpp
        src/backend/backend.cpp
        src/backend/backend-deconvolver.cpp
        src/backend/backend-driver.cpp
        src/backend/audio/mp4processor.cpp
        src/backend/audio/bitWriter.cpp
        src/backend/audio/mp2processor.cpp
        src/backend/data/ip-datahandler.cpp
        src/backend/data/journaline-datahandler.cpp
        src/backend/data/journaline/crc_8_16.c
        src/backend/data/journaline/log.c
        src/backend/data/journaline/newssvcdec_impl.cpp
        src/backend/data/journaline/Splitter.cpp
        src/backend/data/journaline/dabdgdec_impl.c
        src/backend/data/journaline/newsobject.cpp
        src/backend/data/journaline/NML.cpp
        src/backend/data/epg-2/epg-decoder.cpp
        src/backend/data/epg/epgdec.cpp
        src/backend/data/tdc-datahandler.cpp
        src/backend/data/pad-handler.cpp
        src/backend/data/mot/mot-handler.cpp
        src/backend/data/mot/mot-object.cpp
        src/backend/data/mot/mot-dir.cpp
        src/backend/data/data-processor.cpp
        # src/output/audio-base.cpp
        src/output/audio-player.cpp
        src/output/newconverter.cpp
        src/output/fir-filters.cpp
        src/output/audiofifo.cpp
        src/output/audiooutput.cpp
        src/output/audiooutputqt.cpp
        src/support/ringbuffer.cpp
        src/support/Xtan2.cpp
        src/support/dab-params.cpp
        src/support/band-handler.cpp
        src/support/dab-tables.cpp
        src/support/buttons/newpushbutton.cpp
        src/support/buttons/normalpushbutton.cpp
        src/support/buttons/circlepushbutton.cpp
        src/support/viterbi-spiral/viterbi-spiral.cpp
        src/support/color-selector.cpp
        src/support/time-table.cpp
        src/support/openfiledialog.cpp
        src/support/content-table.cpp
        src/support/ITU_Region_1.cpp
        src/support/coordinates.cpp
        src/support/mapport.cpp
        src/support/http-handler.cpp
        src/support/dxDisplay.cpp
        src/support/tii-library/tii-codes.cpp
        src/support/custom_frame.cpp
        src/support/setting-helper.cpp
        src/support/wav_writer.cpp
        src/support/angle_direction.cpp
        src/scopes/iqdisplay.cpp
        src/scopes/carrier-display.cpp
        src/scopes/audio-display.cpp
        src/scopes/spectrogramdata.cpp
        src/spectrum-viewer/spectrum-viewer.cpp
        src/spectrum-viewer/spectrum-scope.cpp
        src/spectrum-viewer/waterfall-scope.cpp
        src/spectrum-viewer/correlation-viewer.cpp
        src/file-devices/xml-filewriter/xml-filewriter.cpp
        src/service-list/service-list-handler.cpp
        src/service-list/service-db.cpp
        src/configuration/configuration.cpp
        src/output/Qt-audio.cpp
        src/output/Qt-audiodevice.cpp
)

#########################################################################

if (FDK_AAC)
    find_package(Fdk-AAC)
    if (NOT LIBFDK_AAC_FOUND)
        message(FATAL_ERROR, "Please install libfdk-aac")
    endif ()
    include_directories(src/specials/fdk-aac)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/backend/audio/fdk-aac.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/backend/audio/fdk-aac.cpp
    )
    list(APPEND extraLibs ${FDK_AAC_LIBRARIES})
    add_definitions(-D__WITH_FDK_AAC__)
elseif (NOT FDK_AAC)
    find_package(Faad)
    if (NOT FAAD_FOUND)
        message(FATAL_ERROR "Please install libfaad")
    endif ()
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/backend/audio/faad-decoder.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/backend/audio/faad-decoder.cpp
    )
    add_definitions(-D__WITH_FAAD__)
endif (FDK_AAC)

if (VITERBI_NEON)
    set(VITERBI_SSE false)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/spiral-neon.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/support/viterbi-spiral/spiral-neon.c
    )

    add_definitions(-DNEON_AVAILABLE)
elseif (VITERBI_SSE)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/spiral-sse.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/support/viterbi-spiral/spiral-sse.c
    )
    add_definitions(-DSSE_AVAILABLE)
else (VITERBI_SSE)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/spiral-no-sse.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/support/viterbi-spiral/spiral-no-sse.c
    )
endif (VITERBI_NEON)

if (DATA_STREAMER)
    include_directories(
            src/server-thread
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/server-thread/tcp-server.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/server-thread/tcp-server.cpp
    )
    add_definitions(-DDATA_STREAMER)
endif (DATA_STREAMER)

include_directories(
        #${SDRPLAY_INCLUDES}
        ${QT6Widgets_INCLUDES}
        ${Qt6Multimedia_INCLUDE_DIRS}
        ${QT_QTCORE_INCLUDE_DIR}
        ${QT_QTGUI_INCLUDE_DIR}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${FAAD_INCLUDE_DIRS}
        ${SNDFILES_INCLUDE_DIRS}
)

add_executable(${objectName}
        ${${objectName}_SRCS}
        ${${objectName}_HDRS}
        ${UIS}
        ${RSCS}
        ${TRS}
        ${MOCS}
        resources.qrc
)

target_link_libraries(${objectName}
        PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Xml
        Qt6::Sql
        Qt6::Multimedia
        ${RTLTCP_lib}
        ${extraLibs}
        ${FAAD_LIBRARIES}
        ${CMAKE_DL_LIBS}
)

########################################################################
# Add subsequent libraries
########################################################################

add_subdirectory(src/fft)
add_subdirectory(src/devices)

target_link_libraries(${objectName} PRIVATE ${objectName}_fft)
target_link_libraries(${objectName} PRIVATE ${objectName}_devices)

########################################################################
# Create install / uninstall target
########################################################################

install(TARGETS ${objectName} DESTINATION ${CMAKE_INSTALL_PREFIX})

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
