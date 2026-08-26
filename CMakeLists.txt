# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.12)

set(NEO1_PLATFORM "pico" CACHE STRING "Neo1 platform target: pico or sdl")
set_property(CACHE NEO1_PLATFORM PROPERTY STRINGS pico sdl)

# The Pico SDK import must happen before project() so CMake selects the ARM
# toolchain on a clean configure. The VS Code Pico extension's generated block
# above supplies its managed PICO_SDK_PATH. Standalone builds without that
# block/environment fall back to the checked-in SDK submodule.
if (NEO1_PLATFORM STREQUAL "pico")
    if (NOT PICO_SDK_PATH)
        set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk")
    endif()
    # Olimex Neo6502 is a project-specific RP2040 board definition carried by
    # the checked-in SDK fork; expose just that board header to managed SDKs.
    list(APPEND PICO_BOARD_HEADER_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk/src/boards/include/boards")
    list(REMOVE_DUPLICATES PICO_BOARD_HEADER_DIRS)
    set(PICO_BOARD olimex_neo6502 CACHE STRING "Pico SDK board")
    set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb")
    include(pico_sdk_import.cmake)
endif()

project(neo1                           C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if (NEO1_PLATFORM STREQUAL "pico")
    pico_sdk_init()

    # The Pico runner requires PicoDVI. Keep the option visible so unsupported
    # attempts to disable it fail with an explicit configuration error below.
    option(ENABLE_DVI "Enable DVI output using PicoDVI" ON)

    # Do not add TinyUSB as a direct subdirectory here. The Pico SDK provides the
    # TinyUSB integration/targets we want, and adding the upstream TinyUSB tree as a
    # raw subdirectory can break reconfigure in the Pico VS Code workflow.
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/fatfs/source
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PICO_TINYUSB_PATH}/src
        ${PICO_TINYUSB_PATH}/src/class/hid
    )

    add_compile_definitions(CFG_TUSB_MCU=OPT_MCU_RP2040)

    if (ENABLE_DVI)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/PicoDVI/software/include)
        add_subdirectory(lib/PicoDVI/software/libdvi)
        # This pinned PicoDVI revision predates the SDK 2.x rename of the DMA
        # debug transfer-count field from tcr to dbg_tcr. Keep the compatibility
        # alias on PicoDVI's interface rather than modifying the submodule.
        if (PICO_SDK_VERSION_MAJOR GREATER_EQUAL 2)
            target_compile_definitions(libdvi INTERFACE tcr=dbg_tcr)
        endif()
    else()
        message(FATAL_ERROR "ENABLE_DVI=OFF is not currently supported by systems/neo1-pico (neo1_video requires PicoDVI).")
    endif()

    add_subdirectory(lib/fatfs/source)
elseif (NOT NEO1_PLATFORM STREQUAL "sdl")
    message(FATAL_ERROR "NEO1_PLATFORM must be 'pico' or 'sdl'")
endif()

add_subdirectory(src/devices)
add_subdirectory(src/systems)
if (NEO1_PLATFORM STREQUAL "sdl")
    add_subdirectory(src/runners)
endif()
add_subdirectory(src/terminal)
add_subdirectory(systems)

# Host tests exercise Neo1-owned protocol code without linking an SDL runner or
# cross-compiling test executables for RP2040. BUILD_TESTING is provided by
# CTest and remains available to ordinary SDL configure presets.
if (NEO1_PLATFORM STREQUAL "sdl")
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
