// click-clack — Chronos-style chess clock case
// Parametric. Render top or bottom by setting `part`.
//
//   part = "bottom"  -> lower shell with battery bay & USB-C cutout
//   part = "top"     -> angled top plate with OLED windows & MX cutouts
//   part = "preview" -> both, assembled, for visual check (don't print)
//
// Units: mm. Print at 0.2mm layer height, 3 perimeters, 20% infill.
// Use M3 heat-set inserts in the four corner posts.

part = "preview";   // "bottom" | "top" | "preview"

// ---------- main dimensions ----------
W       = 220;   // overall width
D       = 90;    // overall depth (front-to-back)
H_back  = 55;    // height at the back (tall side of the wedge)
H_front = 30;    // height at the front (short side)
WALL    = 2.4;
TOP_T   = 3.0;   // top plate thickness
FLOOR_T = 2.4;

// ---------- OLED window ----------
// 2.42" SSD1309 active area is ~55.0 x 27.5 mm. Module PCB ~65 x 38 mm.
OLED_W      = 56;
OLED_H      = 28;
OLED_PCB_W  = 66;
OLED_PCB_H  = 39;
OLED_GAP_FROM_CENTER = 38;  // distance from case centerline to inner edge of each OLED
OLED_Y_OFFSET = 8;          // shift toward back of case

// ---------- MX switch cutout ----------
// Standard Cherry MX plate cutout: 14.0 x 14.0 mm square.
MX = 14.0;
MX_PLATE_T = 1.5;   // ideal switch-clip plate thickness; we recess this from underside

// Player paddles: large keycap area, switch positioned where the cap will sit
PLAYER_X_FROM_CENTER = 75;  // each paddle switch X offset
PLAYER_Y = -22;             // toward front

// Center small reset
CENTER_X = 0;
CENTER_Y = -22;

// Aux row (mode/set/up/down) along the back edge
AUX_Y = 18;
AUX_SPACING = 18;
AUX_X = [-AUX_SPACING*1.5, -AUX_SPACING*0.5, AUX_SPACING*0.5, AUX_SPACING*1.5];

// ---------- USB-C cutout (back wall) ----------
USBC_W = 10;
USBC_H = 4;
USBC_Z = 12;  // height above floor

// ---------- mounting posts ----------
POST_R = 4;
POST_INSET = 6;
INSERT_R = 2.1;   // for M3 heat-set

// ============================================================
// helpers
// ============================================================
module wedge_solid() {
    // wedge: rectangular footprint, top face slopes from H_back at +Y to H_front at -Y
    polyhedron(
        points = [
            [-W/2, -D/2, 0],          // 0
            [ W/2, -D/2, 0],          // 1
            [ W/2,  D/2, 0],          // 2
            [-W/2,  D/2, 0],          // 3
            [-W/2, -D/2, H_front],    // 4
            [ W/2, -D/2, H_front],    // 5
            [ W/2,  D/2, H_back],     // 6
            [-W/2,  D/2, H_back],     // 7
        ],
        faces = [
            [0,1,2,3],
            [4,5,1,0],
            [6,2,1,5],
            [7,3,2,6],
            [4,0,3,7],
            [7,6,5,4],
        ]
    );
}

module corner_posts(h) {
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([sx*(W/2 - POST_INSET), sy*(D/2 - POST_INSET), 0])
            cylinder(h=h, r=POST_R, $fn=32);
}

module corner_post_holes(h) {
    for (sx = [-1, 1]) for (sy = [-1, 1])
        translate([sx*(W/2 - POST_INSET), sy*(D/2 - POST_INSET), -0.1])
            cylinder(h=h+0.2, r=INSERT_R, $fn=24);
}

