# click-clack

A Chronos-style chess clock built on ESP32-S3 with dual SPI OLEDs and Cherry MX switches.

## Layout

```
click-clack/
├── firmware/        PlatformIO project (ESP32-S3 + Arduino + U8g2)
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
├── hardware/
│   ├── schematic.txt    ASCII wiring diagram for perfboard build
│   └── pinout.md        Pin assignments
└── case/
    └── click-clack.scad  Parametric OpenSCAD case
```

## Hardware

- MCU: ESP32-S3-WROOM-1 DevKitC
- Displays: 2× 2.42" SSD1309 128×64 SPI OLED
- Switches: 7× Cherry MX (2 player, 4 menu, 1 center reset)
- Power: 1S LiPo + USB-C TP4056 + MT3608 boost
- Buzzer: passive piezo

See `hardware/pinout.md` for the full pin map and `hardware/schematic.txt` for wiring.

## Build

```
cd firmware
pio run -t upload
pio device monitor
```

## Controls (Chronos-style)

- **Player buttons** (left/right): press to end your move and start opponent's clock
- **Center small button**: pause / resume; long-press to reset to last loaded time control
- **Mode**: cycle between RUN, MENU, OPTIONS
- **Set**: in MENU, select field to edit; in OPTIONS, toggle setting
- **Up/Down**: adjust value

## Time controls

- Sudden death (e.g. 5+0, 3+0, 1+0)
- Fischer increment (e.g. 3+2, 5+3, 15+10)
- Each side configurable independently (asymmetric / odds games)

## Options (persisted to NVS)

- `stop_on_flag` — freeze game when a flag falls (default ON)
- `beep_on_flag` — buzzer on flag (default ON)
- `beep_on_move` — short click on each move press (default OFF)
- `low_time_warn` — beep at 10s/5s remaining (default ON)
- `brightness` — OLED contrast 0–255
- `default_tc` — time control loaded on power-up
