# lib/display: ILI9488 显示驱动与图形层
# 目标名: display (静态库)

cmake_minimum_required(VERSION 3.13)

set(DISPLAY_SOURCES
    src/ili9488_driver.cpp
    src/ili9488_ui.cpp
    src/hal/ili9488_hal.cpp
    src/fonts/ili9488_font.cpp
)

add_library(display STATIC ${DISPLAY_SOURCES})

target_include_directories(display PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(display PUBLIC
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_pwm
)
