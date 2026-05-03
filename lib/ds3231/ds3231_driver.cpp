#include "ds3231.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

// 初始化DS3231
bool ds3231_init(ds3231_t *ds3231, i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin) {
    if (!ds3231 || !i2c) {
        return false;
    }

    ds3231->i2c = i2c;
    ds3231->sda_pin = sda_pin;
    ds3231->scl_pin = scl_pin;
    ds3231->addr = DS3231_I2C_ADDR;

    // 初始化I2C (DS3231支持400kHz高速I2C，但使用100kHz更稳定)
    i2c_init(i2c, 100000); // 100kHz

    // 设置GPIO引脚
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    // 启用振荡器
    return ds3231_enable_oscillator(ds3231, true);
}

// 写入寄存器
bool ds3231_write_register(ds3231_t *ds3231, uint8_t reg, uint8_t value) {
    if (!ds3231) {
        return false;
    }

    uint8_t data[2] = {reg, value};
    int result = i2c_write_blocking(ds3231->i2c, ds3231->addr, data, 2, false);
    return result == 2;
}

// 读取寄存器
bool ds3231_read_register(ds3231_t *ds3231, uint8_t reg, uint8_t *value) {
    if (!ds3231 || !value) {
        return false;
    }

    int result = i2c_write_blocking(ds3231->i2c, ds3231->addr, &reg, 1, true);
    if (result != 1) {
        return false;
    }

    result = i2c_read_blocking(ds3231->i2c, ds3231->addr, value, 1, false);
    return result == 1;
}

// 读取时间
bool ds3231_read_time(ds3231_t *ds3231, ds3231_time_t *time) {
    if (!ds3231 || !time) {
        return false;
    }

    uint8_t data[7];

    uint8_t reg = DS3231_SECONDS_REG;
    int result = i2c_write_blocking(ds3231->i2c, ds3231->addr, &reg, 1, true);
    if (result != 1) {
        return false;
    }

    result = i2c_read_blocking(ds3231->i2c, ds3231->addr, data, 7, false);
    if (result != 7) {
        return false;
    }

    time->seconds = bcd_to_bin(data[0] & 0x7F);
    time->minutes = bcd_to_bin(data[1] & 0x7F);
    time->hours = bcd_to_bin(data[2] & 0x3F);
    time->day = data[3] & 0x07;
    time->date = bcd_to_bin(data[4] & 0x3F);
    time->month = bcd_to_bin(data[5] & 0x1F);
    time->year = bcd_to_bin(data[6]);

    return true;
}

// 写入时间
bool ds3231_write_time(ds3231_t *ds3231, const ds3231_time_t *time) {
    if (!ds3231 || !time) {
        return false;
    }

    uint8_t data[8];
    data[0] = DS3231_SECONDS_REG;
    data[1] = bin_to_bcd(time->seconds);
    data[2] = bin_to_bcd(time->minutes);
    data[3] = bin_to_bcd(time->hours);
    data[4] = time->day & 0x07;
    data[5] = bin_to_bcd(time->date);
    data[6] = bin_to_bcd(time->month);
    data[7] = bin_to_bcd(time->year);

    int result = i2c_write_blocking(ds3231->i2c, ds3231->addr, data, 8, false);
    return result == 8;
}

// 读取温度
bool ds3231_read_temperature(ds3231_t *ds3231, float *temperature) {
    if (!ds3231 || !temperature) {
        return false;
    }

    uint8_t msb, lsb;

    if (!ds3231_read_register(ds3231, DS3231_TEMP_MSB, &msb)) {
        return false;
    }

    if (!ds3231_read_register(ds3231, DS3231_TEMP_LSB, &lsb)) {
        return false;
    }

    int16_t temp_raw = (int16_t)((msb << 8) | lsb);
    temp_raw >>= 6;

    *temperature = temp_raw * 0.25f;
    return true;
}

// 启用/禁用振荡器
bool ds3231_enable_oscillator(ds3231_t *ds3231, bool enable) {
    if (!ds3231) {
        return false;
    }

    uint8_t control_reg;
    if (!ds3231_read_register(ds3231, DS3231_CONTROL_REG, &control_reg)) {
        return false;
    }

    if (enable) {
        control_reg &= ~(1 << DS3231_EOSC_BIT);
    } else {
        control_reg |= (1 << DS3231_EOSC_BIT);
    }

    return ds3231_write_register(ds3231, DS3231_CONTROL_REG, control_reg);
}

// 检查振荡器是否停止
bool ds3231_is_oscillator_stopped(ds3231_t *ds3231, bool *stopped) {
    if (!ds3231 || !stopped) {
        return false;
    }

    uint8_t status_reg;
    if (!ds3231_read_register(ds3231, DS3231_STATUS_REG, &status_reg)) {
        return false;
    }

    *stopped = (status_reg & (1 << DS3231_OSF_BIT)) != 0;
    return true;
}

// 将时间转换为字符串
void ds3231_time_to_string(const ds3231_time_t *time, char *buffer, size_t buffer_size) {
    if (!time || !buffer || buffer_size < 20) {
        return;
    }

    const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    snprintf(buffer, buffer_size, "%s %04d-%02d-%02d %02d:%02d:%02d",
             day_names[time->day - 1],
             2000 + time->year,
             time->month,
             time->date,
             time->hours,
             time->minutes,
             time->seconds);
}

// BCD转二进制
uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 二进制转BCD
uint8_t bin_to_bcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}
