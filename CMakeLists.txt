cmake_minimum_required(VERSION 3.16...3.30)

# set CMAKE_BUILD_TYPE to "Release" if not defined (like call from command line)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Set build type to Release if not specified" FORCE)
endif ()

### make sure our local CMake Modules path comes first (must be before any find_package calls)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/CMakeFunctions.cmake)

# First, we need to include vcpkg's auto-loader script, it sets up CMAKE_TOOLCHAIN_FILE
# IMPORTANT: this must be done for the call to project() ?!
try_find_vcpkg_toolset_path()
print(CMAKE_TOOLCHAIN_FILE)

##############################
project(DABstar VERSION 5.5.0)
##############################

set(objectName "dabstar")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_SANITIZERS "Enable address/undefined sanitizers in Debug builds" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_SANITIZERS)
  if (NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  else ()
    message(WARNING "USE_SANITIZERS is not supported with MSVC, ignoring")
  endif ()
endif ()

option(USE_LTO "Set -flto flag" OFF)  # switch off to save compile/link time while development

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

# special release version flags
if (USE_LTO AND NOT MSVC)
  include(ProcessorCount)
  ProcessorCount(N)
  if (N GREATER 1)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=${N}")
  else ()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto")
  endif ()

  # LTO produces slim objects that plain ar/ranlib cannot index ("plugin
  # needed to handle lto object" -> undefined references from static libs).
  # Use the compiler's plugin-aware wrappers (gcc-ar/gcc-ranlib on GCC,
  # llvm-ar/llvm-ranlib on Clang). CMake fills these in on Linux and MinGW.
  if (CMAKE_CXX_COMPILER_AR)
    set(CMAKE_AR "${CMAKE_CXX_COMPILER_AR}")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
  endif ()
  if (CMAKE_CXX_COMPILER_RANLIB)
    set(CMAKE_RANLIB "${CMAKE_CXX_COMPILER_RANLIB}")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
  endif ()
endif ()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DQT_NO_DEBUG_OUTPUT") # inhibits qDebug outputs in release version

# common compiler flags (platform-specific)
if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /we4716 /fp:fast /D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=return-type -ffast-math -fsingle-precision-constant")
endif ()

# as MSVC compiler does not like VLAs (Variable Length Array) inhibit them for GCC also
if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wvla -Werror=vla)
endif ()

