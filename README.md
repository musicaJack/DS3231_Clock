# DS3231 Clock (RP2040)

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%20Pico%20W-red.svg)](https://www.raspberrypi.com/products/raspberry-pi-pico/)
[![Version](https://img.shields.io/badge/Version-3.2.0-green.svg)]

English | [中文](README.zh.md)
 <img src="imgs/analog-clock.jpg" width="240" />

Firmware for **Raspberry Pi Pico (RP2040)** that drives a **DS3231** real-time clock and a **3.5″ ILI9488** LCD (320×480, portrait) with an **analog-style watch face**: hour numerals, tick ring, three hands, and a **bottom-centered** `MM/DD` + **English weekday** line. An optional **month calendar** view uses landscape orientation (480×320 logical). Time and calendar are read from the **DS3231 only**; the MCU does not run a free-running software clock for display. **USB serial** can set the RTC; successful writes go to the chip (battery-backed modules keep time across power loss). **GPIO keys** toggle clock vs calendar and change the month shown in calendar mode.


---

## Features

- DS3231 over **I2C1**; ILI9488 over **SPI0** (40 MHz in `pin_config.hpp`).
- Analog dial refresh driven by the RTC **second** register change (not busy-loop timing).
- **Month calendar** (landscape): black/white grid; **today** (from RTC) inverted when viewing that month.
- **GPIO**: short press **GP14** toggles clock ↔ calendar; in calendar mode **GP8** next month, **GP9** previous month (default **active-low** with pull-ups; configurable in `main.cpp`).
- USB CDC (TinyUSB) **non-blocking** line input for time sync.
- Integration surfaces: `AnalogClockView`, `calendar_month_paint`, DS3231 APIs in `lib/ds3231`, USB helpers in `src/sync/`.

---

## Repository layout

| Path | Role |
|------|------|
| `apps/ds3231_clock/main.cpp` | Application entry: init hardware, main loop, UI mode and keys. |
| `src/clock/` | Watch UI: `AnalogClockView`, `gfx_clock`, `calendar_month_view`. |
| `src/sync/` | USB time-sync parsing and RTC write helpers. |
| `lib/ds3231/` | DS3231 driver (`ds3231.h`, `ds3231_driver.cpp`). |
| `lib/display/` | ILI9488 driver stack (CMake target `display`). |

Include paths for the firmware target: `src/clock`, `src/sync`, and `lib/ds3231` (see root `CMakeLists.txt`).

---

## Hardware connections

### DS3231 (I2C1)

| Signal | GPIO |
|--------|------|
| SDA | **GP7** |
| SCL | **GP6** |

Use **I2C1** only for the RTC on these pins unless you change the wiring and `main.cpp` constants.

### ILI9488 (SPI0)

Defined in `lib/display/include/pin_config.hpp` (same mapping as the PicoBoard_Games_ILI9488 reference):

| Signal | GPIO |
|--------|------|
| SCK | GP18 |
| MOSI | GP19 |
| CS | GP17 |
| DC | GP20 |
| RST | GP15 |
| BL (backlight PWM) | GP16 |

SPI instance: **SPI0**, 40 MHz. MISO is unused (`255`).

### Keys (optional calendar UI)

| Function | GPIO | Notes |
|----------|------|--------|
| Toggle clock / month calendar | **GP14** | Short press (~20–900 ms); internal pull-up if `kKeysActiveLow` |
| Previous month (calendar) | **GP9** | Same polarity as mode key |
| Next month (calendar) | **GP8** | Same polarity as mode key |

**Note:** DS3231 and SPI display pins do **not** overlap these GPIOs.

---

## Build

Requirements:

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- Environment variable **`PICO_SDK_PATH`** pointing at the SDK root
- Ninja (optional) or another CMake generator

```bash
cd DS3231_Clock
mkdir build && cd build
cmake -G Ninja ..
cmake --build .
```

Artifacts (under `build/`): `ds3231_clock.elf`, and usually `ds3231_clock.uf2` for drag-and-drop programming on Pico.

---

## Usage

### On device

1. Power on; the firmware reads time from the DS3231 and draws the dial.
2. **GP14** short press switches to the month calendar (landscape); press again to return to the clock.
3. In calendar mode, **GP8** / **GP9** step the viewed month (next / previous).

### USB time sync

1. Open the USB CDC serial port (e.g. 115200 unless overridden).
2. Send a line and press Enter:
   - `hh:mm:ss` — set time of day (date left unchanged by reading back RTC first where implemented).
   - `YYYY-MM-DD hh:mm:ss` or `YYYY-MM-DDThh:mm:ss` — full date and time (weekday computed for DS3231).

See `usb_time_sync.cpp` for exact parsing rules and return codes of `usb_sync_poll_line`.

---

## USB serial output (startup / diagnostics)

After `stdio_init_all()` and an ~800 ms delay, the firmware prints **via USB CDC** (same port as time sync):

| Message | Meaning |
|---------|---------|
| Banner + format help from `usb_sync_print_banner()` | Describes accepted time strings (Chinese text in current firmware) |
| `DS3231: I2C1 SDA=GP7 SCL=GP6` | RTC wiring echoed from `main.cpp` |
| `ILI9488: SPI0 per lib/display pin_config.hpp` | Display uses SPI0; details in `pin_config.hpp` |
| `Keys: MODE=GP14 PREV=GP9 NEXT=GP8 (active low)` | GPIO map for UI keys; `PREV`/`NEXT` are **month** in calendar mode |
| `RTC OSF: set time via USB...` | Only if oscillator-stop flag is set — battery or valid time needed |
| `DS3231 init failed.` / `ILI9488 init failed.` | Fatal init error; firmware stops in a tight loop |
| Prompt from `usb_sync_print_prompt()` | Input line ready for USB time sync |

Additional lines appear when you submit a time line (success / failure messages from `usb_sync_poll_line`).

---

## Integration notes (API sketch)

- **RTC:** `ds3231_init`, `ds3231_read_time`, `ds3231_write_time`, etc. — `lib/ds3231/ds3231.h`.
- **Display:** `ili9488::ILI9488Driver` — `lib/display/include/ili9488_driver.hpp`; link CMake target `display`.
- **Watch face:** `AnalogClockView` — `src/clock/analog_clock.hpp`; requires an initialized `ILI9488Driver`.
- **Calendar:** `calendar_month_paint` — `src/clock/calendar_month_view.hpp`.
- **USB sync:** `usb_sync_poll_line`, `rtc_set_datetime`, … — `src/sync/usb_time_sync.hpp`.

Link order (CMake): `display`, `pico_stdlib`, `hardware_i2c`, `hardware_spi`, `hardware_pwm`, `hardware_gpio`, plus TinyUSB via stdio USB as configured.

---

## License

See [LICENSE](LICENSE) in the repository root.
