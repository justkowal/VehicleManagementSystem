cmake_minimum_required(VERSION 3.20)

project(
    VehicleManagementSystem
    VERSION 1.3.0
    DESCRIPTION "Vehicle rental management system"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Build type not specified — defaulting to Release")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(GNUInstallDirs)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

file(WRITE "${CMAKE_BINARY_DIR}/_deps/.clang-tidy" "Checks: '-*'\n")

find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not found — static analysis disabled")
endif()

function(enable_clang_tidy target)
    if(CLANG_TIDY_EXE)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY

            "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        )
    endif()
endfunction()

add_subdirectory(libs/notui)
add_subdirectory(src)

include(CTest)
enable_testing()

add_executable(UnitTests
    tests/test_vehicle.cpp
    tests/test_filestorage.cpp
    tests/test_fleet.cpp
    tests/test_codegen.cpp
    tests/test_concurrency.cpp
    tests/test_notui.cpp
    tests/test_storage_failures.cpp
    tests/test_time_anomalies.cpp
    tests/test_validation.cpp
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

target_compile_options(UnitTests PRIVATE
    $<$<CONFIG:Debug>:
        -Wall
        -Wextra
        -Wshadow
    >
    $<$<CONFIG:Release>:
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wformat=2
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Werror
    >
)

add_test(NAME UnitTests COMMAND UnitTests)

install(
    TARGETS RentalSystem
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Application
)


install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    COMPONENT Application
    OPTIONAL
    FILES_MATCHING PATTERN ".gitkeep"
)

install(
    DIRECTORY DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/records"
    COMPONENT Application
)

add_library(custom_printer_plugin MODULE src/plugins/CustomPrinter.cpp)

target_include_directories(custom_printer_plugin PRIVATE "${CMAKE_SOURCE_DIR}/include")

set_target_properties(custom_printer_plugin PROPERTIES 
    EXCLUDE_FROM_ALL TRUE
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

enable_clang_tidy(custom_printer_plugin)

install(
    TARGETS custom_printer_plugin
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins"
    COMPONENT Plugins
)

add_custom_target(install-app
    DEPENDS RentalSystem rental_engine
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --component Application
    COMMENT "Installing application component..."
)

add_custom_target(install-plugin
    DEPENDS custom_printer_plugin
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --component Plugins
    COMMENT "Installing plugins component..."
)

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


set(CPACK_PACKAGE_NAME "VehicleManagementSystem")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "justkowal")
set(CPACK_PACKAGE_CONTACT "kowal.dev@protonmail.com")

set(CPACK_GENERATOR "TGZ;DEB;RPM")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jakub Kowal <kowal.dev@protonmail.com>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_RPM_PACKAGE_AUTOREQ ON)

include(CPack)
