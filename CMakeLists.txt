cmake_minimum_required(VERSION 3.14)
project(VehicleRentalSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
else()
    message(WARNING "clang-tidy not found! Static analysis disabled.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED notcurses)

add_subdirectory(src)

# --- Testing Framework (Catch2) ---

set(CMAKE_CXX_CLANG_TIDY "")

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2 
)
FetchContent_MakeAvailable(Catch2)

file(GLOB TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(UnitTests ${TEST_SOURCES})

target_include_directories(UnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(UnitTests PRIVATE rental_engine Catch2::Catch2WithMain)

include(CTest)
add_test(NAME FleetLogicTests COMMAND UnitTests)