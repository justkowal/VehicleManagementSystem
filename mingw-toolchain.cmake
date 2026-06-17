# mingw-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Force CMake to utilize the native Linux cross-compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Adjust the target environment lookups
list(APPEND CMAKE_FIND_ROOT_PATH "/usr/x86_64-w64-mingw32" "${CMAKE_CURRENT_LIST_DIR}/libs/mingw64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Configure PKG_CONFIG environment for cross-compiling
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_CURRENT_LIST_DIR}/libs/mingw64/lib/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")

# Force find_library to only resolve static libraries (.a) to ensure static linking
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.lib")

