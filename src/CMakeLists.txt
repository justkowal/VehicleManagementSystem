add_library(rental_engine
    Vehicle.cpp
    FleetManager.cpp
)

target_include_directories(rental_engine PUBLIC ${CMAKE_SOURCE_DIR}/include)

set(DEV_FLAGS
    -Wall
    -Wextra
    -Wshadow
)

set(RELEASE_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wformat=2
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Werror
)

target_compile_options(rental_engine PRIVATE
    $<$<CONFIG:Debug>:${DEV_FLAGS}>
    $<$<CONFIG:Release>:${RELEASE_FLAGS}>
)