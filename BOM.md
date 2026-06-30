# click-clack — Shopping List

There's no single store that carries everything (MCU dev boards, OLEDs,
keyboard switches, LiPo, and 3D-print hardware all live in different
ecosystems). The lists below are grouped by vendor so each cart is
self-contained. Total estimate: **~$70–95** for one clock.

---

## 1. Amazon (fastest, slightly more expensive)

If you want everything in 2 days and don't care about a few dollars:

| Qty | Item | Search term | ~$ |
|---|---|---|---|
| 1 | ESP32-S3-DevKitC-1 (N8R2 or N16R8) | "ESP32-S3-DevKitC-1" | $15 |
| 2 | 2.42" OLED 128×64 SSD1309 SPI, white or yellow | "2.42 inch OLED SSD1309 SPI" | $24 |
| 1 | TP4056 USB-C charging module **with protection** (5-pack) | "TP4056 USB C protection" | $7 |
| 1 | MT3608 boost converter (5-pack) | "MT3608 DC-DC boost" | $6 |
| 1 | 1S LiPo 2000 mAh with JST-PH and protection | "2000mAh 3.7V lipo JST" | $10 |
| 1 | Passive piezo buzzer (assortment) | "passive piezo buzzer 3V" | $6 |
| 1 | M3 heat-set inserts + screws kit | "M3 heat set inserts kit" | $12 |
| 1 | 22 AWG silicone wire assortment | "22 AWG silicone wire kit" | $10 |
| 1 | Perfboard assortment (double-sided, 5×7 / 7×9 cm) | "double sided perfboard" | $8 |

**Amazon subtotal: ~$98** (you'll have leftovers of almost everything)

---

## 2. Mechanical keyboard parts (Amazon, Drop, KBDfans, or NovelKeys)

| Qty | Item | Notes |
|---|---|---|
| 3 (or a 10-pack) | Cherry MX switches | Pick your feel: **MX Brown** = tactile/quiet (recommended for a chess clock at a tournament), **MX Blue** = clicky/loud, **MX Red** = linear |
| 2 | Large round keycaps for the clock buttons | To mimic the big round Chronos buttons. Big-knob / arcade-style 1u caps or jumbo round caps both work; a contrasting colour each side is classic. |
| 1 | Red keycap for the center button | The Chronos's red center button. A red DSA/XDA 1u cap is fine. |

**Estimate: ~$15–25** depending on brand. NovelKeys and KBDfans sell singles; Amazon sells 10-packs of generic-brand "MX-style" switches (Gateron, Outemu) for ~$10 which work fine for this.

---

## 3. DigiKey / Mouser BOM (CSV — see `BOM.csv`)

For just the discrete components (resistors, capacitors), use the included
`BOM.csv` to upload to **DigiKey** (https://www.digikey.com/en/mylists/list/upload)
or **Mouser** (https://www.mouser.com/Bom/UploadBom). Both accept the
columns `Manufacturer Part Number`, `Quantity`, and `Customer Reference`.

The CSV covers resistors, capacitors, and the buzzer driver MOSFET (optional).
**Subtotal: ~$5** plus shipping (which is why I recommend buying these as
part of a bigger order, or getting an assortment from Amazon instead).

---

## 4. 3D print

Ready-to-slice meshes live in `case/stl/` (STL + Bambu Studio 3MF). Either:
- Print at home in **PETG** (recommended — it takes the button-slapping better
  than PLA) or PLA+, ~150g of filament total — **~$3** in material. On a Bambu
  Lab printer just drop in the 3MF; see the README for the full profile.
- Or upload the STLs to **JLCPCB 3D Printing**, **PCBWay**, **Craftcloud**, or
  **Shapeways** — **~$25–40** in MJF or SLA

**Fasteners for assembly** (a cheap M2/M3 assortment covers all of it):
- 8 × **M2 self-tapping screws** (~6 mm) — mount the two OLED modules.
- 8 × **M3 screws** (8–10 mm) + 8 × **M3 brass heat-set inserts** — the two
  panels into the shell bosses.
- A strip of **double-sided foam tape** or a small velcro strap for the battery.

---

## 5. Optional / nice-to-have

| Item | Why |
|---|---|
| Logic analyzer (Saleae clone, ~$10) | Debug SPI if OLEDs don't come up |
| JST-PH crimping kit | Clean battery connection |
| Hot glue + Kapton tape | Strain relief on the LiPo wires |
| Slide switch (SPDT, panel mount) | Hard power switch on the back |

---

## TL;DR cheapest path

**Just buy the Amazon list above plus a 10-pack of Gateron Brown switches
and some blank DSA caps from Amazon.** Total ~$120, you'll have spare
parts for a v2, and everything ships in 2 days. The DigiKey route is
better if you're already placing an order there for other projects.
