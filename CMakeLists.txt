cmake_minimum_required(VERSION 3.21)
#cmake_policy(SET CMP0048 NEW) # use version number in command project()
##############################
project(DABstar VERSION 1.0.0)
##############################

set(objectName "dabstar")

# special release version flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DQT_NO_DEBUG_OUTPUT") # inhibits qDebug outputs in release version
#set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")  # CMake adds -flto already to release version

# common flags
set(CMAKE_CXX_FLAGS "${CMAKE_XCC_FLAGS} -Wall -std=c++17")

set(CMAKE_AUTORCC ON)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/bin" CACHE PATH "default install path" FORCE)
endif ()

#set (CMAKE_INSTALL_PREFIX /usr/local/bin)
message(STATUS "${PROJECT_NAME} V${PROJECT_VERSION}")

add_definitions(-DAPP_NAME="${objectName}")
add_definitions(-DPRJ_NAME="${PROJECT_NAME}")
add_definitions(-DPRJ_VERS="${PROJECT_VERSION}")

#	modify if you want
#	Note that since there are always problems with the rtlsdr interface
#	under Windows we split the rtlsdr interface into two versions
#	one for Linux and one for Window

if (MINGW)
    add_definitions(-municode)
    #else (MINGW)
    #	add_definitions (-DSSE_AVAILABLE)
endif ()

add_definitions(-D__FFTW3__)
#add_definitions(-D__KISS_FFT__)

IF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

if (GIT_COMMIT_HASH)
    add_definitions("-DGITHASH=\"${GIT_COMMIT_HASH}\"")
else ()
    add_definitions("-DGITHASH=\"       \"")
endif ()


########################################################################
# select the release build type by default to get optimization flags
########################################################################
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: defaulting to release.")
endif (NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

### make sure our local CMake Modules path comes first
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake/Modules)

######################################################################
######################################################################
# select only one of these audio outputs
# set() overwrites the CMake cache entries
set(USE_PORTAUDIO ON)
set(QT_AUDIO OFF)
set(TCP_STREAMER OFF)

# select one or more devices (set to ON)
# attention option() does not overwrite the CMake cache entries, so use CMake command line or your IDE to overwrite this
option(AIRSPY OFF)
option(SDRPLAY_V2 OFF)
option(SDRPLAY OFF)
option(RTLSDR_LINUX ON)
option(RTLSDR_WIN OFF)
option(HACKRF OFF)
option(LIMESDR OFF)
option(RTL_TCP OFF)
option(PLUTO OFF)

option(DATA_STREAMER OFF)
option(VITERBI_SSE OFF)
option(VITERBI_NEON OFF)
option(FDK_AAC OFF)
######################################################################
######################################################################

#add_definitions (-D__THREADED_BACKEND)	# uncomment for use for an RPI
#
########################################################################

find_package(PkgConfig)
find_package(LSBId)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
include_directories(
        ${Qt5Network_INCLUDE_DIRS}
        ${Qt5Xml_INCLUDE_DIRS}
)

find_package(Qwt)
if (NOT QWT_FOUND)
    message(FATAL_ERROR "please install qwt library")
endif ()
include_directories(${QWT_INCLUDE_DIRS})
list(APPEND extraLibs ${QWT_LIBRARIES})

find_package(FFTW3f)
if (NOT FFTW3F_FOUND)
    message(FATAL_ERROR "please install FFTW3")
endif ()
include_directories(${FFTW_INCLUDE_DIRS})
#list(APPEND extraLibs ${FFTW3F_LIBRARIES})


find_package(zlib)
if (NOT ZLIB_FOUND)
    message(FATAL_ERROR "please install libz")
endif ()
list(APPEND extraLibs ${ZLIB_LIBRARY})

find_package(LibSndFile)
if (NOT LIBSNDFILE_FOUND)
    message(FATAL_ERROR "please install libsndfile")
endif ()
list(APPEND extraLibs ${LIBSNDFILE_LIBRARY})

find_package(LibSampleRate)
if (NOT LIBSAMPLERATE_FOUND)
    message(FATAL_ERROR "please install libsamplerate")
endif ()
list(APPEND extraLibs ${LIBSAMPLERATE_LIBRARY})

find_library(PTHREADS pthread)
if (NOT (PTHREADS))
    message(FATAL_ERROR "please install libpthread")
