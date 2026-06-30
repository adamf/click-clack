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
    ├── click-clack.scad  Parametric OpenSCAD case
    ├── render-preview.png Assembled render
    └── stl/              Ready-to-slice meshes (STL + Bambu 3MF)
```

![assembled case](case/render-preview.png)

## Hardware

- MCU: ESP32-S3-WROOM-1 DevKitC
- Displays: 2× 2.42" SSD1309 128×64 SPI OLED
- Switches: 3× Cherry MX (2 clock buttons + 1 center button, Chronos-style)
- Power: 1S LiPo + USB-C TP4056 + MT3608 boost
- Buzzer: passive piezo

See `hardware/pinout.md` for the full pin map and `hardware/schematic.txt` for wiring.

## Build

```
cd firmware
pio run -t upload
pio device monitor
```

## Case / 3D printing (Bambu Lab)

The case copies the classic **Chronos** shape — a steep, tilted **screen
face** and a flat **top** for the buttons — sized to the real thing
(~210 × 70 × 63 mm). It's three printed parts: a lower **shell** that carries
the Chronos silhouette, a flat **display panel** that the OLEDs mount behind,
and a flat **top panel** that the switches mount into. Ready-to-slice meshes
are in `case/stl/` (STL + Bambu Studio's native 3MF); the source of truth is
the parametric `case/click-clack.scad`.

| Part | File | Size | Print orientation |
|---|---|---|---|
| Shell         | `click-clack-shell.{stl,3mf}`   | 210 × 70 × 59 mm | floor on the bed |
| Display panel | `click-clack-display.{stl,3mf}` | 209 × 67 × 4 mm  | flat, screen face on the bed |
| Top panel     | `click-clack-top.{stl,3mf}`     | 209 × 27 × 4 mm  | flat, top face on the bed |

All three parts fit a 256 × 256 mm Bambu bed (X1 / X1C / P1S / P1P / A1) and
print flat / floor-down with **no supports** — the two faces are separate flat
panels, so nothing is ever printed as an overhang. (The **A1 mini** at
180 × 180 is too small for the 210 mm width.)

Slice in **Bambu Studio** (drop in the 3MF, or import the STL):

- **Filament:** PETG or PLA+ — a chess clock gets slapped, and PETG shrugs off
  the repeated impact better than plain PLA.
- **Layer:** 0.20 mm · **Walls:** 4 perimeters · **Infill:** 20–30 % gyroid.
- **Supports:** none. The USB-C cutout is a short 10 mm bridge, the switch
  reliefs open upward, and every boss/pillar is vertical.
- **Adhesion:** 5 mm brim on the shell — 210 mm of flat footprint can lift at
  the corners without one.

### How to print it

Start to finish on a Bambu Lab printer:

1. **Install Bambu Studio** (free, from the Bambu Lab site) and pick your
   printer + nozzle (0.4 mm) the first time you open it.
2. **Load the parts.** `File → Import` the three meshes in `case/stl/` (or just
   `click-clack-plate.3mf`, which lays all three out for you). They arrive in
   the correct orientation — shell floor-down, both panels flat — so **don't
   rotate them**.
3. **Pick the filament** in the top-right dropdown (Bambu PETG HF or any
   PETG/PLA+ you have loaded), and set the print profile to **0.20 mm
   Standard**.
4. **Apply the settings** above: in *Quality* set Wall loops = 4; in *Strength*
   set infill to 20–30 % Gyroid; in *Support* leave supports **off**; in
   *Others → Brim* choose Outer brim, 5 mm. (Save these as a custom process
   preset and you only do it once.)
5. **Slice** and sanity-check the preview: roughly ~6–8 h and ~150 g of
   filament for all three parts. Scrub the layer slider — you should see no
   support structures.
6. **Print.** Send over the network or drop the `.3mf`/`.gcode` on the SD card.
   Use a clean PEI plate; for PETG, a light glue-stick layer stops it bonding
   *too* hard. First-layer adhesion across the 210 mm shell is the main risk —
   that's what the brim is for.
7. **After printing:** peel the brim off, then press M3 heat-set inserts into
   the shell bosses with a soldering iron (~200 °C, square and let cool). The
   two panels screw down into them last, once the electronics are mounted.

No Bambu printer? The same STLs slice fine in PrusaSlicer/Cura, or upload them
to a print service (see `BOM.md`).

### Built to take a beating

Players hammer these buttons, so the top panel is engineered for it:

- 4 mm panels seated in a recess around their whole perimeter — no corner-only
  span.
- M3 screw bosses at every panel corner clamp the panels down.
- Support **pillars** rise from the floor right under the two clock buttons, so
  a hard press transfers straight into the shell instead of flexing the panel.
- A transverse **ridge rib** (with a wiring notch) backs the seam where the two
  faces meet.
- Each MX cutout has a proper 1.5 mm clip land with an underside relief, so the
  switches actually snap in and stay put under load.

Use M3 brass heat-set inserts in the shell bosses and M3 screws (8–10 mm)
through the counterbores in the panels.

Regenerate the meshes after editing the `.scad`:

```
cd case
for p in shell display top; do
  openscad -o stl/click-clack-$p.stl -D "part=\"$p\"" click-clack.scad
done
```

## Controls (Chronos-style, three buttons)

Just two clock buttons and one center button — exactly like a Chronos. The
center button handles everything else through press-and-hold combos.

**Playing:**
- **Your clock button** (left / right): press to end your move and start the opponent
- **Center, tap**: pause / resume
- **Center, tap at flag**: start a fresh game with the loaded time control

**Settings — hold the center button:**
- **Hold center ~1s** (while stopped): enter SETUP (a beep confirms)
- In SETUP: **tap center** = next field · **left** = decrease · **right** = increase
- **Hold center ~1s** again: save, exit, and reset the clocks to the new control

SETUP walks through a **Preset** (common controls applied to both sides), then
per-side base/increment, then the options below.

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
