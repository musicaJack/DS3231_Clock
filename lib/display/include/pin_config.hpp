#pragma once

/**
 * ILI9488 引脚（与 PicoBoard_Games_ILI9488 一致：SPI0）。
 *
 * DS3231 单独占用 I2C1：SDA=GPIO7、SCL=GPIO6，与 SPI 引脚无重叠。
 * 请在工程中仅用 i2c1 + GP6/GP7 连接 RTC，勿再接其他 I2C 设备除非您自行改脚。
 */

#include "pico/stdlib.h"

typedef struct spi_inst spi_inst_t;

#define ILI9488_SPI_INST        spi0
#define ILI9488_SPI_SPEED_HZ    40000000u

#define ILI9488_PIN_SCK         18
#define ILI9488_PIN_MOSI        19
#define ILI9488_PIN_MISO        255

#define ILI9488_PIN_CS          17
#define ILI9488_PIN_DC          20
#define ILI9488_PIN_RST         15
#define ILI9488_PIN_BL          16

#define ILI9488_GET_SPI_CONFIG() \
    ILI9488_SPI_INST, ILI9488_PIN_DC, ILI9488_PIN_RST, ILI9488_PIN_CS, \
    ILI9488_PIN_SCK, ILI9488_PIN_MOSI, ILI9488_PIN_BL, ILI9488_SPI_SPEED_HZ