else (NOT (PTHREADS))
    set(extraLibs ${extraLibs} ${PTHREADS})
endif (NOT (PTHREADS))

#######################################################################
#
#	Here we really start

include_directories(
        ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        ${QT5Widgets_INCLUDES}
        ${QT_QTCORE_INCLUDE_DIR}
        ${QT_QTGUI_INCLUDE_DIR}
        ./code/helpers/pauzeslide
        ./code/support
        ./code
        ./code/ofdm
        ./code/protection
        ./code/backend
        ./code/backend/audio
        ./code/backend/data
        ./code/backend/data/journaline
        ./code/backend/data/mot
        ./code/backend/data/epg-2
        ./code/backend/data/epg
        ./code/support
        ./code/support/tii-library
        ./code/support/buttons
        ./code/support/viterbi-jan
        ./code/support/viterbi-spiral
        ./code/output
        ./code/scopes-qwt6
        ./code/viewers
        ./code/viewers/spectrum-viewer
        ./code/viewers/tii-viewer
        ./code/viewers/snr-viewer
        ./code/qt-devices
        ./code/qt-devices/rawfiles-new
        ./code/qt-devices/wavfiles-new
        ./code/qt-devices/xml-filereader
        ./code/eti-handler
        /usr/include/
)

set(${objectName}_HDRS
        ./code/radio.h
        ./code/pauzeslide/pauzeslide.h
        ./code/dab-processor.h
        ./code/eti-handler/eti-generator.h
        ./code/dab-constants.h
        ./code/mot-content-types.h
        ./code/ofdm/sample-reader.h
        ./code/ofdm/phasereference.h
        ./code/ofdm/ofdm-decoder.h
        ./code/ofdm/phasetable.h
        ./code/ofdm/freq-interleaver.h
        ./code/ofdm/fib-decoder.h
        ./code/ofdm/dab-config.h
        ./code/ofdm/fib-table.h
        ./code/ofdm/fic-handler.h
        ./code/ofdm/tii-detector.h
        ./code/ofdm/timesyncer.h
        ./code/protection/protTables.h
        ./code/protection/protection.h
        ./code/protection/uep-protection.h
        ./code/protection/eep-protection.h
        ./code/backend/firecode-checker.h
        ./code/backend/frame-processor.h
        ./code/backend/charsets.h
        ./code/backend/galois.h
        ./code/backend/reed-solomon.h
        ./code/backend/msc-handler.h
        ./code/backend/backend.h
        ./code/backend/backend-deconvolver.h
        ./code/backend/backend-driver.h
        ./code/backend/audio/mp4processor.h
        ./code/backend/audio/bitWriter.h
        ./code/backend/audio/mp2processor.h
        ./code/backend/data/ip-datahandler.h
        ./code/backend/data/tdc-datahandler.h
        ./code/backend/data/journaline-datahandler.h
        ./code/backend/data/journaline/dabdatagroupdecoder.h
        ./code/backend/data/journaline/crc_8_16.h
        ./code/backend/data/journaline/log.h
        ./code/backend/data/journaline/newssvcdec_impl.h
        ./code/backend/data/journaline/Splitter.h
        ./code/backend/data/journaline/dabdgdec_impl.h
        ./code/backend/data/journaline/newsobject.h
        ./code/backend/data/journaline/NML.h
        ./code/backend/data/epg/epgdec.h
        ./code/backend/data/epg-2/epg-decoder.h
        ./code/backend/data/virtual-datahandler.h
        ./code/backend/data/pad-handler.h
        ./code/backend/data/mot/mot-handler.h
        ./code/backend/data/mot/mot-object.h
        ./code/backend/data/mot/mot-dir.h
        ./code/backend/data/data-processor.h
        ./code/output/fir-filters.h
        ./code/output/audio-base.h
        ./code/output/newconverter.h
        ./code/support/ringbuffer.h
        ./code/support/Xtan2.h
        ./code/support/dab-params.h
        ./code/support/band-handler.h
        ./code/support/dab-tables.h
        ./code/support/viterbi-jan/viterbi-handler.h
        ./code/support/viterbi-spiral/viterbi-spiral.h
        ./code/support/preset-handler.h
        ./code/support/presetcombobox.h
        ./code/support/history-handler.h
        ./code/support/color-selector.h
        ./code/support/time-table.h
        ./code/support/findfilenames.h
        ./code/support/content-table.h
        ./code/support/dl-cache.h
        ./code/support/ITU_Region_1.h
        ./code/support/coordinates.h
        ./code/support/mapport.h
        ./code/support/http-handler.h
        ./code/support/converted_map.h
        ./code/support/tii-library/tii-codes.h
        ./code/support/buttons/smallcombobox.h
        ./code/support/buttons/newpushbutton.h
        ./code/support/buttons/normalpushbutton.h
        ./code/support/buttons/smallqlistview.h
        ./code/support/buttons/smallspinbox.h
        ./code/scopes-qwt6/iqdisplay.h
        ./code/scopes-qwt6/spectrogramdata.h
        ./code/scopes-qwt6/audio-display.h
        ./code/viewers/spectrum-viewer/spectrum-viewer.h
        ./code/viewers/spectrum-viewer/spectrum-scope.h
        ./code/viewers/spectrum-viewer/waterfall-scope.h
        ./code/viewers/spectrum-viewer/correlation-viewer.h
        ./code/viewers/tii-viewer/tii-viewer.h
        ./code/viewers/snr-viewer/snr-viewer.h
        ./code/qt-devices/device-handler.h
        ./code/qt-devices/device-exceptions.h
        ./code/qt-devices/xml-filewriter.h
        ./code/qt-devices/rawfiles-new/rawfiles.h
        ./code/qt-devices/rawfiles-new/raw-reader.h
        ./code/qt-devices/wavfiles-new/wavfiles.h
        ./code/qt-devices/wavfiles-new/wav-reader.h
        ./code/qt-devices/xml-filereader/element-reader.h
        ./code/qt-devices/xml-filereader/xml-filereader.h
        ./code/qt-devices/xml-filereader/xml-reader.h
        ./code/qt-devices/xml-filereader/xml-descriptor.h
        )

