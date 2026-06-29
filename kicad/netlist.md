# click-clack — netlist (source of truth)

This is the human-readable connection list for the perfboard build.
The `.kicad_sch` file mirrors this; if they ever disagree, **this file
wins** and the schematic should be updated to match.

## Reference designators

| Ref | Part |
|---|---|
| U1 | ESP32-S3-WROOM-1 DevKitC-1 |
| U2 | TP4056 USB-C charger module (with DW01 protection) |
| U3 | MT3608 boost converter module (set Vout = 5.0V) |
| DS1 | OLED #1, 2.42" SSD1309 128×64 SPI (Player 1, left) |
| DS2 | OLED #2, 2.42" SSD1309 128×64 SPI (Player 2, right) |
| BT1 | 1S LiPo, 2000–3000 mAh, with built-in protection |
| SW1 | Cherry MX — Player 1 (left paddle) |
| SW2 | Cherry MX — Player 2 (right paddle) |
| SW3 | Cherry MX — Mode |
| SW4 | Cherry MX — Set |
| SW5 | Cherry MX — Up |
| SW6 | Cherry MX — Down |
| SW7 | Cherry MX — Center / Reset / Pause |
| BZ1 | Passive piezo buzzer |
| R1 | 100 kΩ — battery sense divider, top |
| R2 | 100 kΩ — battery sense divider, bottom |
| R3 | 220 Ω — buzzer series |

## Power tree

| Net | Connections |
|---|---|
| `USB_5V` | USB-C VBUS → U2.IN+ |
| `USB_GND` | USB-C GND → U2.IN- → GND |
| `BAT+` | U2.BAT+ → BT1.+ → U3.IN+ → R1 (top of divider) |
| `GND` | U2.BAT- → BT1.- → U3.IN- → all module GNDs |
| `+5V` | U3.OUT+ → U1.5V → DS1.VCC → DS2.VCC |
| `+3V3` | U1.3V3 (output from DevKitC LDO; not used externally) |

> The DevKitC has its own LDO. Feed it on the **5V** pin, not VIN.
> Most SSD1309 OLED breakouts accept 3.3–5V; using +5V is safest.

## ESP32-S3 — SPI (shared between both OLEDs)

| Net | ESP32-S3 GPIO | DS1 pin | DS2 pin |
|---|---|---|---|
| `SCK`  | GPIO12 | SCK / D0 | SCK / D0 |
| `MOSI` | GPIO11 | MOSI / D1 | MOSI / D1 |
| `OLED_DC`  | GPIO9  | DC | DC |
| `OLED_RST` | GPIO8  | RST | RST |
| `OLED_CS1` | GPIO10 | CS  | — |
| `OLED_CS2` | GPIO13 | —   | CS |

## ESP32-S3 — buttons

All buttons connect MCU pin → MX switch → GND. Use internal `INPUT_PULLUP`,
no external pull-up resistors needed.

| Net | GPIO | Switch |
|---|---|---|
| `BTN_P1`     | GPIO4  | SW1 |
| `BTN_P2`     | GPIO5  | SW2 |
| `BTN_MODE`   | GPIO6  | SW3 |
| `BTN_SET`    | GPIO7  | SW4 |
| `BTN_UP`     | GPIO15 | SW5 |
| `BTN_DOWN`   | GPIO16 | SW6 |
| `BTN_CENTER` | GPIO17 | SW7 |

## ESP32-S3 — output / sense

| Net | GPIO | Connects to |
|---|---|---|
| `BUZZER` | GPIO18 | R3 (220Ω) → BZ1.+ ; BZ1.- → GND |
| `VBAT_SENSE` | GPIO1 (ADC1_CH0) | midpoint of R1/R2 divider |
| `CHRG_STAT` | GPIO2 | U2.CHRG (open-drain, active low) |

### Battery sense divider

```
BAT+ ──[ R1 100k ]──┬──[ R2 100k ]── GND
                    │
                    └──► VBAT_SENSE (GPIO1)
```

Max input ≈ 2.1 V at 4.2 V battery, well within ADC range.

## Decoupling / housekeeping

- 10 µF ceramic across DS1 VCC↔GND and DS2 VCC↔GND, close to each module.
- 100 nF ceramic across U1 5V↔GND.
- The TP4056 and MT3608 modules already include their own input/output caps.

## Notes for the PCB pass (later)

- DS1/DS2 RST is shared. If you ever see only one panel come up, split
  RST onto two GPIOs (e.g. GPIO14 and a free pin) and update firmware.
- The center button doubles as pause (short press) and reset-to-loaded-TC
  (long press, >1s). It has no special hardware — same MX switch as the
  others, just a smaller keycap.
- Avoid GPIO 0/3/45/46 (strapping), 19/20 (USB), 26–37 (flash/PSRAM).
