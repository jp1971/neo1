find_package(SDL2 REQUIRED)

add_executable(neo1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_platform_sdl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_storage_stub.c
)

target_compile_definitions(neo1 PRIVATE
    NEO1_PERSONALITY=${NEO1_PERSONALITY}
    NEO1_ENABLE_VACI=${NEO1_ENABLE_VACI}
    NEO1_ENABLE_VCFFA1=${NEO1_ENABLE_VCFFA1}
    NEO1_CPU_BACKEND=${NEO1_CPU_BACKEND}
)

target_include_directories(neo1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(neo1 PRIVATE SDL2::SDL2)

if (APPLE)
    target_link_libraries(neo1 PRIVATE SDL2::SDL2main)
endif()