set(${objectName}_SRCS
        ${${objectName}_SRCS}
        ./code/main.cpp
        ./code/radio.cpp
        ./code/support/techdata.cpp
        ./code/dab-processor.cpp
        ./code/eti-handler/eti-generator.cpp
        ./code/ofdm/sample-reader.cpp
        ./code/ofdm/ofdm-decoder.cpp
        ./code/ofdm/phasereference.cpp
        ./code/ofdm/phasetable.cpp
        ./code/ofdm/freq-interleaver.cpp
        ./code/ofdm/fib-decoder.cpp
        ./code/ofdm/fic-handler.cpp
        ./code/ofdm/tii-detector.cpp
        ./code/ofdm/timesyncer.cpp
        ./code/protection/protTables.cpp
        ./code/protection/protection.cpp
        ./code/protection/eep-protection.cpp
        ./code/protection/uep-protection.cpp
        ./code/backend/firecode-checker.cpp
        ./code/backend/charsets.cpp
        ./code/backend/galois.cpp
        ./code/backend/reed-solomon.cpp
        ./code/backend/msc-handler.cpp
        ./code/backend/backend.cpp
        ./code/backend/backend-deconvolver.cpp
        ./code/backend/backend-driver.cpp
        ./code/backend/audio/mp4processor.cpp
        ./code/backend/audio/bitWriter.cpp
        ./code/backend/audio/mp2processor.cpp
        ./code/backend/data/ip-datahandler.cpp
        ./code/backend/data/journaline-datahandler.cpp
        ./code/backend/data/journaline/crc_8_16.c
        ./code/backend/data/journaline/log.c
        ./code/backend/data/journaline/newssvcdec_impl.cpp
        ./code/backend/data/journaline/Splitter.cpp
        ./code/backend/data/journaline/dabdgdec_impl.c
        ./code/backend/data/journaline/newsobject.cpp
        ./code/backend/data/journaline/NML.cpp
        ./code/backend/data/epg-2/epg-decoder.cpp
        ./code/backend/data/epg/epgdec.cpp
        ./code/backend/data/tdc-datahandler.cpp
        ./code/backend/data/pad-handler.cpp
        ./code/backend/data/mot/mot-handler.cpp
        ./code/backend/data/mot/mot-object.cpp
        ./code/backend/data/mot/mot-dir.cpp
        ./code/backend/data/data-processor.cpp
        ./code/output/audio-base.cpp
        ./code/output/newconverter.cpp
        ./code/output/fir-filters.cpp
        ./code/support/Xtan2.cpp
        ./code/support/dab-params.cpp
        ./code/support/band-handler.cpp
        ./code/support/dab-tables.cpp
        ./code/support/preset-handler.cpp
        ./code/support/presetcombobox.cpp
        ./code/support/buttons/smallcombobox.cpp
        ./code/support/buttons/newpushbutton.cpp
        ./code/support/buttons/normalpushbutton.cpp
        ./code/support/buttons/smallqlistview.cpp
        ./code/support/buttons/smallspinbox.cpp
        ./code/support/history-handler.cpp
        ./code/support/viterbi-jan/viterbi-handler.cpp
        ./code/support/viterbi-spiral/viterbi-spiral.cpp
        ./code/support/color-selector.cpp
        ./code/support/time-table.cpp
        ./code/support/findfilenames.cpp
        ./code/support/content-table.cpp
        ./code/support/ITU_Region_1.cpp
        ./code/support/coordinates.cpp
        ./code/support/mapport.cpp
        ./code/support/http-handler.cpp
        ./code/support/tii-library/tii-codes.cpp
        ./code/scopes-qwt6/iqdisplay.cpp
        ./code/scopes-qwt6/audio-display.cpp
        ./code/scopes-qwt6/spectrogramdata.cpp
        ./code/viewers/spectrum-viewer/spectrum-viewer.cpp
        ./code/viewers/spectrum-viewer/spectrum-scope.cpp
        ./code/viewers/spectrum-viewer/waterfall-scope.cpp
        ./code/viewers/spectrum-viewer/correlation-viewer.cpp
        ./code/viewers/tii-viewer/tii-viewer.cpp
        ./code/viewers/snr-viewer/snr-viewer.cpp
        ./code/qt-devices/device-handler.cpp
        ./code/qt-devices/xml-filewriter.cpp
        ./code/qt-devices/rawfiles-new/rawfiles.cpp
        ./code/qt-devices/rawfiles-new/raw-reader.cpp
        ./code/qt-devices/wavfiles-new/wavfiles.cpp
        ./code/qt-devices/wavfiles-new/wav-reader.cpp
        ./code/qt-devices/xml-filereader/xml-filereader.cpp
        ./code/qt-devices/xml-filereader/xml-reader.cpp
        ./code/qt-devices/xml-filereader/xml-descriptor.cpp
        )

