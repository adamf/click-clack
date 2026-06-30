// click-clack — Chronos-style chess clock
// ESP32-S3 + 2x SSD1309 SPI OLED + 3x Cherry MX
//
// Just three buttons, like a real Chronos:
//   - LEFT / RIGHT  : the two clock buttons (one per player)
//   - CENTER        : the single button between the displays
//
// Play:
//   Tap your own clock button to end your move and start the opponent.
//   Tap CENTER          : pause / resume.
//   Tap CENTER at FLAG  : start a fresh game with the loaded time control.
//
// Settings (no extra buttons — it's all combos on CENTER):
//   Hold CENTER (~1s) while stopped : enter SETUP (a beep confirms).
//   In SETUP:
//     Tap CENTER  : move to the next field
//     Tap LEFT    : decrease the value     (P1 button = "-")
//     Tap RIGHT   : increase the value     (P2 button = "+")
//     Hold CENTER : save, exit, and reset the clocks to the new control
//
// SETUP fields: Preset (applies to both sides), then per-side base/increment,
// then the options (stop-on-flag, beeps, low-time warning, brightness).

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Bounce2.h>
#include <Preferences.h>
#include <esp_timer.h>

// ---------- pins ----------
static constexpr int PIN_SCK   = 12;
static constexpr int PIN_MOSI  = 11;
static constexpr int PIN_DC    = 9;
static constexpr int PIN_RST   = 8;
static constexpr int PIN_CS1   = 10;
static constexpr int PIN_CS2   = 13;

static constexpr int PIN_BTN_P1     = 4;   // left clock button
static constexpr int PIN_BTN_P2     = 5;   // right clock button
static constexpr int PIN_BTN_CENTER = 17;  // single center button

static constexpr int PIN_BUZZER = 18;
static constexpr int PIN_VBAT   = 1;

// ---------- displays ----------
// Two SSD1309 panels sharing SCK/MOSI/DC/RST, separate CS.
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI oled1(
    U8G2_R0, /*cs=*/PIN_CS1, /*dc=*/PIN_DC, /*reset=*/PIN_RST);
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI oled2(
    U8G2_R0, /*cs=*/PIN_CS2, /*dc=*/PIN_DC, /*reset=*/U8X8_PIN_NONE);

// ---------- buttons ----------
struct Btn { Bounce b; int pin; };
Btn p1{{}, PIN_BTN_P1}, p2{{}, PIN_BTN_P2}, bCenter{{}, PIN_BTN_CENTER};
Btn* allBtns[] = {&p1, &p2, &bCenter};

// ---------- state ----------
enum class State : uint8_t { IDLE, RUNNING, PAUSED, FLAG, SETUP };
State state = State::IDLE;

// time control: per-side base (ms) and increment (ms)
struct TC {
    uint32_t base_ms[2];
    uint32_t inc_ms[2];
};
TC tc;                 // currently loaded
int64_t remain_us[2];  // live remaining microseconds
uint8_t  active = 0;   // 0=P1, 1=P2 (the side whose clock is ticking)
bool     flagged[2] = {false, false};
int64_t  last_tick_us = 0;

// options
struct Options {
    bool stop_on_flag   = true;
    bool beep_on_flag   = true;
    bool beep_on_move   = false;
    bool low_time_warn  = true;
    uint8_t brightness  = 200;
} opt;

Preferences prefs;

// ---------- setup mode ----------
// Common symmetric time controls {base minutes, increment seconds}.
static const uint16_t PRESETS[][2] = {
    {1, 0}, {2, 1}, {3, 0}, {3, 2}, {5, 0}, {5, 3},
    {10, 0}, {10, 5}, {15, 10}, {25, 0}, {30, 0}, {60, 0},
};
static const int N_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);
static const int N_FIELDS  = 10;   // see fieldText()

uint8_t setup_field = 0;
int     preset_idx  = 2;   // 3+0 by default

// center-button press tracking
uint32_t center_down_ms = 0;
bool     center_long_fired = false;
static constexpr uint32_t HOLD_MS = 900;

// ---------- helpers ----------
void beep(uint16_t freq, uint16_t ms) {
    ledcWriteTone(0, freq);
    delay(ms);
    ledcWriteTone(0, 0);
}

