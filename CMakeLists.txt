cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Target board" FORCE)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ds3231_clock C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(lib/display)

add_executable(ds3231_clock
    apps/ds3231_clock/main.cpp
    src/clock/analog_clock.cpp
    src/clock/gfx_clock.cpp
    src/sync/usb_time_sync.cpp
    lib/ds3231/ds3231_driver.cpp
)

target_include_directories(ds3231_clock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clock
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/ds3231
)

target_link_libraries(ds3231_clock PRIVATE
    display
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_spi
    hardware_pwm
)

pico_enable_stdio_usb(ds3231_clock 1)
pico_enable_stdio_uart(ds3231_clock 1)

pico_add_extra_outputs(ds3231_clock)