set(${objectName}_MOCS
        ./code/radio.h
        ./code/support/techdata.h
        ./code/dab-processor.h
        ./code/output/audio-base.h
        ./code/ofdm/sample-reader.h
        ./code/ofdm/ofdm-decoder.h
        ./code/ofdm/phasereference.h
        ./code/ofdm/fib-decoder.h
        ./code/ofdm/fic-handler.h
        ./code/backend/audio/mp2processor.h
        ./code/backend/audio/mp4processor.h
        ./code/backend/data/virtual-datahandler.h
        ./code/backend/data/pad-handler.h
        ./code/backend/data/mot/mot-object.h
        ./code/backend/data/ip-datahandler.h
        ./code/backend/data/tdc-datahandler.h
        ./code/backend/data/epg-2/epg-decoder.h
        ./code/backend/data/data-processor.h
        ./code/support/band-handler.h
        ./code/support/preset-handler.h
        ./code/support/presetcombobox.h
        ./code/support/buttons/smallcombobox.h
        ./code/support/buttons/newpushbutton.h
        ./code/support/buttons/normalpushbutton.h
        ./code/support/buttons/smallqlistview.h
        ./code/support/buttons/smallspinbox.h
        ./code/support/history-handler.h
        ./code/support/color-selector.h
        ./code/support/time-table.h
        ./code/support/content-table.h
        ./code/support/coordinates.h
        ./code/support/mapport.h
        ./code/support/http-handler.h
        ./code/scopes-qwt6/iqdisplay.h
        ./code/scopes-qwt6/audio-display.h
        ./code/viewers/spectrum-viewer/spectrum-viewer.h
        ./code/viewers/spectrum-viewer/spectrum-scope.h
        ./code/viewers/spectrum-viewer/waterfall-scope.h
        ./code/viewers/spectrum-viewer/correlation-viewer.h
        ./code/viewers/tii-viewer/tii-viewer.h
        ./code/viewers/snr-viewer/snr-viewer.h
        ./code/qt-devices/rawfiles-new/rawfiles.h
        ./code/qt-devices/rawfiles-new/raw-reader.h
        ./code/qt-devices/wavfiles-new/wavfiles.h
        ./code/qt-devices/wavfiles-new/wav-reader.h
        ./code/qt-devices/xml-filereader/xml-filereader.h
        ./code/qt-devices/xml-filereader/xml-reader.h
        )