void loadPrefs() {
    prefs.begin("clack", false);
    tc.base_ms[0] = prefs.getULong("b0", 3UL * 60 * 1000);   // 3+0 default
    tc.base_ms[1] = prefs.getULong("b1", 3UL * 60 * 1000);
    tc.inc_ms[0]  = prefs.getULong("i0", 0);
    tc.inc_ms[1]  = prefs.getULong("i1", 0);
    opt.stop_on_flag  = prefs.getBool("sof", true);
    opt.beep_on_flag  = prefs.getBool("bof", true);
    opt.beep_on_move  = prefs.getBool("bom", false);
    opt.low_time_warn = prefs.getBool("ltw", true);
    opt.brightness    = prefs.getUChar("br", 200);
}

void savePrefs() {
    prefs.putULong("b0", tc.base_ms[0]);
    prefs.putULong("b1", tc.base_ms[1]);
    prefs.putULong("i0", tc.inc_ms[0]);
    prefs.putULong("i1", tc.inc_ms[1]);
    prefs.putBool("sof", opt.stop_on_flag);
    prefs.putBool("bof", opt.beep_on_flag);
    prefs.putBool("bom", opt.beep_on_move);
    prefs.putBool("ltw", opt.low_time_warn);
    prefs.putUChar("br", opt.brightness);
}

void resetClocks() {
    remain_us[0] = (int64_t)tc.base_ms[0] * 1000;
    remain_us[1] = (int64_t)tc.base_ms[1] * 1000;
    flagged[0] = flagged[1] = false;
    state = State::IDLE;
}

// format ms as MM:SS or H:MM:SS or with tenths under 20s
void fmtTime(int64_t us, char* out, size_t n) {
    if (us < 0) us = 0;
    uint32_t total_ms = us / 1000;
    uint32_t s = total_ms / 1000;
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sec = s % 60;
    if (us < 20LL * 1000 * 1000) {
        uint32_t tenths = (total_ms % 1000) / 100;
        snprintf(out, n, "%lu.%lu", (unsigned long)s, (unsigned long)tenths);
    } else if (h > 0) {
        snprintf(out, n, "%lu:%02lu:%02lu",
                 (unsigned long)h, (unsigned long)m, (unsigned long)sec);
    } else {
        snprintf(out, n, "%lu:%02lu", (unsigned long)m, (unsigned long)sec);
    }
}

// ---------- drawing ----------
void drawClockFace(U8G2& d, int side, bool isActive) {
    d.clearBuffer();
    char buf[16];
    fmtTime(remain_us[side], buf, sizeof(buf));

    // big time, centered
    d.setFont(u8g2_font_logisoso32_tn);
    int w = d.getStrWidth(buf);
    int x = (128 - w) / 2;
    d.drawStr(x, 46, buf);

    // top label
    d.setFont(u8g2_font_6x10_tr);
    const char* label = (side == 0) ? "P1" : "P2";
    d.drawStr(2, 9, label);

    // tc summary right
    char tcbuf[16];
    snprintf(tcbuf, sizeof(tcbuf), "%lu+%lu",
             (unsigned long)(tc.base_ms[side] / 60000),
             (unsigned long)(tc.inc_ms[side] / 1000));
    int tw = d.getStrWidth(tcbuf);
    d.drawStr(128 - tw - 2, 9, tcbuf);

    if (flagged[side]) {
        d.drawStr(45, 62, "FLAG");
    } else if (state == State::PAUSED) {
        d.drawStr(45, 62, "PAUSE");
    } else if (isActive && state == State::RUNNING) {
        d.drawBox(0, 60, 128, 4);
    }
    d.sendBuffer();
}

