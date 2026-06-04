cmake_minimum_required(VERSION 3.14)
project(VehicleRentalSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. Automatically find and run clang-tidy during compilation!
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
else()
    message(WARNING "clang-tidy not found! Static analysis disabled.")
endif()

# Discover local system libraries for notcurses
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)

add_executable(test_notcurses test_notcurses.cpp)

target_include_directories(test_notcurses PRIVATE ${CMAKE_SOURCE_DIR}/include ${NOTCURSES_INCLUDE_DIRS})
target_link_libraries(test_notcurses PRIVATE ${NOTCURSES_LIBRARIES})