set(${objectName}_UIS
        ${${objectName}_UIS}
        ./forms/dabradio.ui
        ./forms/technical_data.ui
        ./forms/config-helper.ui
        ./code/viewers/spectrum-viewer/scopewidget.ui
        ./code/viewers/tii-viewer/tii-widget.ui
        ./code/viewers/snr-viewer/snr-widget.ui
        ./code/qt-devices/xml-filereader/xmlfiles.ui
        )

#########################################################################

if (FDK_AAC)
    find_package(Fdk-AAC)
    if (NOT LIBFDK_AAC_FOUND)
        message(FATAL_ERROR, "please install libfdk-aac")
    endif ()
    include_directories(./specials/fdk-aac)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/backend/audio/fdk-aac.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/backend/audio/fdk-aac.cpp
            )
    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/backend/audio/fdk-aac.h
            )
    list(APPEND extraLibs ${FDK_AAC_LIBRARIES})
    add_definitions(-D__WITH_FDK_AAC__)
elseif (NOT FDK_AAC)
    find_package(Faad)
    if (NOT FAAD_FOUND)
        message(FATAL_ERROR "please install libfaad")
    endif ()
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/backend/audio/faad-decoder.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/backend/audio/faad-decoder.cpp
            )
    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/backend/audio/faad-decoder.h
            )
    add_definitions(-D__WITH_FAAD__)
endif (FDK_AAC)

#	the viterbi decoder
#
if (VITERBI_NEON)
    set(VITERBI_SSE false)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/support/viterbi-spiral/spiral-neon.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/support/viterbi-spiral/spiral-neon.c
            )

    add_definitions(-DNEON_AVAILABLE)
elseif (VITERBI_SSE)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/support/viterbi-spiral/spiral-sse.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/support/viterbi-spiral/spiral-sse.c
            )
    add_definitions(-DSSE_AVAILABLE)
else (VITERBI_SSE)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/support/viterbi-spiral/spiral-no-sse.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/support/viterbi-spiral/spiral-no-sse.c
            )
endif (VITERBI_NEON)

##########################################################################
#	The devices
#
if (SDRPLAY_V2)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/sdrplay-handler-v2/sdrplay-widget-v2.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/sdrplay-handler-v2/sdrplay-handler-v2.h
            ./code/qt-devices/sdrplay-handler-v2/sdrplayselect.h
            )

    include_directories(
            ./code/qt-devices/sdrplay-handler-v2
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/sdrplay-handler-v2/sdrplay-handler-v2.h
            ./code/qt-devices/sdrplay-handler-v2/sdrplayselect.h
            ./code/qt-devices/sdrplay-handler-v2/mirsdrapi-rsp.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/sdrplay-handler-v2/sdrplay-handler-v2.cpp
            ./code/qt-devices/sdrplay-handler-v2/sdrplayselect.cpp
            )

    add_definitions(-DHAVE_SDRPLAY_V2)
endif (SDRPLAY_V2)

if (SDRPLAY)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/sdrplay-handler-v3/sdrplay-widget-v3.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/sdrplay-handler-v3/sdrplay-handler-v3.h
            ./code/qt-devices/sdrplay-handler-v3/Rsp-device.h
            )

    include_directories(
            ./code/qt-devices/sdrplay-handler-v3
            ./code/qt-devices/sdrplay-handler-v3/include
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/sdrplay-handler-v3/sdrplay-handler-v3.h
            ./code/qt-devices/sdrplay-handler-v3/sdrplay-commands.h
            ./code/qt-devices/sdrplay-handler-v3/include/sdrplay_api.h
            ./code/qt-devices/sdrplay-handler-v3/Rsp-device.h
            ./code/qt-devices/sdrplay-handler-v3/Rsp1A-handler.h
            ./code/qt-devices/sdrplay-handler-v3/RspII-handler.h
            ./code/qt-devices/sdrplay-handler-v3/RspDuo-handler.h
            ./code/qt-devices/sdrplay-handler-v3/RspDx-handler.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/sdrplay-handler-v3/sdrplay-handler-v3.cpp
            ./code/qt-devices/sdrplay-handler-v3/Rsp-device.cpp
            ./code/qt-devices/sdrplay-handler-v3/Rsp1A-handler.cpp
            ./code/qt-devices/sdrplay-handler-v3/RspII-handler.cpp
            ./code/qt-devices/sdrplay-handler-v3/RspDuo-handler.cpp
            ./code/qt-devices/sdrplay-handler-v3/RspDx-handler.cpp
            )

    add_definitions(-DHAVE_SDRPLAY_V3)
