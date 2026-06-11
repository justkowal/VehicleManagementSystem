cmake_minimum_required(VERSION 3.20)

project(
    VehicleRentalSystem
    VERSION 1.0.0
    DESCRIPTION "Vehicle rental management system"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Global language standard — must be set before any target definitions
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Always emit compile_commands.json (used by clang-tidy, clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the default build type if none was specified.
# Users can select the build type from standard options (Debug, Release, RelWithDebInfo, MinSizeRel)
# by setting -DCMAKE_BUILD_TYPE on the command line or choosing from the IDE list.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Build type not specified — defaulting to Release")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Route all compiled executables to build/bin/ for a clean layout
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# Install support — standard directory variables (bindir, datadir, etc.)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# FetchContent — declared early so subdirectories can depend on it
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Suppress clang-tidy for FetchContent deps: write a local .clang-tidy
# that disables all checks. clang-tidy resolves config by walking up the
# directory tree, so this file takes precedence for anything under _deps/.
file(WRITE "${CMAKE_BINARY_DIR}/_deps/.clang-tidy" "Checks: '-*'\n")

# ---------------------------------------------------------------------------
# System dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)

# ---------------------------------------------------------------------------
# Static analysis — discover clang-tidy once, apply per-target below
# ---------------------------------------------------------------------------
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not found — static analysis disabled")
endif()

# Helper function: opt a target into clang-tidy analysis when available
function(enable_clang_tidy target)
    if(CLANG_TIDY_EXE)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY
            # Semicolon-separated: executable + arguments
            "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        )
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(libs/notui)
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Testing
# ---------------------------------------------------------------------------
include(CTest)
enable_testing()

# Explicitly list test sources — avoids CMake's stale-glob footgun where
# adding/removing a file requires a manual re-configure to be picked up.
add_executable(UnitTests
    tests/test_vehicle.cpp
    tests/test_filestorage.cpp
    tests/test_fleet.cpp
    tests/test_codegen.cpp
    tests/test_concurrency.cpp
    tests/test_notui.cpp
    tests/test_storage_failures.cpp
    tests/test_time_anomalies.cpp
)

target_include_directories(UnitTests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/tests"
)

target_link_libraries(UnitTests PRIVATE
    rental_engine
    notui
    Catch2::Catch2WithMain
)

# Tests intentionally excluded from clang-tidy: Catch2 macros generate
# patterns that produce false-positive diagnostics.
add_test(NAME UnitTests COMMAND UnitTests)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
# Install the main application binary
install(
    TARGETS RentalSystem
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Install an empty data skeleton so the install prefix contains the
# records/ subdirectory. Runtime data is written to the OS user-data
# directory (XDG_DATA_HOME on Linux, %APPDATA% on Windows) and is NOT
# stored under the install prefix.
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    OPTIONAL  # The data/ dir may not exist yet in a fresh checkout
    FILES_MATCHING PATTERN ".gitkeep"
)

# Ensure the records/ subdirectory is created at install time
install(
    DIRECTORY DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/records"
)