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
project(DABstar VERSION 4.6.0)
##############################

set(objectName "dabstar")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_SANITIZERS "Enable address/undefined sanitizers in Debug builds" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math -fsingle-precision-constant")

# as MSVC compiler does not like VLAs (Variable Length Array) inhibit them for GCC also
if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wvla -Werror=vla)
endif ()

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
option(AIRSPY     "Use AirSpy"     OFF)  # untested
option(SPYSERVER  "Use SpyServer"  OFF)  # untested
option(SDRPLAY_V2 "Use SdrPlay V2" OFF)  # untested
option(SDRPLAY_V3 "Use SdrPlay V3" OFF)
option(HACKRF     "Use HackRf"     OFF)
option(LIMESDR    "Use LimeSdr"    OFF)  # untested
option(RTL_TCP    "Use RtlTcp"     OFF)
option(PLUTO      "Use Pluto"      OFF)  # untested
option(UHD        "Use UHD"        OFF)
option(RTLSDR     "Use RtlSdr"      ON)
option(SOAPY      "Use SoapySdr"   OFF)  # partly untested


# additional options
option(USE_LIQUID    "Use LiquidDsp for HBF and Resampler"    OFF)  # this needs LiquidDsp (https://github.com/jgaeddert/liquid-dsp)
option(DATA_STREAMER "Use DataStreamer"                       OFF)  # untested
option(FDK_AAC       "Use FDK-AAC instead of FAAD"             ON)

# choose only one of the next three, otherwise the first ON-entry is valid
option(VITERBI_AVX2  "Use AVX2 for Viterbi" OFF)  # fastest, for modern Intel/AMD processors
option(VITERBI_SSE2  "Use SSE2 for Viterbi" OFF)  # quite fast, for bit older Intel/AMD processors
option(VITERBI_NEON  "Use NEON for Viterbi" OFF)  # for ARM processors like Raspi (not tested)

# there are other places like the OFDM decoder which can use SSE/AVX
option(SSE_OR_AVX    "Use SSE or AVX for OFDM and others beside Viterbi" OFF)  # needs VOLK (https://github.com/gnuradio/volk)

######################################################################
######################################################################

#add_definitions(-D__THREADED_BACKEND__) # uncomment for use for an RPI
#add_definitions (-D__EPG_TRACE__)

########################################################################
if (USE_LIQUID)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/support/halfbandfilter.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/support/halfbandfilter.cpp
    )

    list(APPEND extraLibs liquid)

    add_definitions(-DHAVE_LIQUID)
endif ()

find_package(PkgConfig) # imports pkg_check_modules() macros
find_package(LSBId REQUIRED)
find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)
find_package(Qt6Xml REQUIRED)
find_package(Qt6Sql REQUIRED)
find_package(Qt6Multimedia REQUIRED)

find_package(FFTW3f)
if (NOT FFTW3F_FOUND)
    message(FATAL_ERROR "Please install FFTW3")
endif ()
include_directories(${FFTW3F_INCLUDE_DIRS})
list(APPEND extraLibs ${FFTW3F_LIBRARIES})

# Qwt6.3.0 built with Qt6 has an unusual installation path for its Qt6Qwt6.pc file, so add it here
if (WIN32)
    # set(ENV{PKG_CONFIG_PATH} "C:/Qwt-6.3.0/:$ENV{PKG_CONFIG_PATH}")
    # set(Qt6Qwt6_DIR "C:/Qwt-6.3.0/")
    find_package(Qwt REQUIRED) # environment variable QWT_ROOT has to be defined!
    include_directories(${QWT_INCLUDE_DIRS})
    include_directories(../dabstar-libs/include)
    list(APPEND extraLibs ${QWT_LIBRARIES})
else ()
    set(ENV{PKG_CONFIG_PATH} "/usr/local/qwt-6.3.0/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    search_for_library(QWT Qt6Qwt6) # maybe try change this to Qwt instead of Qt6Qwt6
endif ()

search_for_library(ZLIB zlib)
search_for_library(LIBSNDFILE sndfile)

if (SSE_OR_AVX)
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/support/simd_extensions.h
            src/ofdm/ofdm-decoder-simd.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/ofdm/ofdm-decoder-simd.cpp
    )
    find_package(Volk REQUIRED)
    include_directories(${VOLK_INCLUDE_DIRS})
    list(APPEND extraLibs ${VOLK_LIBRARIES})
    add_definitions(-DHAVE_SSE_OR_AVX)
    # print_all_vars(VOLK)