endif (SDRPLAY)

if (AIRSPY)
    #           find_package(LibAIRSPY)
    #           if (NOT LIBAIRSPY_FOUND)
    #               message(FATAL_ERROR "please install airspy library")
    #           endif ()
    #	   ## include_directories (${AIRSPYLIB_INCLUDE_DIR})

    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/airspy-handler/airspy-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/airspy-handler/airspy-handler.h
            ./code/qt-devices/airspy-handler/airspyselect.h
            )

    include_directories(
            ./qt-devices/airspy-handler
            ./qt-devices/airspy-handler/libairspy
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/airspy-handler/airspy-handler.h
            ./code/qt-devices/airspy-handler/airspyselect.h
            ./code/qt-devices/airspy-handler/libairspy/airspy.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/airspy-handler/airspy-handler.cpp
            ./code/qt-devices/airspy-handler/airspyselect.cpp
            )

    add_definitions(-DHAVE_AIRSPY)
endif (AIRSPY)

if (HACKRF)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/hackrf-handler/hackrf-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/hackrf-handler/hackrf-handler.h
            )

    include_directories(
            ./code/qt-devices/hackrf-handler
            ./code/qt-devices/hackrf-handler/libhackrf
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/hackrf-handler/hackrf-handler.h
            ./code/qt-devices/hackrf-handler/libhackrf/hackrf.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/hackrf-handler/hackrf-handler.cpp
            )

    add_definitions(-DHAVE_HACKRF)
endif (HACKRF)
#
if (PLUTO)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/pluto-handler-2/pluto-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/pluto-handler-2/pluto-handler.h
            )

    include_directories(
            ./code/qt-devices/pluto-handler-2
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/pluto-handler-2/pluto-handler.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/pluto-handler-2/pluto-handler.cpp
            )

    add_definitions(-DHAVE_PLUTO)
endif (PLUTO)
#
if (LIMESDR)
    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/lime-handler/lime-handler.h
            ./code/qt-devices/lime-handler/lime-widget.h
            )

    include_directories(
            ./code/qt-devices/lime-handler
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/lime-handler/lime-handler.h
            ./code/qt-devices/lime-handler/lime-widget.h
            ./code/qt-devices/lime-handler/LimeSuite.h
            ./code/qt-devices/lime-handler/LMS7002M_parameters.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/lime-handler/lime-handler.cpp
            )

    add_definitions(-DHAVE_LIME)
endif (LIMESDR)

if (RTLSDR_LINUX)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/rtlsdr-handler-linux/rtlsdr-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/rtlsdr-handler-linux/rtl-dongleselect.h
            ./code/qt-devices/rtlsdr-handler-linux/rtlsdr-handler.h
            )

    include_directories(
            ./code/qt-devices/rtlsdr-handler-linux/
    )

    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/rtlsdr-handler-linux/rtlsdr-handler.h
            ./code/qt-devices/rtlsdr-handler-linux/rtl-dongleselect.h
            ./code/qt-devices/rtlsdr-handler-linux/rtl-sdr.h
            ./code/qt-devices/rtlsdr-handler-linux/rtl-sdr_export.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/rtlsdr-handler-linux/rtlsdr-handler.cpp
            ./code/qt-devices/rtlsdr-handler-linux/rtl-dongleselect.cpp
            )

    add_definitions(-DHAVE_RTLSDR)
endif ()

