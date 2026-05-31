# CMake toolchain file for cross-compiling DABstar to Windows x86_64 with MinGW-w64
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchains/mingw-w64-x86_64.cmake ...

set(CMAKE_SYSTEM_NAME    Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# ---------------------------------------------------------------------------
# Cross-compiler executables (from mingw-w64 Ubuntu package)
# ---------------------------------------------------------------------------
set(CROSS_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   NAMES ${CROSS_PREFIX}-gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${CROSS_PREFIX}-g++   REQUIRED)
find_program(CMAKE_RC_COMPILER  NAMES ${CROSS_PREFIX}-windres)
find_program(CMAKE_AR           NAMES ${CROSS_PREFIX}-ar)
find_program(CMAKE_RANLIB       NAMES ${CROSS_PREFIX}-ranlib)

# ---------------------------------------------------------------------------
# Sysroot – supplied via -DCMAKE_FIND_ROOT_PATH=... on the cmake command line
# (set by cross-compile-windows.sh)
# ---------------------------------------------------------------------------

# Search behaviour:
#   NEVER  – always use the host for programs (moc, uic, rcc stay on Linux)
#   ONLY   – only look inside the target sysroot for libs / includes / packages
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Prefer static libs to reduce runtime DLL count for self-contained deps
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dll.a" ".lib")
