// click-clack — Chronos-style chess clock case
// Parametric OpenSCAD. Two printed parts: lower shell + angled top plate.
//
//   part = "bottom"  -> lower shell: battery bay, USB-C cutout, support
//                       pillars and screw posts. Print floor-down.
//   part = "top"     -> angled top plate, laid FLAT for printing.
//                       Cosmetic face is on the bed; switch reliefs face up.
//   part = "preview" -> both, assembled, for visual check (do not print).
//   part = "plate"   -> both parts arranged flat on one build plate.
//   part = "demo"    -> assembled with representative keycaps (visual only).
//
// ---------------------------------------------------------------------------
// Printing on a Bambu Lab printer (X1 / X1C / P1S / P1P / A1)
// ---------------------------------------------------------------------------
// Build volume 256 x 256 x 256 mm. This case is 220 x 90 mm, so it fits with
// room to spare. (The A1 *mini* at 180 x 180 is too small for the 220 mm
// width — split the model or use a full-size machine.)
//
// Recommended Bambu Studio settings:
//   * Filament  : PETG or PLA+ (PETG preferred — a chess clock gets slapped,
//                 and PETG is tougher and more impact-resistant than plain PLA)
//   * Layer     : 0.20 mm
//   * Walls     : 4 perimeters (the top plate takes the button load)
//   * Infill    : 20-30% gyroid
//   * Supports  : NONE needed. The USB-C cutout is a short 10 mm bridge, the
//                 switch reliefs open upward, and all posts are vertical.
//   * Adhesion  : 5 mm brim on the bottom shell (220 mm of flat footprint can
//                 lift at the corners without one).
//   * Inserts   : M3 brass heat-set inserts in the screw posts; M3 screws
//                 (8-10 mm) drop through the counterbores in the top plate.
// ---------------------------------------------------------------------------

part = "preview";   // "bottom" | "top" | "preview" | "plate"

$fn = 48;

// ---------- main dimensions ----------
W       = 220;   // overall width
D       = 90;    // overall depth (front-to-back)
H_back  = 55;    // height at the back (tall side of the wedge)
H_front = 30;    // height at the front (short side)
WALL    = 2.8;   // shell wall thickness
TOP_T   = 4.0;   // top plate thickness (was 3.0 — thicker resists button load)
FLOOR_T = 3.0;   // shell floor thickness

// wedge slope (top face rises from front to back)
slope   = atan((H_back - H_front) / D);
D_slope = D / cos(slope);   // top-plate depth measured along the slope

// ---------- top-plate seating ----------
LID_CLEAR = 0.3;   // gap around the lid so it drops into the opening
LEDGE_W   = 2.5;   // width of the perimeter ledge the top plate rests on

// ---------- OLED window ----------
// 2.42" SSD1309 active area is ~55.0 x 27.5 mm. Module PCB ~65 x 38 mm.
OLED_W      = 56;
OLED_H      = 28;
OLED_GAP_FROM_CENTER = 38;  // case centerline to inner edge of each OLED
OLED_Y_OFFSET = -6;         // displays sit front-and-centre (world Y)

// ---------- MX switch cutout ----------
// Cherry MX plate mount: 14.0 x 14.0 mm square hole in a 1.5 mm plate.
// The plate here is thicker, so the underside is relieved to leave a 1.5 mm
// land the switch clips grab onto.
MX          = 14.0;   // plate hole
MX_PLATE_T  = 1.5;    // land thickness the clips snap against
MX_RELIEF   = 16.0;   // underside relief so the clips can flex

// Switch layout — Chronos style: just three buttons. The two clock buttons
// sit diagonally (left toward the back, right toward the front) for big round
// keycaps; one center button drives every setting via press-and-hold combos.
P1_POS     = [-66,  26];   // left clock button  (back-left)
P2_POS     = [ 66, -30];   // right clock button (front-right)
CENTER_POS = [  0,  26];   // single center button (the "red square")
SWITCHES   = [P1_POS, P2_POS, CENTER_POS];

