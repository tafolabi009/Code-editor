# MinGW-w64 cross-compilation toolchain file for x86_64 Windows
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake ..
#

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Target environment location
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Adjust find commands to search for programs on the host
# but libraries and headers in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable ASM for cross-compilation (Linux NASM != Windows NASM format)
set(ENABLE_ASM OFF CACHE BOOL "Disable ASM for cross-compilation" FORCE)

# Set Windows-specific flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Enable static linking for portability
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

# Windows version targeting (Windows 7+)
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DWINVER=0x0601)
