# Pinout — ESP32-S3-WROOM-1 DevKitC

## SPI bus (shared between both OLEDs)

| Signal | GPIO | Notes |
|---|---|---|
| SCK   | 12 | shared |
| MOSI  | 11 | shared |
| DC    | 9  | shared (data/command) |
| RST   | 8  | shared (tied together, reset both at boot) |
| CS1   | 10 | OLED #1 (Player 1, left) |
| CS2   | 13 | OLED #2 (Player 2, right) |

OLED MISO is unused (write-only). Use HSPI on the ESP32-S3 (`SPI` default
in Arduino-ESP32 maps to FSPI; either is fine — the code uses U8g2's
hardware SPI constructor).

## Buttons (all `INPUT_PULLUP`, switch to GND)

Three buttons, Chronos-style. The center button handles pause/resume and —
held down together with the clock buttons — every setting.

| Button | GPIO |
|---|---|
| Left clock button  | 4  |
| Right clock button | 5  |
| Center             | 17 |

GPIO 6, 7, 15, 16 (the old Mode/Set/Up/Down) are now free.

## Output / sense

| Signal | GPIO | Notes |
|---|---|---|
| Buzzer (passive piezo, drive via MOSFET or directly) | 18 | PWM via LEDC |
| Battery sense | 1 | ADC1_CH0, 100k/100k divider from VBAT |
| TP4056 CHRG status | 2 | active LOW when charging |

## Power tree

```
USB-C ──┬── TP4056 (+DW01 protection) ──┬── 1S LiPo (3.0–4.2V) ──┐
        │                               │                       │
        └── (charge only)               └── BAT+ ────────────────┴── MT3608 boost ── 5V ──┬── ESP32-S3 5V pin (LDO → 3.3V)
                                                                                          └── OLED VCC
```

The DevKitC has its own AMS1117 LDO; feed it 5V on the 5V pin (not VIN).
OLEDs run on 3.3V or 5V depending on the breakout — most SSD1309 modules
have an onboard regulator and accept 3.3–5V.

## Reserved / avoid

- GPIO 0, 3, 45, 46 — strapping pins, leave alone
- GPIO 19, 20 — native USB D-/D+
- GPIO 26–32 — used by SPI flash on WROOM-1
- GPIO 33–37 — used by octal PSRAM on WROOM-1 (avoid even on non-PSRAM variants)
