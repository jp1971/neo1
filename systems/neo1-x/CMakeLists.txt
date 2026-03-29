add_executable(neo1
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_terminal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_msc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_cffa1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/neo1_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/console_font_8x8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.S
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ff.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffunicode.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/ffsystem.c
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source/diskio.c
)

find_program(CA65_EXECUTABLE ca65)
find_program(LD65_EXECUTABLE ld65)
find_package(Python3 COMPONENTS Interpreter QUIET)

set(NEO1_VACI_SOURCE ${CMAKE_SOURCE_DIR}/src/ram/neo1_vaci_v1.s)
set(NEO1_VACI_CFG ${CMAKE_SOURCE_DIR}/src/ram/neo1_vaci_v1.cfg)
set(NEO1_VACI_GEN_SCRIPT ${CMAKE_SOURCE_DIR}/src/ram/gen_vaci_header.py)
set(NEO1_VACI_OBJ ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_vaci_v1.o)
set(NEO1_VACI_BIN ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_vaci_v1.bin)
set(NEO1_VACI_LABELS ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_vaci_v1.labels)
set(NEO1_VACI_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_vaci_v1.h)
set(NEO1_CFFA1_M2_SOURCE ${CMAKE_SOURCE_DIR}/src/ram/neo1_cffa1_m2_blockdrv.s)
set(NEO1_CFFA1_M2_CFG ${CMAKE_SOURCE_DIR}/src/ram/neo1_cffa1_m2.cfg)
set(NEO1_CFFA1_M2_GEN_SCRIPT ${CMAKE_SOURCE_DIR}/src/ram/gen_cffa1_m2_header.py)
set(NEO1_CFFA1_M2_OBJ ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_cffa1_m2_blockdrv.o)
set(NEO1_CFFA1_M2_BIN ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_cffa1_m2_blockdrv.bin)
set(NEO1_CFFA1_M2_LABELS ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_cffa1_m2_blockdrv.labels)
set(NEO1_CFFA1_M2_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/ram/neo1_cffa1_m2_blockdrv.h)

if(CA65_EXECUTABLE AND LD65_EXECUTABLE AND Python3_Interpreter_FOUND)
    add_custom_command(
        OUTPUT ${NEO1_VACI_HEADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/ram
        COMMAND ${CA65_EXECUTABLE} ${NEO1_VACI_SOURCE} -o ${NEO1_VACI_OBJ}
        COMMAND ${LD65_EXECUTABLE} -C ${NEO1_VACI_CFG} ${NEO1_VACI_OBJ} -o ${NEO1_VACI_BIN} -Ln ${NEO1_VACI_LABELS}
        COMMAND ${Python3_EXECUTABLE} ${NEO1_VACI_GEN_SCRIPT} --bin ${NEO1_VACI_BIN} --out ${NEO1_VACI_HEADER}
        DEPENDS ${NEO1_VACI_SOURCE} ${NEO1_VACI_CFG} ${NEO1_VACI_GEN_SCRIPT}
        VERBATIM
        COMMENT "Building neo1_vaci_v1 RAM image/header from assembly"
    )

    add_custom_command(
        OUTPUT ${NEO1_CFFA1_M2_HEADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/ram
        COMMAND ${CA65_EXECUTABLE} ${NEO1_CFFA1_M2_SOURCE} -o ${NEO1_CFFA1_M2_OBJ}
        COMMAND ${LD65_EXECUTABLE} -C ${NEO1_CFFA1_M2_CFG} ${NEO1_CFFA1_M2_OBJ} -o ${NEO1_CFFA1_M2_BIN} -Ln ${NEO1_CFFA1_M2_LABELS}
        COMMAND ${Python3_EXECUTABLE} ${NEO1_CFFA1_M2_GEN_SCRIPT} --labels ${NEO1_CFFA1_M2_LABELS} --bin ${NEO1_CFFA1_M2_BIN} --out ${NEO1_CFFA1_M2_HEADER}
        DEPENDS ${NEO1_CFFA1_M2_SOURCE} ${NEO1_CFFA1_M2_CFG} ${NEO1_CFFA1_M2_GEN_SCRIPT}
        VERBATIM
        COMMENT "Building neo1_cffa1_m2_blockdrv RAM image/header from assembly"
    )

    add_custom_target(neo1_vaci_header_gen DEPENDS ${NEO1_VACI_HEADER})
    add_custom_target(neo1_cffa1_m2_header_gen DEPENDS ${NEO1_CFFA1_M2_HEADER})
    add_dependencies(neo1 neo1_vaci_header_gen)
    add_dependencies(neo1 neo1_cffa1_m2_header_gen)
    target_include_directories(neo1 BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
else()
    message(WARNING "RAM tool auto-rebuild disabled (missing ca65/ld65/python3); using checked-in src/ram headers")
endif()

target_compile_options(neo1 PRIVATE -Wall)

target_compile_definitions(neo1 PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=olimex_neo6502_cfg
    DVI_VERTICAL_REPEAT=1
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1
    NEO1_ENABLE_DVI=1
    NEO1_PERSONALITY=50
    # Personality toggles -- all default to (1). Override here to disable.
    # NEO1_PERSONALITY=50   # Neo1-50: WozMon-only ROM at $FF00, E000-EFFF writable
    # NEO1_ENABLE_VACI=0    # set 0 to free $C100 for hardware ACI or other uses
    # NEO1_ENABLE_VCFFA1=0  # set 0 to free $AFF0-$AFFF and $AFDC-$AFDD
)

target_include_directories(neo1 PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/PicoDVI/software/include
    ${CMAKE_SOURCE_DIR}/lib/PicoDVI/software/libdvi
    ${CMAKE_SOURCE_DIR}/lib/fatfs/source
)

target_link_libraries(neo1
    pico_stdlib
    pico_multicore
    pico_util
    tinyusb_host
    tinyusb_board
    libdvi
    hardware_dma
    hardware_irq
    hardware_pio
    hardware_interp
    hardware_vreg
    hardware_clocks
)

# create map/bin/hex/uf2
pico_add_extra_outputs(neo1)

# Serial-only for now
pico_enable_stdio_usb(neo1 0)
pico_enable_stdio_uart(neo1 1)