// click-clack — Chronos-style chess clock case
// Parametric OpenSCAD.
//
// Shape follows a real Chronos: a steeply TILTED FRONT FACE holding the two
// screens, and a FLAT TOP behind it holding the buttons. Three printed parts,
// all printable flat / floor-down with no supports:
//
//   part = "shell"   -> lower body: floor, front lip, side walls (the Chronos
//                       silhouette), back wall, ridge rib, screw bosses and
//                       button support pillars. Print floor-down.
//   part = "display" -> the tilted screen panel, laid FLAT for printing.
//   part = "top"     -> the flat top button panel, laid FLAT for printing.
//   part = "preview" -> all three assembled (do not print).
//   part = "demo"    -> assembled with representative keycaps (visual only).
//   part = "plate"   -> all three parts arranged flat on one build plate.
//
// Print on a Bambu Lab printer (256^3 bed; this is 205 x 95 mm). PETG or PLA+,
// 0.20 mm, 4 walls, 20-30% gyroid, 5 mm brim on the shell, no supports.
// M3 heat-set inserts in the shell bosses; M3 screws through the panels.

part = "preview";

$fn = 48;

// ---------- overall dimensions ----------
// Sized to the classic Chronos: ~8.25" x 2.75" x 2.5" (210 x 70 x 63 mm) — a
// shallow, fairly upright wedge with a steep screen face and a flat-top strip.
W        = 210;   // width
D        = 70;    // depth (front-to-back)
FRONT_LIP = 10;   // height of the little vertical lip at the very front
H_TOP    = 63;    // height of the flat top (and the back wall)
Y_RIDGE  = 7;     // depth position of the ridge where the two faces meet

WALL     = 2.8;
FLOOR_T  = 3.0;
PANEL_T  = 4.0;   // thickness of the display / top panels
LEDGE    = 2.0;   // (unused spacing reserved)
CLEAR    = 0.3;   // panel fit clearance

// ---------- display panel geometry (the tilted face: B -> C) ----------
B = [-D/2, FRONT_LIP];   // front-lip top
C = [Y_RIDGE, H_TOP];    // ridge
disp_dY  = C[0] - B[0];                 // run (depth)
disp_dZ  = C[1] - B[1];                 // rise (height)
disp_tilt = atan(disp_dZ / disp_dY);    // from horizontal
disp_len  = sqrt(disp_dY*disp_dY + disp_dZ*disp_dZ);
disp_midY = (B[0] + C[0]) / 2;
disp_midZ = (B[1] + C[1]) / 2;

// ---------- flat top geometry (C -> E) ----------
top_depth = D/2 - Y_RIDGE;     // depth of the flat top strip
top_midY  = (Y_RIDGE + D/2) / 2;

// ---------- OLED windows (on the display panel) ----------
OLED_W = 56;
OLED_H = 28;
OLED_GAP_FROM_CENTER = 38;   // centerline to inner edge of each window
OLED_SLOPE_OFFSET = 0;       // shift up/down the slope (panel-local y')

// ---------- MX switch cutouts (on the flat top panel) ----------
MX         = 14.0;
MX_PLATE_T = 1.5;
MX_RELIEF  = 16.0;

// Button positions on the flat top, panel-local [x, y] (y measured from the
// flat-top centre; +y toward the back). Slight Chronos diagonal.
BTN_L = [-60,  4];    // left clock button  (toward back)
BTN_R = [ 60, -4];    // right clock button (toward front)
BTN_C = [  0,  0];    // center button (the red one)
BUTTONS = [BTN_L, BTN_R, BTN_C];

// ---------- USB-C cutout (low on the back wall) ----------
USBC_W = 10;
USBC_H = 4;
USBC_Z = 8;

// ---------- screws / bosses ----------
BOSS_R   = 4.5;
INSERT_R = 2.0;
SCREW_CLEAR_R = 1.8;
CBORE_R  = 3.2;
CBORE_D  = 2.4;

// display-panel screw positions, panel-local [x, y'] (y' along the slope)
DISP_SCREWS = [[-(W/2-9), -(disp_len/2-8)], [ (W/2-9), -(disp_len/2-8)],
               [-(W/2-9),  (disp_len/2-8)], [ (W/2-9),  (disp_len/2-8)]];