// one line of text for a SETUP field
void fieldText(int i, char* out, size_t n) {
    switch (i) {
    case 0:
        snprintf(out, n, "Preset: %u+%u",
                 PRESETS[preset_idx][0], PRESETS[preset_idx][1]);
        break;
    case 1: snprintf(out, n, "L base: %lu min", (unsigned long)(tc.base_ms[0] / 60000)); break;
    case 2: snprintf(out, n, "L inc:  %lu sec", (unsigned long)(tc.inc_ms[0]  / 1000));  break;
    case 3: snprintf(out, n, "R base: %lu min", (unsigned long)(tc.base_ms[1] / 60000)); break;
    case 4: snprintf(out, n, "R inc:  %lu sec", (unsigned long)(tc.inc_ms[1]  / 1000));  break;
    case 5: snprintf(out, n, "Stop flag: %s", opt.stop_on_flag  ? "ON" : "OFF"); break;
    case 6: snprintf(out, n, "Beep flag: %s", opt.beep_on_flag  ? "ON" : "OFF"); break;
    case 7: snprintf(out, n, "Beep move: %s", opt.beep_on_move  ? "ON" : "OFF"); break;
    case 8: snprintf(out, n, "Low warn:  %s", opt.low_time_warn ? "ON" : "OFF"); break;
    case 9: snprintf(out, n, "Bright: %u", opt.brightness); break;
    default: out[0] = '\0'; break;
    }
}

void drawSetup(U8G2& d) {
    d.clearBuffer();
    d.setFont(u8g2_font_6x12_tr);
    d.drawStr(2, 10, "SETUP");

    // a window of three fields centred on the cursor
    int start = (setup_field <= 1) ? 0
              : (setup_field >= N_FIELDS - 1) ? N_FIELDS - 3
              : setup_field - 1;
    for (int i = 0; i < 3; i++) {
        int idx = start + i;
        if (idx < 0 || idx >= N_FIELDS) continue;
        char field[24], line[28];
        fieldText(idx, field, sizeof(field));
        snprintf(line, sizeof(line), "%c%s", (idx == setup_field) ? '>' : ' ', field);
        d.drawStr(2, 26 + i * 13, line);
    }

    d.setFont(u8g2_font_4x6_tr);
    d.drawStr(2, 63, "P1:-  P2:+  C:next  holdC:save");
    d.sendBuffer();
}

void render() {
    if (state == State::SETUP) {
        drawSetup(oled1);
        drawSetup(oled2);
    } else {
        drawClockFace(oled1, 0, active == 0);
        drawClockFace(oled2, 1, active == 1);
    }
}

// ---------- ticking ----------
void tickClocks() {
    int64_t now = esp_timer_get_time();
    int64_t dt = now - last_tick_us;
    last_tick_us = now;
    if (state != State::RUNNING) return;
    remain_us[active] -= dt;

    // low-time warnings (10s, 5s) on the active side
    if (opt.low_time_warn) {
        static int last_warn[2] = {-1, -1};
        int64_t s = remain_us[active] / 1000000;
        if ((s == 10 || s == 5) && last_warn[active] != (int)s) {
            last_warn[active] = s;
            beep(2200, 60);
        }
        if (remain_us[active] > 12LL * 1000000) last_warn[active] = -1;
    }

    if (remain_us[active] <= 0) {
        remain_us[active] = 0;
        flagged[active] = true;
        if (opt.beep_on_flag) beep(1500, 250);
        if (opt.stop_on_flag) {
            state = State::FLAG;
        }
        // if not stopping on flag, the clock just sits at 0 and the
        // opponent can keep playing — Chronos "no-stop" behavior
    }
}

// ---------- input handling ----------
void onPlayerPress(int side) {
    if (state == State::IDLE) {
        // first press starts the OPPONENT's clock (you press to end your move)
        active = 1 - side;
        last_tick_us = esp_timer_get_time();
        state = State::RUNNING;
        if (opt.beep_on_move) beep(2000, 20);
        return;
    }
    if (state == State::RUNNING && active == side) {
        // your move ended: add your increment, switch
        remain_us[side] += (int64_t)tc.inc_ms[side] * 1000;
        active = 1 - side;
        if (opt.beep_on_move) beep(2000, 20);
    }
}

void adjBase(int side, int delta) {
    int64_t v = (int64_t)tc.base_ms[side] + delta * 60000;
    if (v < 60000) v = 60000;                          // 1 min min
    if (v > 180LL * 60 * 1000) v = 180LL * 60 * 1000;  // 3 hr max
    tc.base_ms[side] = v;
}
void adjInc(int side, int delta) {
    int64_t v = (int64_t)tc.inc_ms[side] + delta * 1000;
    if (v < 0) v = 0;
    if (v > 60000) v = 60000;
    tc.inc_ms[side] = v;
}

