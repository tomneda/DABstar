cmake_minimum_required(VERSION 3.16...3.30)

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
project(DABstar VERSION 5.3.0)
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
set(CMAKE_AUTOUIC OFF)
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
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math")

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
    configure_file("${PROJECT_SOURCE_DIR}/scripts/appimage_creator/dabstar.desktop.in"
                   "${PROJECT_SOURCE_DIR}/scripts/appimage_creator/dabstar.desktop" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/scripts/appimage_creator/create_appimage.sh.in"
                   "${PROJECT_SOURCE_DIR}/scripts/appimage_creator/create_appimage.sh" @ONLY)
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
option(SDRPLAY    "Use SDRplay"    OFF)
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
if (WIN32)
    include_directories(${CMAKE_SOURCE_DIR}/../dabstar-libs/include)
endif ()

if (USE_LIQUID)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/halfbandfilter.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/support/halfbandfilter.cpp
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
find_package(Qt6Charts REQUIRED)

set(main_ui
        forms/cir_widget.ui
        forms/configuration.ui
        forms/dabradio.ui
        forms/dumpwidget.ui
        forms/spectrum_viewer.ui
        forms/techdata.ui
        forms/updatedialog.ui
        forms/ensemble_list.ui
)

find_package(FFTW3F)
if (NOT FFTW3F_FOUND)
    message(FATAL_ERROR "Please install FFTW3")
endif ()
include_directories(${FFTW3F_INCLUDE_DIRS})
list(APPEND extraLibs ${FFTW3F_LIBRARIES})

search_for_library(ZLIB zlib)
search_for_library(LIBSNDFILE sndfile)

if (SSE_OR_AVX)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/simd_extensions.h
            src/base/ofdm/ofdm_decoder_simd.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/ofdm/ofdm_decoder_simd.cpp
    )
    find_package(Volk REQUIRED)
    include_directories(${VOLK_INCLUDE_DIRS})
    list(APPEND extraLibs ${VOLK_LIBRARIES})
    add_definitions(-DHAVE_SSE_OR_AVX)
    # print_all_vars(VOLK)
else ()
    set($(objectName)_HDRS
            ${${objectName}_HDRS}
            src/base/ofdm/ofdm_decoder.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/ofdm/ofdm_decoder.cpp
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
        src/base/main
        src/base/ofdm
        src/base/decoder
        src/base/protection
        src/base/backend
        src/base/backend/audio
        src/base/backend/data
        src/base/backend/data/journaline
        src/base/backend/data/mot
        src/base/backend/data/epg_2
        src/base/backend/data/epg
        src/base/support
        src/base/support/tii_library
        src/base/support/buttons
        src/base/support/viterbi_spiral
        src/base/audio
        src/base/scopes
        src/base/spectrum_viewer
        src/base/file_devices/xml_filewriter
        src/base/eti_handler
        src/base/service_list
        src/base/configuration
        src/base/ensemble_list
        src/base/update
        #/usr/include/
)