// ============================================================
// bottom shell
// ============================================================
module bottom() {
    difference() {
        union() {
            // outer wedge
            wedge_solid();
            // posts inside
            intersection() {
                corner_posts(H_back);
                wedge_solid();
            }
        }
        // hollow it out, leaving FLOOR_T floor and WALL walls
        translate([0, 0, FLOOR_T])
            scale([(W - 2*WALL)/W, (D - 2*WALL)/D, 1])
                wedge_solid();
        // remove top: a generous chunk above the wedge top minus TOP_T
        // (the top plate sits in this recess)
        translate([0, 0, 0])
            translate([-W/2 - 1, -D/2 - 1, 0])
                cube([W + 2, D + 2, 1000])  // dummy, replaced below
                ;
        // USB-C cutout in back wall
        translate([-USBC_W/2, D/2 - WALL - 0.1, USBC_Z])
            cube([USBC_W, WALL + 0.2, USBC_H]);
        // post holes for inserts
        corner_post_holes(H_back);
    }
}

// Cleaner bottom: subtract an inner wedge offset inward + downward.
module bottom_clean() {
    difference() {
        wedge_solid();
        // inner cavity
        translate([0, 0, FLOOR_T])
            linear_extrude(height = H_back)  // tall enough
                offset(r = -WALL)
                    square([W, D], center=true);
        // back-wall USB cutout
        translate([-USBC_W/2, D/2 - WALL - 0.1, USBC_Z])
            cube([USBC_W, WALL + 0.2, USBC_H]);
        // recess for top plate to drop into (top TOP_T of the wedge)
        translate([0, 0, 0])
            difference() {
                translate([0,0,-0.1]) wedge_solid_shifted_down(TOP_T + 0.1);
                wedge_solid();
            }
        corner_post_holes(H_back);
    }
    // posts (clipped to wedge interior)
    intersection() {
        corner_posts(H_back - TOP_T);
        wedge_solid();
    }
}

// helper: wedge translated up (used to carve top recess)
module wedge_solid_shifted_down(dz) {
    translate([0,0,-dz]) wedge_solid();
}

// ============================================================
// top plate (angled face of the wedge)
// ============================================================
// The top plate is a flat slab oriented to match the wedge's slope.
// Slope angle:
slope_deg = atan((H_back - H_front) / D);

module top_plate_flat() {
    // Build the plate flat in XY (W x D_slope x TOP_T), then rotate into place.
    D_slope = D / cos(slope_deg);
    difference() {
        translate([-W/2, -D_slope/2, 0])
            cube([W, D_slope, TOP_T]);
        // OLED windows (cut through)
        for (sx = [-1, 1])
            translate([sx * OLED_GAP_FROM_CENTER + sx*OLED_W/2, OLED_Y_OFFSET, -0.1])
                translate([-OLED_W/2, -OLED_H/2, 0])
                    cube([OLED_W, OLED_H, TOP_T + 0.2]);
        // MX switch cutouts (square 14x14)
        // Player 1 (left)
        mx_cutout(-PLAYER_X_FROM_CENTER, PLAYER_Y);
        // Player 2 (right)
        mx_cutout( PLAYER_X_FROM_CENTER, PLAYER_Y);
        // Center small button
        mx_cutout(CENTER_X, CENTER_Y);
        // Aux row (mode, set, up, down)
        for (x = AUX_X) mx_cutout(x, AUX_Y);
        // post screw holes
        for (sx = [-1, 1]) for (sy = [-1, 1])
            translate([sx*(W/2 - POST_INSET), sy*(D/2 - POST_INSET) / cos(slope_deg), -0.1])
                cylinder(h=TOP_T + 0.2, r=1.7, $fn=24);
    }
}

module mx_cutout(x, y) {
    translate([x - MX/2, y - MX/2, -0.1])
        cube([MX, MX, TOP_T + 0.2]);
}

module top_plate_placed() {
    // tilt the flat plate so its surface lies along the wedge top
    translate([0, 0, H_front - TOP_T*cos(slope_deg)])
        rotate([slope_deg, 0, 0])
            top_plate_flat();
}

// ============================================================
// render
// ============================================================
if (part == "bottom") {
    bottom_clean();
} else if (part == "top") {
    // print flat
    top_plate_flat();
} else {
    // preview assembled
    color("DimGray") bottom_clean();
    color("Gainsboro") top_plate_placed();
}
