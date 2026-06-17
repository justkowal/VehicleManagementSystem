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
    set(RENTAL_SYSTEM_SOURCES
        main.cpp
        UI.cpp
    )
    if(MINGW)
        list(APPEND RENTAL_SYSTEM_SOURCES "${CMAKE_SOURCE_DIR}/libs/notui/src/WinCompat.cpp")
    endif()
    add_executable(RentalSystem ${RENTAL_SYSTEM_SOURCES})
 
    target_include_directories(RentalSystem PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
    )
 
    if(MINGW)
        target_link_libraries(RentalSystem PRIVATE
            rental_engine
            notui
        )
    else()
        target_link_libraries(RentalSystem PRIVATE
            rental_engine
            notui
            PkgConfig::NOTCURSES
            ${CMAKE_DL_LIBS}
        )
    endif()
 
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
    set(DEMO_NOTUI_SOURCES
        demo_notui.cpp
    )
    if(MINGW)
        list(APPEND DEMO_NOTUI_SOURCES "${CMAKE_SOURCE_DIR}/libs/notui/src/WinCompat.cpp")
    endif()
    add_executable(demo_notui ${DEMO_NOTUI_SOURCES})
 
    target_link_libraries(demo_notui PRIVATE notui)

    if(MINGW)
        target_link_options(RentalSystem PRIVATE -static)
        target_link_options(demo_notui PRIVATE -static)
        target_link_libraries(RentalSystem PRIVATE pthread)
        target_link_libraries(demo_notui PRIVATE pthread)
    endif()
endif()