// top-panel screw positions, panel-local [x, y]
TOP_SCREWS  = [[-(W/2-9), -(top_depth/2-6)], [ (W/2-9), -(top_depth/2-6)],
               [-(W/2-9),  (top_depth/2-6)], [ (W/2-9),  (top_depth/2-6)]];

// ============================================================
// body silhouette
// ============================================================
profile = [[-D/2, 0], [D/2, 0], [D/2, H_TOP], [Y_RIDGE, H_TOP], [-D/2, FRONT_LIP]];

// solid Chronos body: profile is (depth, height); extrude across the width
module body_solid() {
    translate([-W/2, 0, 0])
        multmatrix([[0,0,1,0],[1,0,0,0],[0,1,0,0],[0,0,0,1]])
            linear_extrude(W) polygon(profile);
}

// place children on the tilted display face: x' across width, y' up the slope,
// z' = 0 on the outer face, +z' outward.
module on_display() {
    translate([0, disp_midY, disp_midZ])
        rotate([disp_tilt, 0, 0])
            children();
}

// place children on the flat top: x across width, y depth-from-top-centre,
// z = 0 on the outer (top) surface, +z up.
module on_top() {
    translate([0, top_midY, H_TOP])
        children();
}

// ============================================================
// panels — flat for printing (z = 0 cosmetic/bed face)
// ============================================================
module oled_window(sx) {
    cx = sx * (OLED_GAP_FROM_CENTER + OLED_W/2);
    translate([cx - OLED_W/2, OLED_SLOPE_OFFSET - OLED_H/2, -1])
        cube([OLED_W, OLED_H, PANEL_T + 2]);
}

module mx_cut(pos) {
    translate([pos[0], pos[1], 0]) {
        translate([-MX/2, -MX/2, -1]) cube([MX, MX, PANEL_T + 2]);
        translate([-MX_RELIEF/2, -MX_RELIEF/2, MX_PLATE_T])
            cube([MX_RELIEF, MX_RELIEF, PANEL_T]);
    }
}

module screw_hole(pos) {
    translate([pos[0], pos[1], -1]) cylinder(h = PANEL_T + 2, r = SCREW_CLEAR_R);
    translate([pos[0], pos[1], -0.01]) cylinder(h = CBORE_D, r = CBORE_R);
}

module display_panel_flat() {
    w = W - 2*CLEAR;
    l = disp_len - 2*CLEAR;
    difference() {
        translate([-w/2, -l/2, 0]) cube([w, l, PANEL_T]);
        oled_window(-1); oled_window(1);
        for (s = DISP_SCREWS) screw_hole(s);
    }
}

module top_panel_flat() {
    w = W - 2*CLEAR;
    l = top_depth - 2*CLEAR;
    difference() {
        translate([-w/2, -l/2, 0]) cube([w, l, PANEL_T]);
        for (b = BUTTONS) mx_cut(b);
        for (s = TOP_SCREWS) screw_hole(s);
    }
}

// panels placed in the assembly (cosmetic face outward, flush with the
// silhouette; mirror puts the flat part's z=0 cosmetic face at z'=0)
module display_panel_placed() {
    on_display() mirror([0,0,1]) display_panel_flat();
}
module top_panel_placed() {
    on_top() mirror([0,0,1]) top_panel_flat();
}

// grown panel solids, used to carve the seats the panels drop into
module display_seat() {
    on_display() translate([0, 0, -PANEL_T])
        linear_extrude(PANEL_T + 6)
            offset(CLEAR) square([W, disp_len], center = true);
}
module top_seat() {
    on_top() translate([0, 0, -PANEL_T])
        linear_extrude(PANEL_T + 6)
            offset(CLEAR) square([W, top_depth], center = true);
}

// ============================================================
// shell internals: bosses, pillars, ridge rib
// ============================================================
// everything below the display-panel underside (to clip display bosses)
module below_display() {
    on_display() translate([0, 0, -PANEL_T - 400]) cube([1200, 1200, 800], center = true);
}