# Linux-only: override install prefix default
if (UNIX AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
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
  add_compile_options(-municode)
endif ()

# handle GIT hash
get_git_commit_hash() # returns GIT_COMMIT_HASH
print(GIT_COMMIT_HASH)
if (GIT_COMMIT_HASH)
  add_definitions("-DGITHASH=\"${GIT_COMMIT_HASH}\"")
else ()
  add_definitions("-DGITHASH=\"(unknown)\"")
endif ()

print(CMAKE_MODULE_PATH)

######################################################################
######################################################################

# select one or more devices (set to ON)
# attention: option() does not overwrite the CMake cache entries, so use CMake command line or your IDE to overwrite this
# As I (tomneda) possess not all of these devices I place an "untested" behind. They may run, or not run.
option(AIRSPY "Use AirSpy" OFF)  # untested
option(SPYSERVER "Use SpyServer" OFF)  # untested
option(SDRPLAY "Use SDRplay" OFF)
option(HACKRF "Use HackRf" OFF)
option(LIMESDR "Use LimeSdr" OFF)  # untested
option(RTL_TCP "Use RtlTcp" OFF)
option(PLUTO "Use Pluto" OFF)  # untested
option(UHD "Use UHD" OFF)
option(RTLSDR "Use RtlSdr" ON)
option(SOAPY "Use SoapySdr" OFF)  # partly untested


# additional options
option(USE_LIQUID "Use LiquidDsp for HBF and Resampler" OFF)  # this needs LiquidDsp (https://github.com/jgaeddert/liquid-dsp)
option(DATA_STREAMER "Use DataStreamer" OFF)  # untested
option(FDK_AAC "Use FDK-AAC instead of FAAD" ON)

# choose only one of the next three, otherwise the first ON-entry is valid
option(VITERBI_AVX2 "Use AVX2 for Viterbi" OFF)  # fastest, for modern Intel/AMD processors
option(VITERBI_SSE2 "Use SSE2 for Viterbi" OFF)  # quite fast, for bit older Intel/AMD processors
option(VITERBI_NEON "Use NEON for Viterbi" OFF)  # for ARM processors like Raspi (not tested)

# there are other places like the OFDM decoder which can use SSE/AVX
option(SSE_OR_AVX "Use SSE or AVX for OFDM and others beside Viterbi" OFF)  # needs VOLK (https://github.com/gnuradio/volk)

######################################################################
######################################################################

# ---------------------------------------------------------------------------
# Feature switches that change the contents/layout of headers shared across
# targets (dabradio.h, dab_processor.h, ofdm_decoder*.h, device headers, ...).
# They must be defined identically for EVERY translation unit - the executable
# (main.cpp) as well as all libraries - otherwise different TUs see different
# class definitions, which violates the ODR (LTO reports this as -Wodr /
# -Wlto-type-mismatch). Defining them here, before add_executable() and
# add_subdirectory(), guarantees that. The matching source-file / find_package()
# selection stays keyed on the same option() in each subdirectory.
# ---------------------------------------------------------------------------
if (USE_LIQUID)
  add_compile_definitions(HAVE_LIQUID)
endif ()
if (SSE_OR_AVX)
  add_compile_definitions(HAVE_SSE_OR_AVX)
endif ()
if (DATA_STREAMER)
  add_compile_definitions(DATA_STREAMER)
endif ()
if (FDK_AAC)
  add_compile_definitions(__WITH_FDK_AAC__)
else ()
  add_compile_definitions(__WITH_FAAD__)
endif ()
# choose only one - the first ON entry wins (matches the Viterbi source selection)
if (VITERBI_AVX2)
  add_compile_definitions(HAVE_VITERBI_AVX2)
elseif (VITERBI_SSE2)
  add_compile_definitions(HAVE_VITERBI_SSE2)
elseif (VITERBI_NEON)
  add_compile_definitions(HAVE_VITERBI_NEON)
endif ()

#add_definitions(-D__THREADED_BACKEND__) # uncomment for use for an RPI
#add_definitions (-D__EPG_TRACE__)

########################################################################
if (WIN32)
  # SYSTEM: these are bundled third-party headers (liquid-dsp, ...) we do not
  # control. Marking the directory as a system include makes the compiler
  # suppress their warnings (e.g. liquid's -Wdeprecated-declarations) while still
  # fully warning on our own code - matching the Linux behaviour where these libs
  # are found in a system location.
  include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/../dabstar-libs/include)
endif ()

find_package(PkgConfig) # imports pkg_check_modules() macros

# LSBId is Linux-only (uses lsb_release to detect the distribution)
if (UNIX)
  find_package(LSBId REQUIRED)
endif ()

find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6Network REQUIRED)
find_package(Qt6Xml REQUIRED)
find_package(Qt6Sql REQUIRED)
find_package(Qt6Multimedia REQUIRED)
find_package(Qt6Charts REQUIRED)

if (UNIX AND NOT APPLE)
  find_library(PTHREADS pthread)
  if (NOT PTHREADS)
    message(FATAL_ERROR "Please install libpthread")
  else ()
    set(extraLibs ${extraLibs} ${PTHREADS})
  endif ()
endif ()

print(CMAKE_SOURCE_DIR)
print(CMAKE_CURRENT_BINARY_DIR)

# NOTE: main.cpp must live in the executable target itself, NOT in a static
# library. With LTO enabled (-flto) the linker will not extract the entry point
# from a static archive, which fails as "undefined reference to WinMain/main"
# (seen on MinGW, but the hazard applies to GCC/Clang LTO on Linux too).
add_executable(${objectName}
        resources.qrc
        src/base/main/main.cpp
)

########################################################################
# Add subsequent libraries
########################################################################

add_subdirectory(src)

target_link_libraries(${objectName}
        PRIVATE
        ${objectName}_base
        ${objectName}_devices
        ${extraLibs}
)

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

########################################################################
# Create the PDF version of the user manual (not built by default)
#
#   cmake --build <build-dir> --target manual
#
# Needs pandoc and XeLaTeX, see doc/manual/README.md
########################################################################

find_program(PANDOC_EXECUTABLE pandoc)

if (UNIX AND PANDOC_EXECUTABLE)
  # called via bash so that it does not depend on the executable bit of the checkout
  add_custom_target(manual
          COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/build-manual.sh
                  ${CMAKE_CURRENT_BINARY_DIR}/DABstar-Manual.pdf
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMENT "Building the DABstar user manual PDF"
          VERBATIM
          USES_TERMINAL)
elseif (UNIX)
  message(STATUS "pandoc not found -> target 'manual' (user manual PDF) is not available")
endif ()
