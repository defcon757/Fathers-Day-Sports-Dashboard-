# Wiring Reference

## OLED Display (SH1106 1.3" I2C)

| OLED Pin | ESP32 Pin | Notes |
|----------|-----------|-------|
| VCC | 3.3V | Do NOT use 5V — will damage the display |
| GND | GND | Any GND pin on the ESP32 |
| SDA | GPIO 21 | Default ESP32 I2C data pin |
| SCL | GPIO 22 | Default ESP32 I2C clock pin |

The I2C address is `0x3C` (standard for most SH1106 modules). If your display doesn't appear, try `0x3D` — some variants use this alternate address.

## Buttons

Both buttons use the ESP32's internal pull-up resistors (`INPUT_PULLUP`). No external resistors are needed.

| Button | ESP32 Pin | Other leg |
|--------|-----------|-----------|
| Button 1 (Next / Mode switch) | GPIO 18 | GND |
| Button 2 (Previous) | GPIO 19 | GND |

When the button is **not pressed**, the pin reads HIGH (3.3V via pull-up).
When the button is **pressed**, it connects the pin to GND and reads LOW.

> ⚠️ GPIO 18 and 19 are also the default VSPI pins (MOSI/MISO) on some ESP32 boards. If a library initializes SPI on startup, it can hold these pins in a mode that prevents `digitalRead()` from working. If buttons seem unresponsive and your Serial Monitor shows nothing changing, try GPIO 4 and GPIO 5 instead — they're free of SPI conflicts.

## Power

The device runs from the ESP32's USB port. Any USB power source works:
- USB wall adapter
- USB power bank (ideal for portability)
- Computer USB port during development

Current draw is low — the OLED and ESP32 together pull under 100mA typically, well within any USB source's rating.

## Pin Summary Diagram

```
ESP32 Dev Board
                    ┌─────────────────┐
               3.3V─┤ 3V3         GND ├─GND (OLED, Buttons)
                    │                 │
          OLED SDA──┤ GPIO21          │
          OLED SCL──┤ GPIO22          │
                    │                 │
           Button1──┤ GPIO18          │
           Button2──┤ GPIO19          │
                    └─────────────────┘
```
