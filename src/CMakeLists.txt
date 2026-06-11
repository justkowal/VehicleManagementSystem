# ---------------------------------------------------------------------------
# rental_engine — core business logic library
# ---------------------------------------------------------------------------
add_library(rental_engine OBJECT
    Vehicle.cpp
    FleetMgmt.cpp
    FileStorage.cpp
    EscPosPrinter.cpp
    AppPaths.cpp
)

# PUBLIC: consumers of rental_engine automatically inherit this include path
target_include_directories(rental_engine PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

# Compiler warnings — escalated in Release to catch production issues early
target_compile_options(rental_engine PRIVATE
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

enable_clang_tidy(rental_engine)

# ---------------------------------------------------------------------------
# RentalSystem — main application executable
# ---------------------------------------------------------------------------
add_executable(RentalSystem
    main.cpp
    UI.cpp
)

target_include_directories(RentalSystem PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(RentalSystem PRIVATE
    rental_engine
    notui
    PkgConfig::NOTCURSES
)

target_compile_options(RentalSystem PRIVATE
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

enable_clang_tidy(RentalSystem)

# ---------------------------------------------------------------------------
# demo_notui — developer demo for the notui widget library
# ---------------------------------------------------------------------------
add_executable(demo_notui
    demo_notui.cpp
)

target_link_libraries(demo_notui PRIVATE notui)