set(${objectName}_HDRS
        ${${objectName}_HDRS}
        src/base/main/dabradio.h
        src/base/main/audio_manager.h
        src/base/main/epg_mot_handler.h
        src/base/main/tii_manager.h
        src/base/main/gap_progress_bar.h
        src/base/main/dab_channel_desc.h
        src/base/main/glob_defs.h
        src/base/main/glob_enums.h
        src/base/main/dab_processor.h
        src/base/main/dab_constants.h
        src/base/main/mot_content_types.h
        src/base/main/mot_slide_progress.h
        src/base/eti_handler/eti_generator.h
        src/base/main/bit_extractors.h
        src/base/ofdm/sample_reader.h
        src/base/ofdm/phasereference.h
        src/base/ofdm/phasetable.h
        src/base/ofdm/freq_interleaver.h
        src/base/decoder/fib_decoder_if.h
        src/base/decoder/fib_decoder.h
        src/base/decoder/fib_config_fig0.h
        src/base/decoder/fib_config_fig1.h
        src/base/decoder/fib_table.h
        src/base/decoder/fic_decoder.h
        src/base/decoder/fib_helper.h
        src/base/ofdm/tii_detector.h
        src/base/ofdm/timesyncer.h
        src/base/protection/protTables.h
        src/base/protection/protection.h
        src/base/protection/uep_protection.h
        src/base/protection/eep_protection.h
        src/base/backend/firecode_checker.h
        src/base/backend/crc.h
        src/base/backend/frame_processor.h
        src/base/backend/charsets.h
        src/base/backend/galois.h
        src/base/backend/reed_solomon.h
        src/base/backend/msc_handler.h
        src/base/backend/backend.h
        src/base/backend/backend_deconvolver.h
        src/base/backend/backend_driver.h
        src/base/backend/audio/mp4processor.h
        src/base/backend/audio/bit_writer.h
        src/base/backend/audio/mp2processor.h
        src/base/backend/data/ip_datahandler.h
        src/base/backend/data/tdc_datahandler.h
        src/base/backend/data/journaline_datahandler.h
        src/base/backend/data/journaline_viewer.h
        src/base/backend/data/journaline/dabdatagroupdecoder.h
        src/base/backend/data/journaline/crc_8_16.h
        src/base/backend/data/journaline/log.h
        src/base/backend/data/journaline/newssvcdec_impl.h
        src/base/backend/data/journaline/Splitter.h
        src/base/backend/data/journaline/dabdgdec_impl.h
        src/base/backend/data/journaline/newsobject.h
        src/base/backend/data/journaline/NML.h
        src/base/backend/data/epg/epgdec.h
        src/base/backend/data/epg_2/epg_decoder.h
        src/base/backend/data/virtual_datahandler.h
        src/base/backend/data/pad_handler.h
        src/base/backend/data/mot/mot_handler.h
        src/base/backend/data/mot/mot_object.h
        src/base/backend/data/mot/mot_dir.h
        src/base/backend/data/data_processor.h
        src/base/audio/audiofifo.h
        src/base/audio/audiooutput.h
        src/base/audio/audioiodevice.h
        src/base/audio/test_tone.h
        src/base/audio/audiooutputqt.h
        src/base/support/fir_filters.h
        src/base/support/ringbuffer.h
        src/base/support/Xtan2.h
        src/base/support/band_handler.h
        src/base/support/dab_tables.h
        src/base/support/tii_list_display.h
        src/base/support/viterbi_spiral/viterbi_spiral.h
        src/base/support/color_selector.h
        src/base/support/time_table.h
        src/base/support/openfiledialog.h
        src/base/support/content_table.h
        src/base/support/dl_cache.h
        src/base/support/itu_regions.h
        src/base/support/map_http_server.h
        src/base/support/tii_library/tii_codes.h
        src/base/support/custom_frame.h
        src/base/support/gui_helpers.h
        src/base/support/setting_helper.h
        src/base/support/wav_writer.h
        src/base/support/compass_direction.h
        src/base/support/time_meas.h
        src/base/support/copyright_info.h
        src/base/scopes/iqdisplay.h
        src/base/scopes/carrier_display.h
        src/base/scopes/audio_display.h
        src/base/scopes/plot_widget.h
        src/base/scopes/level_meter.h
        src/base/spectrum_viewer/spectrum_viewer.h
        src/base/spectrum_viewer/cir_viewer.h
        src/base/spectrum_viewer/spectrum_scope.h
        src/base/spectrum_viewer/waterfall_scope.h
        src/base/spectrum_viewer/correlation_viewer.h
        src/base/file_devices/xml_filewriter/xml_filewriter.h
        src/base/service_list/service_list_handler.h
        src/base/service_list/service_db.h
        src/base/configuration/configuration.h
        src/base/ensemble_list/ensemble_list.h
        src/base/ensemble_list/ensemble_list_db.h
        src/base/ensemble_list/ensemble_list_db_handler.h
        src/base/update/updatechecker.h
        src/base/update/updatedialog.h
)