else ()
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/ofdm/ofdm-decoder.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/ofdm/ofdm-decoder.cpp
    )
endif ()

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
        src/decoder
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
        src/audio
        src/scopes
        src/spectrum-viewer
        src/file-devices/xml-filewriter
        src/eti-handler
        src/service-list
        src/configuration
        src/update
        #/usr/include/
)

set(${objectName}_HDRS
        ${${objectName}_HDRS}
        src/main/dabradio.h
        src/main/glob_defs.h
        src/main/glob_enums.h
        src/main/dab-processor.h
        src/main/dab-constants.h
        src/main/mot-content-types.h
        src/eti-handler/eti-generator.h
        src/main/bit-extractors.h
        src/ofdm/sample-reader.h
        src/ofdm/phasereference.h
        src/ofdm/phasetable.h
        src/ofdm/freq-interleaver.h
        src/decoder/fib-decoder_if.h
        src/decoder/fib-decoder.h
        src/decoder/fib-config-fig0.h
        src/decoder/fib-config-fig1.h
        src/decoder/fib-table.h
        src/decoder/fic-decoder.h
        src/decoder/fib-helper.h
        src/ofdm/tii-detector.h
        src/ofdm/timesyncer.h
        src/protection/protTables.h
        src/protection/protection.h
        src/protection/uep-protection.h
        src/protection/eep-protection.h
        src/backend/firecode-checker.h
        src/backend/crc.h
        src/backend/frame-processor.h
        src/backend/charsets.h
        src/backend/galois.h
        src/backend/reed-solomon.h
        src/backend/msc-handler.h
        src/backend/backend.h
        src/backend/backend-deconvolver.h
        src/backend/backend-driver.h
        src/backend/audio/mp4processor.h
        src/backend/audio/bit-writer.h
        src/backend/audio/mp2processor.h
        src/backend/data/ip-datahandler.h
        src/backend/data/tdc-datahandler.h
        src/backend/data/journaline-datahandler.h
        src/backend/data/journaline-viewer.h
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
        src/audio/audiofifo.h
        src/audio/audiooutput.h
        src/audio/audioiodevice.h
        src/audio/audiooutputqt.h
        src/support/fir-filters.h
        src/support/ringbuffer.h
        src/support/Xtan2.h
        src/support/band-handler.h
        src/support/dab-tables.h
        src/support/tii_list_display.h
        src/support/viterbi-spiral/viterbi-spiral.h
        src/support/color-selector.h
        src/support/time-table.h
        src/support/openfiledialog.h
        src/support/content-table.h
        src/support/dl-cache.h
        src/support/ITU_Region_1.h
        src/support/coordinates.h
        src/support/mapport.h
        src/support/map-http-server.h
        src/support/tii-library/tii-codes.h
        src/support/buttons/newpushbutton.h
        src/support/buttons/normalpushbutton.h
        src/support/buttons/circlepushbutton.h
        src/support/custom_frame.h
        src/support/setting-helper.h
        src/support/wav_writer.h
        src/support/compass_direction.h
        src/support/time_meas.h
        src/support/copyright_info.h
        src/scopes/iqdisplay.h
        src/scopes/carrier-display.h
        src/scopes/audio-display.h
        src/scopes/spectrogramdata.h
        src/scopes/cust_qwt_zoom_pan.h
        src/spectrum-viewer/spectrum-viewer.h
        src/spectrum-viewer/cir-viewer.h
        src/spectrum-viewer/spectrum-scope.h
        src/spectrum-viewer/waterfall-scope.h
        src/spectrum-viewer/correlation-viewer.h
        src/file-devices/xml-filewriter/xml-filewriter.h
        src/service-list/service-list-handler.h
        src/service-list/service-db.h
        src/configuration/configuration.h
        src/update/updatechecker.h
        src/update/updatedialog.h
)

