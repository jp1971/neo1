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

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk)
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)

include(pico_sdk_import.cmake)

project(neo1                           C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD olimex_neo6502)

pico_sdk_init()

# Do not add TinyUSB as a direct subdirectory here. The Pico SDK provides the
# TinyUSB integration/targets we want, and adding the upstream TinyUSB tree as a
# raw subdirectory can break reconfigure in the Pico VS Code workflow.

add_compile_options(-Wall)

option(ENABLE_DVI "Build PicoDVI/libdvi (HDMI video output)" ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/fatfs/source
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PICO_TINYUSB_PATH}/src
    ${PICO_TINYUSB_PATH}/src/class/hid
)

add_compile_definitions(CFG_TUSB_MCU=OPT_MCU_RP2040)

if (ENABLE_DVI)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/PicoDVI/software/include)
    add_subdirectory(lib/PicoDVI/software/libdvi)
endif()

add_subdirectory(lib/fatfs/source)
add_subdirectory(systems)
