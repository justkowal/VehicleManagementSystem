# rental_engine core library
add_library(rental_engine OBJECT
    Vehicle.cpp
    FleetMgmt.cpp
    FileStorage.cpp
    EscPosPrinter.cpp
    AppPaths.cpp
    Logger.cpp
)

# consumers of rental_engine inherit this include path
target_include_directories(rental_engine PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

# compiler warnings
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

if(WIN32)
    target_link_libraries(rental_engine PUBLIC ws2_32)
endif()


if(NOTCURSES_FOUND)
    # RentalSystem main application
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

    # demo_notui developer demo
    add_executable(demo_notui
        demo_notui.cpp
    )

    target_link_libraries(demo_notui PRIVATE notui)
endif()