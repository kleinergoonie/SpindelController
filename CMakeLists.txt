# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# == DIE FOLGENDEN ZEILEN NICHT AENDERN, damit die Raspberry Pi Pico VS Code Extension funktioniert ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

# Link-Pruefungen bei der Compiler-Erkennung fuer Bare-Metal-Toolchains vermeiden.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(SpindelControllerAS5600 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_STDIO_UART OFF CACHE BOOL "Disable UART stdio" FORCE)
set(PICO_STDIO_USB ON CACHE BOOL "Enable USB stdio" FORCE)

pico_sdk_init()

add_executable(SpindelControllerAS5600
    SpindelControllerAS5600.c
    motor_control.c
    encoder_mt6835.c
    encoder_as5600.c
    sensor_adc.c
    safety.c
    remora_iface.c
    w5500_udp.c
    ws2812_status.c
    board_led.c
    spindle_ctrl.c
    display_st7789.c
    touch_input.c
    third_party/ioLibrary_Driver/Ethernet/socket.c
    third_party/ioLibrary_Driver/Ethernet/wizchip_conf.c
    third_party/ioLibrary_Driver/Ethernet/W5500/w5500.c
    third_party/ioLibrary_Driver/Internet/DHCP/dhcp.c
)

target_include_directories(SpindelControllerAS5600 PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(SpindelControllerAS5600 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/third_party/ioLibrary_Driver/Ethernet
    ${CMAKE_CURRENT_LIST_DIR}/third_party/ioLibrary_Driver/Ethernet/W5500
    ${CMAKE_CURRENT_LIST_DIR}/third_party/ioLibrary_Driver/Internet/DHCP
)

target_compile_definitions(SpindelControllerAS5600 PRIVATE
    _WIZCHIP_=W5500
    _WIZCHIP_IO_MODE_=_WIZCHIP_IO_MODE_SPI_VDM_
)

# Board-Variante als C-Praeprozessor-Symbol setzen (PICO_BOARD ist nur ein String).
if(PICO_BOARD STREQUAL "waveshare_rp2040_zero")
    target_compile_definitions(SpindelControllerAS5600 PRIVATE BOARD_WAVESHARE_RP2040_ZERO=1)
elseif(PICO_BOARD STREQUAL "waveshare_rp2040_plus_16mb")
    target_compile_definitions(SpindelControllerAS5600 PRIVATE BOARD_WAVESHARE_RP2040_PLUS_16MB=1)
else()
    message(FATAL_ERROR "Unbekanntes PICO_BOARD '${PICO_BOARD}' - bitte CMakeLists.txt und config.h erweitern")
endif()

target_link_libraries(SpindelControllerAS5600
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_pio
    hardware_spi
    hardware_i2c
    hardware_adc
    hardware_pwm
    hardware_watchdog
)

pico_generate_pio_header(SpindelControllerAS5600 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_enable_stdio_uart(SpindelControllerAS5600 0)
pico_enable_stdio_usb(SpindelControllerAS5600 1)

pico_add_extra_outputs(SpindelControllerAS5600)