// ---------- USB-C cutout (lower back wall) ----------
USBC_W = 10;
USBC_H = 4;
USBC_Z = 8;    // height above floor — low on the back, by the charge board

// ---------- posts ----------
// Screw posts take a heat-set insert and a screw from the top plate.
// Support pillars just back up the plate under the load (no screw) — these sit
// right behind each clock button so a hard press transfers straight to the
// floor instead of flexing the plate.
POST_R   = 4.5;
INSERT_R = 2.0;    // M3 heat-set insert (~4.0 mm hole)
CX = W/2 - 6;      // post X at the corners
CY = D/2 - 6;      // post Y at front/back edges
SCREW_POSTS   = [[-CX,-CY], [CX,-CY], [-CX,CY], [CX,CY], [0,-CY], [0,CY]];
SUPPORT_POSTS = [[-66, 38], [66, -38]];   // behind P1, in front of P2

// top-plate screw holes
SCREW_CLEAR_R = 1.8;   // M3 clearance
CBORE_R       = 3.2;   // counterbore for the screw head
CBORE_D       = 2.4;

// ============================================================
// primitives
// ============================================================
module wedge_solid() {
    polyhedron(
        points = [
            [-W/2, -D/2, 0], [ W/2, -D/2, 0], [ W/2,  D/2, 0], [-W/2,  D/2, 0],
            [-W/2, -D/2, H_front], [ W/2, -D/2, H_front],
            [ W/2,  D/2, H_back],  [-W/2,  D/2, H_back],
        ],
        faces = [
            [0,1,2,3], [4,5,1,0], [6,2,1,5], [7,3,2,6], [4,0,3,7], [7,6,5,4],
        ]
    );
}

// Place children in the top-plate plane: z'=0 lies on the sloped top face,
// x' runs across the width, y' runs up the slope, z' is the outward normal.
module on_top_plane() {
    translate([0, 0, (H_front + H_back) / 2])
        rotate([slope, 0, 0])
            children();
}

// Everything at or below the underside of the top plate (used to clip posts).
module below_lid() {
    on_top_plane()
        translate([0, 0, -TOP_T - 500])
            cube([2000, 2000, 1000], center = true);
}

// ============================================================
// bottom shell
// ============================================================
module post(p) {
    intersection() {
        translate([p[0], p[1], 0]) cylinder(h = H_back + 10, r = POST_R);
        below_lid();
        wedge_solid();
    }
}

module bottom() {
    difference() {
        union() {
            // Hollow wedge shell with a stepped opening: the lower cavity is
            // inset by WALL+LEDGE_W, but the top TOP_T (the lid pocket) is inset
            // by only WALL. The step between them is a continuous ledge, TOP_T
            // below the rim, that carries the whole perimeter of the top plate.
            difference() {
                wedge_solid();
                // lower cavity (forms the ledge shelf at its top)
                translate([0, 0, FLOOR_T])
                    linear_extrude(H_back + 20)
                        offset(r = -(WALL + LEDGE_W))
                            square([W, D], center = true);
                // lid pocket: open the top TOP_T to lid size, following the slope
                on_top_plane()
                    translate([0, 0, -TOP_T])
                        linear_extrude(TOP_T + 50)
                            offset(r = -WALL)
                                square([W, D_slope], center = true);
            }
            for (p = SCREW_POSTS)   post(p);
            for (p = SUPPORT_POSTS) post(p);
        }
        // USB-C cutout in the back wall
        translate([-USBC_W/2, D/2 - WALL - LEDGE_W - 0.1, USBC_Z])
            cube([USBC_W, WALL + LEDGE_W + 0.3, USBC_H]);
        // heat-set insert holes (screw posts only), drilled from the top
        for (p = SCREW_POSTS)
            translate([p[0], p[1], FLOOR_T + 1.5])
                cylinder(h = H_back + 20, r = INSERT_R);
    }
}