set(${objectName}_SRCS
        ${${objectName}_SRCS}
        src/main/main.cpp
        src/main/dabradio.cpp
        src/main/dabradio_ui.cpp
        src/main/dabradio_audio.cpp
        src/main/dab-processor.cpp
        src/support/techdata.cpp
        src/eti-handler/eti-generator.cpp
        src/ofdm/sample-reader.cpp
        src/ofdm/phasereference.cpp
        src/ofdm/phasetable.cpp
        src/ofdm/freq-interleaver.cpp
        src/decoder/fib-config-fig0.cpp
        src/decoder/fib-config-fig1.cpp
        src/decoder/fib-decoder.cpp
        src/decoder/fib-decoder-fig0.cpp
        src/decoder/fib-decoder-fig1.cpp
        src/decoder/fib-decoder-string-getter.cpp
        src/decoder/fic-decoder.cpp
        src/decoder/fib-helper.cpp
        src/ofdm/tii-detector.cpp
        src/ofdm/timesyncer.cpp
        src/protection/protTables.cpp
        src/protection/protection.cpp
        src/protection/eep-protection.cpp
        src/protection/uep-protection.cpp
        src/backend/firecode-checker.cpp
        src/backend/crc.cpp
        src/backend/charsets.cpp
        src/backend/galois.cpp
        src/backend/reed-solomon.cpp
        src/backend/msc-handler.cpp
        src/backend/backend.cpp
        src/backend/backend-deconvolver.cpp
        src/backend/backend-driver.cpp
        src/backend/audio/mp4processor.cpp
        src/backend/audio/bit-writer.cpp
        src/backend/audio/mp2processor.cpp
        src/backend/data/ip-datahandler.cpp
        src/backend/data/journaline-datahandler.cpp
        src/backend/data/journaline-viewer.cpp
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
        src/audio/audioiodevice.cpp
        src/audio/audiooutputqt.cpp
        src/support/fir-filters.cpp
        src/support/ringbuffer.cpp
        src/support/Xtan2.cpp
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
        src/support/dl-cache.cpp
        src/support/ITU_Region_1.cpp
        src/support/coordinates.cpp
        src/support/mapport.cpp
        src/support/map-http-server.cpp
        src/support/tii_list_display.cpp
        src/support/tii-library/tii-codes.cpp
        src/support/custom_frame.cpp
        src/support/setting-helper.cpp
        src/support/wav_writer.cpp
        src/support/compass_direction.cpp
        src/support/copyright_info.cpp
        src/scopes/iqdisplay.cpp
        src/scopes/carrier-display.cpp
        src/scopes/audio-display.cpp
        src/scopes/spectrogramdata.cpp
        src/scopes/cust_qwt_zoom_pan.cpp
        src/spectrum-viewer/spectrum-viewer.cpp
        src/spectrum-viewer/spectrum-scope.cpp
        src/spectrum-viewer/waterfall-scope.cpp
        src/spectrum-viewer/correlation-viewer.cpp
        src/spectrum-viewer/cir-viewer.cpp
        src/file-devices/xml-filewriter/xml-filewriter.cpp
        src/service-list/service-list-handler.cpp
        src/service-list/service-db.cpp
        src/configuration/configuration.cpp
        src/update/updatechecker.cpp
        src/update/updatedialog.cpp
)

#########################################################################

if (FDK_AAC)
    find_package(Fdk-AAC)
    if (NOT LIBFDK_AAC_FOUND)
        message(FATAL_ERROR, "Please install libfdk-aac")
    endif ()
    include_directories(${FDK_AAC_INCLUDE_DIR}/fdk-aac)
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

if (VITERBI_AVX2)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/viterbi_16way.h
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    add_definitions(-DHAVE_VITERBI_AVX2)
elseif (VITERBI_SSE2)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/viterbi_8way.h
    )
    add_definitions(-DHAVE_VITERBI_SSE2)
elseif (VITERBI_NEON)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/sse2neon.h
    )
    add_definitions(-DHAVE_VITERBI_NEON)
else ()
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/support/viterbi-spiral/viterbi_scalar.h
    )
endif ()

if (DATA_STREAMER)
    include_directories(
            src/server-thread
    )

    set(${objectName}_HDRS
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
        Qt6::Xml
        Qt6::Sql
        Qt6::Multimedia
        ${extraLibs}
        ${FAAD_LIBRARIES}
        ${CMAKE_DL_LIBS}
)

########################################################################
# Add subsequent libraries
########################################################################

add_subdirectory(src/devices)

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