// edit the current SETUP field; delta is -1 (left) or +1 (right)
void editField(int delta) {
    switch (setup_field) {
    case 0:
        preset_idx = (preset_idx + delta + N_PRESETS) % N_PRESETS;
        tc.base_ms[0] = tc.base_ms[1] = (uint32_t)PRESETS[preset_idx][0] * 60000UL;
        tc.inc_ms[0]  = tc.inc_ms[1]  = (uint32_t)PRESETS[preset_idx][1] * 1000UL;
        break;
    case 1: adjBase(0, delta); break;
    case 2: adjInc(0, delta);  break;
    case 3: adjBase(1, delta); break;
    case 4: adjInc(1, delta);  break;
    case 5: opt.stop_on_flag  = !opt.stop_on_flag;  break;
    case 6: opt.beep_on_flag  = !opt.beep_on_flag;  break;
    case 7: opt.beep_on_move  = !opt.beep_on_move;  break;
    case 8: opt.low_time_warn = !opt.low_time_warn; break;
    case 9: {
        int v = (int)opt.brightness + delta * 16;
        if (v < 16) v = 16;
        if (v > 255) v = 255;
        opt.brightness = v;
        oled1.setContrast(opt.brightness);
        oled2.setContrast(opt.brightness);
        break;
    }
    }
}

// CENTER, short tap — meaning depends on the current state
void centerTap() {
    switch (state) {
    case State::SETUP:
        setup_field = (setup_field + 1) % N_FIELDS;
        break;
    case State::RUNNING:
        state = State::PAUSED;
        break;
    case State::PAUSED:
        last_tick_us = esp_timer_get_time();
        state = State::RUNNING;
        break;
    case State::FLAG:
    case State::IDLE:
        resetClocks();   // start a fresh game with the loaded control
        break;
    }
}

// CENTER, held past HOLD_MS
void centerHold() {
    if (state == State::SETUP) {
        savePrefs();
        resetClocks();
        beep(2400, 40); delay(40); beep(1800, 40);   // "saved" chirp
    } else if (state != State::RUNNING) {
        state = State::SETUP;
        setup_field = 0;
        beep(1800, 40); delay(40); beep(2400, 40);    // "entering" chirp
    }
    // while RUNNING a hold does nothing — a tap pauses instead
}

// ---------- setup / loop ----------
void setupButtons() {
    for (Btn* b : allBtns) {
        pinMode(b->pin, INPUT_PULLUP);
        b->b.attach(b->pin);
        b->b.interval(5);
    }
}

void setup() {
    Serial.begin(115200);

    SPI.begin(PIN_SCK, -1, PIN_MOSI, -1);

    oled1.begin();
    oled2.begin();

    setupButtons();

    ledcSetup(0, 2000, 8);
    ledcAttachPin(PIN_BUZZER, 0);

    loadPrefs();
    oled1.setContrast(opt.brightness);
    oled2.setContrast(opt.brightness);
    resetClocks();
    last_tick_us = esp_timer_get_time();

    render();
}

void loop() {
    for (Btn* b : allBtns) b->b.update();

    // clock buttons: edit values in SETUP, otherwise play
    if (p1.b.fell()) { if (state == State::SETUP) editField(-1); else onPlayerPress(0); }
    if (p2.b.fell()) { if (state == State::SETUP) editField(+1); else onPlayerPress(1); }

    // center button: short tap vs long hold
    if (bCenter.b.fell()) {
        center_down_ms = millis();
        center_long_fired = false;
    }
    if (bCenter.b.read() == LOW && !center_long_fired &&
        (millis() - center_down_ms) > HOLD_MS) {
        centerHold();
        center_long_fired = true;
    }
    if (bCenter.b.rose() && !center_long_fired) {
        centerTap();
    }

    tickClocks();

    static uint32_t last_render = 0;
    uint32_t now = millis();
    if (now - last_render >= 50) {  // 20 Hz
        last_render = now;
        render();
    }
}
