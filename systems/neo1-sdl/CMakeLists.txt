# SDL runner, SDL-local mixed platform services, and its duplicate raw-image
# storage protocols. No Pico service or RAM-payload source is linked here.
find_package(SDL2 REQUIRED)

if (NOT NEO1_CPU_BACKEND STREQUAL "3")
    message(FATAL_ERROR "neo1-sdl requires NEO1_CPU_BACKEND=3 (soft65C02 backend)")
endif()

set(NEO1_ENABLE_MSC "1" CACHE STRING "Enable Neo1 MSC register decode (1 or 0)")
if (NOT NEO1_ENABLE_MSC MATCHES "^[01]$")
    message(FATAL_ERROR "NEO1_ENABLE_MSC must be 0 or 1")
endif()
if (NEO1_ENABLE_VACI STREQUAL "1" AND NOT NEO1_ENABLE_MSC STREQUAL "1")
    message(FATAL_ERROR "NEO1_ENABLE_VACI=1 requires NEO1_ENABLE_MSC=1")
endif()

add_executable(neo1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_platform_sdl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_terminal_sdl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_storage_stub.c
)

target_compile_definitions(neo1 PRIVATE
    NEO1_PERSONALITY=${NEO1_PERSONALITY}
    NEO1_ENABLE_VACI=${NEO1_ENABLE_VACI}
    NEO1_ENABLE_MSC=${NEO1_ENABLE_MSC}
    NEO1_ENABLE_VCFFA1=${NEO1_ENABLE_VCFFA1}
    NEO1_CPU_BACKEND=${NEO1_CPU_BACKEND}
)

target_include_directories(neo1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(neo1 PRIVATE neo1_apple1_pia neo1_terminal_grid SDL2::SDL2)

if (APPLE)
    target_link_libraries(neo1 PRIVATE SDL2::SDL2main)
endif()