set(${objectName}_SRCS
        ${${objectName}_SRCS}
        src/base/main/main.cpp
        src/base/main/dabradio.cpp
        src/base/main/dabradio_ui.cpp
        src/base/main/dabradio_dump.cpp
        src/base/main/audio_manager.cpp
        src/base/main/epg_mot_handler.cpp
        src/base/main/tii_manager.cpp
        src/base/main/dabradio_el.cpp
        src/base/main/dabradio_ctrl.cpp
        src/base/main/dab_channel_desc.cpp
        src/base/main/dab_processor.cpp
        src/base/support/techdata.cpp
        src/base/main/mot_slide_progress.cpp
        src/base/eti_handler/eti_generator.cpp
        src/base/ofdm/sample_reader.cpp
        src/base/ofdm/phasereference.cpp
        src/base/ofdm/phasetable.cpp
        src/base/ofdm/freq_interleaver.cpp
        src/base/decoder/fib_config_fig0.cpp
        src/base/decoder/fib_config_fig1.cpp
        src/base/decoder/fib_decoder.cpp
        src/base/decoder/fib_decoder_fig0.cpp
        src/base/decoder/fib_decoder_fig1.cpp
        src/base/decoder/fib_decoder_string_getter.cpp
        src/base/decoder/fic_decoder.cpp
        src/base/decoder/fib_helper.cpp
        src/base/ofdm/tii_detector.cpp
        src/base/ofdm/timesyncer.cpp
        src/base/protection/protTables.cpp
        src/base/protection/protection.cpp
        src/base/protection/eep_protection.cpp
        src/base/protection/uep_protection.cpp
        src/base/backend/firecode_checker.cpp
        src/base/backend/crc.cpp
        src/base/backend/charsets.cpp
        src/base/backend/galois.cpp
        src/base/backend/reed_solomon.cpp
        src/base/backend/msc_handler.cpp
        src/base/backend/backend.cpp
        src/base/backend/backend_deconvolver.cpp
        src/base/backend/backend_driver.cpp
        src/base/backend/audio/mp4processor.cpp
        src/base/backend/audio/bit_writer.cpp
        src/base/backend/audio/mp2processor.cpp
        src/base/backend/data/ip_datahandler.cpp
        src/base/backend/data/journaline_datahandler.cpp
        src/base/backend/data/journaline_viewer.cpp
        src/base/backend/data/journaline/crc_8_16.c
        src/base/backend/data/journaline/log.c
        src/base/backend/data/journaline/newssvcdec_impl.cpp
        src/base/backend/data/journaline/Splitter.cpp
        src/base/backend/data/journaline/dabdgdec_impl.c
        src/base/backend/data/journaline/newsobject.cpp
        src/base/backend/data/journaline/NML.cpp
        src/base/backend/data/epg_2/epg_decoder.cpp
        src/base/backend/data/epg/epgdec.cpp
        src/base/backend/data/tdc_datahandler.cpp
        src/base/backend/data/pad_handler.cpp
        src/base/backend/data/mot/mot_handler.cpp
        src/base/backend/data/mot/mot_object.cpp
        src/base/backend/data/mot/mot_dir.cpp
        src/base/backend/data/data_processor.cpp
        src/base/audio/audioiodevice.cpp
        src/base/audio/test_tone.cpp
        src/base/audio/audiooutputqt.cpp
        src/base/support/fir_filters.cpp
        src/base/support/ringbuffer.cpp
        src/base/support/Xtan2.cpp
        src/base/support/band_handler.cpp
        src/base/support/dab_tables.cpp
        src/base/support/viterbi_spiral/viterbi_spiral.cpp
        src/base/support/color_selector.cpp
        src/base/support/time_table.cpp
        src/base/support/openfiledialog.cpp
        src/base/support/content_table.cpp
        src/base/support/dl_cache.cpp
        src/base/support/itu_regions.cpp
        src/base/support/map_http_server.cpp
        src/base/support/tii_list_display.cpp
        src/base/support/tii_library/tii_codes.cpp
        src/base/support/custom_frame.cpp
        src/base/support/gui_helpers.cpp
        src/base/support/setting_helper.cpp
        src/base/support/wav_writer.cpp
        src/base/support/compass_direction.cpp
        src/base/support/copyright_info.cpp
        src/base/scopes/iqdisplay.cpp
        src/base/scopes/carrier_display.cpp
        src/base/scopes/audio_display.cpp
        src/base/scopes/plot_widget.cpp
        src/base/scopes/level_meter.cpp
        src/base/spectrum_viewer/spectrum_viewer.cpp
        src/base/spectrum_viewer/spectrum_scope.cpp
        src/base/spectrum_viewer/waterfall_scope.cpp
        src/base/spectrum_viewer/correlation_viewer.cpp
        src/base/spectrum_viewer/cir_viewer.cpp
        src/base/file_devices/xml_filewriter/xml_filewriter.cpp
        src/base/service_list/service_list_handler.cpp
        src/base/service_list/service_db.cpp
        src/base/configuration/configuration.cpp
        src/base/ensemble_list/ensemble_list.cpp
        src/base/ensemble_list/ensemble_list_db.cpp
        src/base/ensemble_list/ensemble_list_db_handler.cpp
        src/base/update/updatechecker.cpp
        src/base/update/updatedialog.cpp
)

#########################################################################

if (FDK_AAC)
    find_package(Fdk-AAC)
    if (NOT LIBFDK_AAC_FOUND)
        message(FATAL_ERROR, "Please install libfdk-aac")
    endif ()
    include_directories(${FDK_AAC_INCLUDE_DIR}/fdk-aac)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/backend/audio/fdk_aac.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/backend/audio/fdk_aac.cpp
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
            src/base/backend/audio/faad_decoder.h
    )
    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/backend/audio/faad_decoder.cpp
    )
    add_definitions(-D__WITH_FAAD__)
endif (FDK_AAC)

if (VITERBI_AVX2)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/viterbi_spiral/viterbi_16way.h
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    add_definitions(-DHAVE_VITERBI_AVX2)
elseif (VITERBI_SSE2)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/viterbi_spiral/viterbi_8way.h
    )
    add_definitions(-DHAVE_VITERBI_SSE2)
elseif (VITERBI_NEON)
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/viterbi_spiral/sse2neon.h
    )
    add_definitions(-DHAVE_VITERBI_NEON)
else ()
    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/support/viterbi_spiral/viterbi_scalar.h
    )
endif ()

if (DATA_STREAMER)
    include_directories(
            src/base/server_thread
    )

    set(${objectName}_HDRS
            ${${objectName}_HDRS}
            src/base/server_thread/tcp_server.h
    )

    set(${objectName}_SRCS
            ${${objectName}_SRCS}
            src/base/server_thread/tcp_server.cpp
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

qt6_wrap_ui(${objectName}_UI_HDRS
        ${main_ui}
)

set(${objectName}_HDRS
        ${${objectName}_HDRS}
        ${${objectName}_UI_HDRS}
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
        Qt6::Charts
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
