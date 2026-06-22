# Wiring Reference

## OLED Display (SH1106 1.3" I2C)

| OLED Pin | ESP32 Pin | Notes |
|----------|-----------|-------|
| VCC | 3.3V | Do NOT use 5V — will damage the display |
| GND | GND | Any GND pin on the ESP32 |
| SDA | GPIO 21 | Default ESP32 I2C data pin |
| SCL | GPIO 22 | Default ESP32 I2C clock pin |

The I2C address is `0x3C` (standard for most SH1106 modules). If your display doesn't initialize, try `0x3D` — some variants use this alternate address.

## Buttons

Both buttons use the ESP32's internal pull-up resistors (`INPUT_PULLUP`). No external resistors are needed.

| Button | ESP32 Pin | Other leg |
|--------|-----------|-----------|
| Button 1 (Next / Mode switch) | GPIO 18 | GND |
| Button 2 (Previous) | GPIO 19 | GND |

When **not pressed**, the pin reads HIGH (3.3V via internal pull-up).
When **pressed**, it connects the pin to GND and reads LOW.

## Troubleshooting

**Buttons unresponsive:** GPIO 18 and 19 are also the default VSPI pins (MOSI/MISO) on some ESP32 boards. If a library initializes SPI on boot, it can hold these pins in a mode that prevents `digitalRead()` from seeing the LOW. Test by uploading the standalone `button_test` sketch and watching Serial Monitor at 115200 baud. If GPIO 18/19 never read LOW even when shorted to GND, try GPIO 4 and 5 instead.

**Display not appearing:** Check the I2C address — try `0x3D` in `display.begin()` if `0x3C` doesn't work. Also confirm SDA/SCL aren't swapped.

**WiFi connects but sports shows no data:** Test the proxy URL directly in a browser first (see README setup steps). If the browser returns valid JSON, the issue is in the firmware config; if it returns an error, redeploy the Worker.

## Pin Summary

```
ESP32 Dev Board
                    ┌─────────────────┐
               3.3V─┤ 3V3         GND ├─── GND (shared: OLED GND, Button legs)
                    │                 │
          OLED SDA──┤ GPIO21          │
          OLED SCL──┤ GPIO22          │
                    │                 │
           Button1──┤ GPIO18          │
           Button2──┤ GPIO19          │
                    └─────────────────┘
```
