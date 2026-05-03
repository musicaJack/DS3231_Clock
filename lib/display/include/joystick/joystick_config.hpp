/**
 * @file joystick_config.hpp
 * @brief Joystick 运行参数配置（与 demo/Joystick_Pico 对齐，硬件与 pin_config.hpp 一致）
 * @note I2C 端口、引脚、地址见 pin_config.hpp（JOYSTICK_I2C_INST, JOYSTICK_PIN_SDA/SCL, JOYSTICK_I2C_ADDR）
 */

#pragma once

// 调试：设为 1 时通过 printf 输出日志（串口查看），发布时可改为 0
#define JOYSTICK_DEBUG_LOG  1   // 摇杆/光标/按键
#define CHESS_DEBUG_LOG     1   // 游戏：初始化、走棋、AI、终局
#define JOYSTICK_LOG_INTERVAL_MS  500   // 摇杆周期日志间隔（毫秒），避免刷屏
#define CHESS_LOG_SKIP_INTERVAL_MS 3000 // 无摇杆时 tick 跳过日志间隔（毫秒）

// Othello 风格：12-bit 偏移 + 死区 + 角度判定；稍降低灵敏度（死区略大、节流略长）
#ifdef JOYSTICK_DEADZONE
#undef JOYSTICK_DEADZONE
#endif
#define JOYSTICK_DEADZONE          80    // 死区（12-bit 偏移），越大越不灵敏，轻微碰不触发
#define JOYSTICK_INPUT_THROTTLE_MS 220   // 同方向连移节流（毫秒），越大光标连移越慢
#define JOYSTICK_PRINT_INTERVAL_MS 250   // 重复打印/事件间隔（毫秒），用于节流
// 光标连移（长按）：首次移动后，按住超过此时间起开始连移，间隔见下
#define JOYSTICK_HOLD_REPEAT_AFTER_MS  350
#define JOYSTICK_HOLD_REPEAT_INTERVAL_MS 140

// 中键按下时 get_button_value() 的返回值（与 demo 一致：0=按下 1=未按；若摇杆完全无反应可尝试改为 1）
#define JOYSTICK_BUTTON_PRESSED_VALUE  0

// LED 颜色（24 位 RGB，与 demo 一致，覆盖 pin_config 默认值）
#ifdef JOYSTICK_LED_OFF
#undef JOYSTICK_LED_OFF
#endif
#define JOYSTICK_LED_OFF   0x000000U
#ifdef JOYSTICK_LED_RED
#undef JOYSTICK_LED_RED
#endif
#define JOYSTICK_LED_RED   0xFF0000U
#ifdef JOYSTICK_LED_GREEN
#undef JOYSTICK_LED_GREEN
#endif
#define JOYSTICK_LED_GREEN 0x00FF00U
#ifdef JOYSTICK_LED_BLUE
#undef JOYSTICK_LED_BLUE
#endif
#define JOYSTICK_LED_BLUE  0x0000FFU

// 方向枚举（便于应用层使用）
enum class JoystickDirection : int {
    None   = 0,
    Up     = 1,
    Down   = 2,
    Left   = 3,
    Right  = 4,
    Center = 5   // 按键按下
};