// ============================================================
// top plate (printed flat: cosmetic face on the bed at z = 0)
// ============================================================
module mx_hole(x, y) {
    yy = y / cos(slope);   // world Y -> slope-frame Y
    translate([x, yy, 0]) {
        // through hole for the switch body
        translate([-MX/2, -MX/2, -0.1])
            cube([MX, MX, TOP_T + 0.2]);
        // underside relief, leaving a 1.5 mm clip land at the cosmetic face
        translate([-MX_RELIEF/2, -MX_RELIEF/2, MX_PLATE_T])
            cube([MX_RELIEF, MX_RELIEF, TOP_T]);
    }
}

module oled_hole(sx) {
    cx = sx * (OLED_GAP_FROM_CENTER + OLED_W/2);
    cy = OLED_Y_OFFSET / cos(slope);
    translate([cx - OLED_W/2, cy - OLED_H/2, -0.1])
        cube([OLED_W, OLED_H, TOP_T + 0.2]);
}

module top_plate_flat() {
    lw = W - 2*WALL - 2*LID_CLEAR;
    ld = D_slope - 2*WALL - 2*LID_CLEAR;
    difference() {
        translate([-lw/2, -ld/2, 0]) cube([lw, ld, TOP_T]);
        oled_hole(-1); oled_hole(1);
        for (s = SWITCHES) mx_hole(s[0], s[1]);
        for (p = SCREW_POSTS) {
            yy = p[1] / cos(slope);
            translate([p[0], yy, -0.1]) cylinder(h = TOP_T + 0.2, r = SCREW_CLEAR_R);
            translate([p[0], yy, -0.1]) cylinder(h = CBORE_D + 0.1, r = CBORE_R);
        }
    }
}

module top_plate_placed() {
    on_top_plane()
        mirror([0, 0, 1])   // cosmetic face outward, reliefs facing into the case
            top_plate_flat();
}

// ============================================================
// keycaps — visualisation only (part = "demo"), never printed
// ============================================================
// Big round caps on the clock buttons (Chronos-style), a red cap on center.
module cap_round(pos, d, h, col) {
    color(col)
        on_top_plane()
            translate([pos[0], pos[1] / cos(slope), 0])
                union() {
                    cylinder(d1 = d, d2 = d - 1.5, h = h, $fn = 60);
                    translate([0, 0, h]) scale([1, 1, 0.22]) sphere(d = d - 1.5, $fn = 60);
                }
}
module cap_square(pos, w, h, col) {
    color(col)
        on_top_plane()
            translate([pos[0], pos[1] / cos(slope), 0])
                linear_extrude(h) offset(r = 2) square([w - 4, w - 4], center = true);
}
module mx_top(pos) {   // hint of the black MX switch housing under the cap
    color("#1b1b1b")
        on_top_plane()
            translate([pos[0], pos[1] / cos(slope), 0])
                translate([-7.8, -7.8, 0]) cube([15.6, 15.6, 1.2]);
}

// ============================================================
// render
// ============================================================
if (part == "bottom") {
    bottom();
} else if (part == "top") {
    top_plate_flat();
} else if (part == "plate") {
    // both parts flat on one build plate
    translate([0, -D/2 - 5, 0]) bottom();
    translate([0,  D_slope/2 + 5, 0]) top_plate_flat();
} else if (part == "demo") {
    // assembled, with representative keycaps fitted (not a printable part)
    color("DimGray")   bottom();
    color("Gainsboro") top_plate_placed();
    for (s = SWITCHES) mx_top(s);
    cap_round(P1_POS, 24, 9, "Silver");
    cap_round(P2_POS, 24, 9, "Silver");
    cap_square(CENTER_POS, 15, 7, "Crimson");
} else {
    color("DimGray")  bottom();
    color("Gainsboro") top_plate_placed();
}
