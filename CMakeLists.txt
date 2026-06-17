cmake_minimum_required(VERSION 3.20)

project(
    VehicleRentalSystem
    VERSION 1.0.0
    DESCRIPTION "Vehicle rental management system"
    LANGUAGES CXX
)

# global language standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json for clangd/tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Build type not specified — defaulting to Release")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# route binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# install support
include(GNUInstallDirs)

# fetch dependencies
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# disable clang-tidy on fetchcontent dependencies
file(WRITE "${CMAKE_BINARY_DIR}/_deps/.clang-tidy" "Checks: '-*'\n")

# system dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    if(MINGW)
        set(PKG_CONFIG_STATIC ON)
        add_compile_definitions(NCURSES_STATIC)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs/mingw64/lib")
    endif()
    pkg_check_modules(NOTCURSES IMPORTED_TARGET notcurses)
else()
    if(WIN32 OR MINGW)
        message(WARNING "pkg-config not found. TUI targets (RentalSystem, demo_notui) will be skipped. If you are using MSYS2/MinGW, install it with: pacman -S mingw-w64-x86_64-pkgconf")
    endif()
endif()

if(PkgConfig_FOUND AND NOT NOTCURSES_FOUND)
    if(WIN32 OR MINGW)
        message(WARNING "notcurses not found. TUI targets (RentalSystem, demo_notui) will be skipped. If you are using MSYS2/MinGW, install it with: pacman -S mingw-w64-x86_64-notcurses")
    endif()
endif()

# static analysis
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CMAKE_CROSSCOMPILING)
    set(CLANG_TIDY_EXE OFF)
endif()
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not found — static analysis disabled")
endif()

# helper to enable clang-tidy
function(enable_clang_tidy target)
    if(CLANG_TIDY_EXE)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY
            # semicolon-separated arguments
            "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        )
    endif()
endfunction()

# subdirectories
if(NOTCURSES_FOUND)
    add_subdirectory(libs/notui)
endif()
add_subdirectory(src)

# testing
include(CTest)
enable_testing()

# list test sources explicitly
set(TEST_SOURCES
    tests/test_vehicle.cpp
    tests/test_filestorage.cpp
    tests/test_fleet.cpp
    tests/test_codegen.cpp
    tests/test_concurrency.cpp
    tests/test_storage_failures.cpp
    tests/test_time_anomalies.cpp
    tests/test_validation.cpp
)
if(NOTCURSES_FOUND)
    list(APPEND TEST_SOURCES tests/test_notui.cpp)
endif()
if(MINGW)
    list(APPEND TEST_SOURCES libs/notui/src/WinCompat.cpp)
endif()

add_executable(UnitTests ${TEST_SOURCES})


target_include_directories(UnitTests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/tests"
)

target_link_libraries(UnitTests PRIVATE
    rental_engine
    Catch2::Catch2WithMain
)
if(NOTCURSES_FOUND)
    target_link_libraries(UnitTests PRIVATE notui)
endif()

if(MINGW)
    target_link_options(UnitTests PRIVATE -static)
    target_link_libraries(UnitTests PRIVATE pthread)
endif()

# ctest excluded from clang-tidy to avoid false-positives
add_test(NAME UnitTests COMMAND UnitTests)

# install rules
# install application binary
if(NOTCURSES_FOUND)
    install(
        TARGETS RentalSystem
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Application
    )
endif()

# install data skeleton
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    COMPONENT Application
    OPTIONAL  # data dir optional if not exists
    FILES_MATCHING PATTERN ".gitkeep"
)

# create records dir at install
install(
    DIRECTORY DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/records"
    COMPONENT Application
)

# ==============================================================================
# 📦 RUNTIME DYNAMIC PLUGIN EXTENSION (PRESENTATION EXCLUSIVE)
# ==============================================================================

# 1. Register the plugin target as a MODULE library (specifically optimized for dlopen)
add_library(custom_printer_plugin MODULE src/plugins/CustomPrinter.cpp)

# 2. Grant the plugin explicit access to interface definitions (like IPrinter.h)
target_include_directories(custom_printer_plugin PRIVATE "${CMAKE_SOURCE_DIR}/include")

# 3. CRITICAL: Isolate this target so a normal build cycle completely ignores it
set_target_properties(custom_printer_plugin PROPERTIES 
    EXCLUDE_FROM_ALL TRUE
    # Automatically drops the compiled .so / .dll inside your live bin/ directory
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

# 4. Enforce your strict static analysis styling onto the plugin codebase
enable_clang_tidy(custom_printer_plugin)

if(MINGW)
    target_link_options(custom_printer_plugin PRIVATE -static)
endif()

# 5. Register install rule specifically for the plugin component
install(
    TARGETS custom_printer_plugin
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins"
    COMPONENT Plugins
)

# 6. Custom targets for component installation (to support Ninja and workflows)
add_custom_target(install-app
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --component Application
    COMMENT "Installing application component..."
)

add_custom_target(install-plugin
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --component Plugins
    COMMENT "Installing plugins component..."
)

# 7. Uninstall target configuration
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        COMMENT "Uninstalling installed files..."
    )
endif()