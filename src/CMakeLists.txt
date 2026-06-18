add_library(rental_engine OBJECT
    Vehicle.cpp
    FleetMgmt.cpp
    FileStorage.cpp
    EscPosPrinter.cpp
    AppPaths.cpp
    Logger.cpp
)

target_include_directories(rental_engine PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

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

add_executable(RentalSystem
    main.cpp
    UI.cpp
    ui/AdvancedFilterModal.cpp
    ui/LoadingOverlay.cpp
    ui/LogsModal.cpp
    ui/QuickReturnModal.cpp
    ui/RentVehicleModal.cpp
    ui/RegisterVehicleModal.cpp
    ui/StatusChangeModal.cpp
)

target_include_directories(RentalSystem PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(RentalSystem PRIVATE
    rental_engine
    notui
    PkgConfig::NOTCURSES
    ${CMAKE_DL_LIBS}
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