module disp_boss(pos) {
    intersection() {
        on_display() translate([pos[0], pos[1], -PANEL_T - 60]) cylinder(h = 80, r = BOSS_R);
        body_solid();
    }
}
module disp_insert(pos) {
    on_display() translate([pos[0], pos[1], -PANEL_T - 30]) cylinder(h = 30 - 5, r = INSERT_R);
}

module top_boss(pos) {
    intersection() {
        translate([pos[0], top_midY + pos[1], 0]) cylinder(h = H_TOP - PANEL_T, r = BOSS_R);
        body_solid();
    }
}
module top_insert(pos) {
    translate([pos[0], top_midY + pos[1], H_TOP - PANEL_T - 14])
        cylinder(h = 14 + 1, r = INSERT_R);
}

// support pillars right under the flat-top buttons (back the press load)
module button_pillar(b) {
    intersection() {
        translate([b[0], top_midY + b[1], 0]) cylinder(h = H_TOP - PANEL_T, r = BOSS_R);
        body_solid();
    }
}

// transverse rib under the ridge, supports both panel edges; notch for wiring
module ridge_rib() {
    difference() {
        intersection() {
            translate([0, Y_RIDGE, 0])
                translate([-(W/2 - WALL), -WALL/2, 0]) cube([W - 2*WALL, WALL, H_TOP]);
            body_solid();
        }
        translate([-25, Y_RIDGE - WALL, FLOOR_T]) cube([50, 3*WALL, 22]);  // wiring notch
    }
}

// ============================================================
// shell
// ============================================================
module shell() {
    difference() {
        union() {
            // walls = body minus the big interior cavity (open top)
            difference() {
                body_solid();
                // interior: inset by WALL on front/back/sides, open from floor up
                translate([-(W/2 - WALL), -(D/2 - WALL), FLOOR_T])
                    cube([W - 2*WALL, D - 2*WALL, H_TOP + 60]);
            }
            ridge_rib();
            for (s = DISP_SCREWS) disp_boss(s);
            for (s = TOP_SCREWS)  top_boss(s);
            for (b = BUTTONS)     button_pillar(b);
        }
        // seats for the two panels (carve the top PANEL_T off the wall edges)
        display_seat();
        top_seat();
        // insert holes
        for (s = DISP_SCREWS) disp_insert(s);
        for (s = TOP_SCREWS)  top_insert(s);
        // USB-C, low on the back wall
        translate([-USBC_W/2, D/2 - WALL - 0.1, USBC_Z]) cube([USBC_W, WALL + 0.3, USBC_H]);
    }
}

// ============================================================
// keycaps — visual only (part = "demo")
// ============================================================
module cap_round(b, d, h, col) {
    color(col) on_top() translate([b[0], b[1], 0])
        union() {
            cylinder(d1 = d, d2 = d - 1.5, h = h, $fn = 60);
            translate([0,0,h]) scale([1,1,0.22]) sphere(d = d - 1.5, $fn = 60);
        }
}
module cap_square(b, w, h, col) {
    color(col) on_top() translate([b[0], b[1], 0])
        linear_extrude(h) offset(2) square([w-4, w-4], center = true);
}

// ============================================================
// render
// ============================================================
if (part == "shell") {
    shell();
} else if (part == "display") {
    display_panel_flat();
} else if (part == "top") {
    top_panel_flat();
} else if (part == "plate") {
    translate([0, -D/2 - 5, 0]) shell();
    translate([0,  D/2 + disp_len/2 + 8, 0]) display_panel_flat();
    translate([0,  D/2 + disp_len + top_depth/2 + 16, 0]) top_panel_flat();
} else if (part == "demo") {
    color("DimGray")   shell();
    color("Gainsboro") display_panel_placed();
    color("Gainsboro") top_panel_placed();
    cap_round(BTN_L, 24, 9, "Silver");
    cap_round(BTN_R, 24, 9, "Silver");
    cap_square(BTN_C, 15, 7, "Crimson");
} else {
    color("DimGray")   shell();
    color("Gainsboro") display_panel_placed();
    color("Silver")    top_panel_placed();
}