if (RTLSDR_WIN)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/rtlsdr-handler-win/rtlsdr-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/rtlsdr-handler-win/rtl-dongleselect.h
            ./code/qt-devices/rtlsdr-handler-win/rtlsdr-handler.h
            )

    include_directories(
            ./qt-devices/rtlsdr-handler-win/
    )

    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/rtlsdr-handler-win/rtlsdr-handler.h
            ./code/qt-devices/rtlsdr-handler-win/rtl-dongleselect.h
            ./code/qt-devices/rtlsdr-handler-win/rtl-sdr.h
            ./code/qt-devices/rtlsdr-handler-win/rtl-sdr_export.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/rtlsdr-handler-win/rtlsdr-handler.cpp
            ./code/qt-devices/rtlsdr-handler-win/rtl-dongleselect.cpp
            )

    add_definitions(-DHAVE_RTLSDR)
endif ()

#
if (TCP_STREAMER)
    add_definitions(-DTCP_STREAMER)
    find_package(Qt5Network REQUIRED)
    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/output/tcp-streamer.h
            )
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/output/tcp-streamer.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/output/tcp-streamer.cpp
            )
endif (TCP_STREAMER)

if (QT_AUDIO)
    add_definitions(-DQT_AUDIO)
    find_package(Qt5Multimedia REQUIRED)
    include_directories(
            ${Qt5Multimedia_INCLUDE_DIRS}
    )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/output/Qt-audio.h
            ./code/output/Qt-audiodevice.h
            )
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/output/Qt-audio.h
            ./code/output/Qt-audiodevice.h
            )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/output/Qt-audio.cpp
            ./code/output/Qt-audiodevice.cpp
            )
endif (QT_AUDIO)

if (RTL_TCP)
    find_package(Qt5Network REQUIRED)
    set(${objectName}_UIS
            ${${objectName}_UIS}
            ./code/qt-devices/rtl_tcp/rtl_tcp-widget.ui
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/qt-devices/rtl_tcp/rtl_tcp_client.h
            )

    include_directories(
            ./code/qt-devices/rtl_tcp
            ${Qt5Network_INCLUDE_DIRS}
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/qt-devices/rtl_tcp/rtl_tcp_client.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/qt-devices/rtl_tcp/rtl_tcp_client.cpp
            )

    set(RTLTCP_lib Qt5::Network)
    add_definitions(-DHAVE_RTL_TCP)
endif (RTL_TCP)

if (DATA_STREAMER)
    include_directories(
            ./code/server-thread
    )

    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            ./code/server-thread/tcp-server.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/server-thread/tcp-server.cpp
            )
    add_definitions(-DDATA_STREAMER)
endif (DATA_STREAMER)

if (USE_PORTAUDIO)
    find_package(Portaudio)
    if (NOT PORTAUDIO_FOUND)
        message(FATAL_ERROR "please install portaudio V19")
    endif ()
    list(APPEND extraLibs ${PORTAUDIO_LIBRARIES})
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            ./code/output/audiosink.h
            )

    set(${objectName}_MOCS
            ${${objectName}_MOCS}
            ./code/output/audiosink.h
            )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            ./code/output/audiosink.cpp
            )
endif (USE_PORTAUDIO)

QT5_WRAP_UI(UIS ${${objectName}_UIS}
        ./code/qt-devices/filereader-widget.ui)

include_directories(
        ${SDRPLAY_INCLUDES}
        ${QT5Widgets_INCLUDES}
        ${QT_QTCORE_INCLUDE_DIR}
        ${QT_QTGUI_INCLUDE_DIR}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${FAAD_INCLUDE_DIRS}
        ${SNDFILES_INCLUDE_DIRS}
)

QT5_WRAP_CPP(MOCS ${${objectName}_MOCS})

add_executable(${objectName}
        ${${objectName}_SRCS}
        ${UIS}
        ${RSCS}
        ${TRS}
        ${MOCS}
        resources.qrc
        )

target_link_libraries(${objectName}
        PRIVATE
        Qt5::Widgets
        Qt5::Network
        Qt5::Xml
        ${RTLTCP_lib}
        ${extraLibs}
        ${FAAD_LIBRARIES}
        ${CMAKE_DL_LIBS}
        )

if (QT_AUDIO)
    target_link_libraries(${objectName} PRIVATE Qt5::Multimedia)
endif (QT_AUDIO)

INSTALL(TARGETS ${objectName} DESTINATION ${CMAKE_INSTALL_PREFIX})
#	INSTALL (TARGETS ${objectName} DESTINATION ./linux-bin)

########################################################################
# Create uninstall target
########################################################################

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)


add_subdirectory(code/fft)
target_link_libraries(${objectName} PRIVATE fft)
