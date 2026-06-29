// click-clack — Chronos-style chess clock
// ESP32-S3 + 2x SSD1309 SPI OLED + 7x Cherry MX
//
// Operation:
//   IDLE   : showing loaded time control, waiting for first press
//   RUNNING: one side's clock ticking
//   PAUSED : center button pressed mid-game
//   FLAG   : a side ran out of time
//   MENU   : edit time control (base / increment, per-side)
//   OPTIONS: toggle stop_on_flag, beep_on_flag, etc.
//
// Press your own paddle to end your move and start opponent.
// Center button: short = pause/resume, long (>1s) = reset to loaded TC.
// Mode button cycles RUN <-> MENU <-> OPTIONS (only when IDLE/PAUSED).

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

static constexpr int PIN_BTN_P1     = 4;
static constexpr int PIN_BTN_P2     = 5;
static constexpr int PIN_BTN_MODE   = 6;
static constexpr int PIN_BTN_SET    = 7;
static constexpr int PIN_BTN_UP     = 15;
static constexpr int PIN_BTN_DOWN   = 16;
static constexpr int PIN_BTN_CENTER = 17;

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
Btn p1{{}, PIN_BTN_P1}, p2{{}, PIN_BTN_P2},
    bMode{{}, PIN_BTN_MODE}, bSet{{}, PIN_BTN_SET},
    bUp{{}, PIN_BTN_UP}, bDown{{}, PIN_BTN_DOWN},
    bCenter{{}, PIN_BTN_CENTER};

Btn* allBtns[] = {&p1, &p2, &bMode, &bSet, &bUp, &bDown, &bCenter};

// ---------- state ----------
enum class State : uint8_t { IDLE, RUNNING, PAUSED, FLAG, MENU, OPTIONS };
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

// menu cursor
uint8_t menu_field = 0;   // 0..3: P1 base, P1 inc, P2 base, P2 inc
uint8_t opt_field  = 0;   // 0..4

// center-button long-press tracking
uint32_t center_down_ms = 0;
bool     center_long_fired = false;

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

void drawMenu(U8G2& d, int side) {
    d.clearBuffer();
    d.setFont(u8g2_font_6x12_tr);
    d.drawStr(2, 10, side == 0 ? "MENU P1" : "MENU P2");
    char buf[24];
    snprintf(buf, sizeof(buf), "Base: %lu min",
             (unsigned long)(tc.base_ms[side] / 60000));
    d.drawStr(2, 28, buf);
    snprintf(buf, sizeof(buf), "Inc:  %lu sec",
             (unsigned long)(tc.inc_ms[side] / 1000));
    d.drawStr(2, 44, buf);

    // cursor: fields 0,1 belong to P1; 2,3 to P2
    int fieldOnThis = -1;
    if (side == 0 && menu_field <= 1) fieldOnThis = menu_field;
    if (side == 1 && menu_field >= 2) fieldOnThis = menu_field - 2;
    if (fieldOnThis == 0) d.drawStr(118, 28, "<");
    if (fieldOnThis == 1) d.drawStr(118, 44, "<");

    d.drawStr(2, 62, "MODE=exit SET=next");
    d.sendBuffer();
}

void drawOptions(U8G2& d, int side) {
    d.clearBuffer();
    d.setFont(u8g2_font_6x12_tr);
    d.drawStr(2, 10, side == 0 ? "OPTIONS" : "OPTIONS");
    const char* names[5] = {
        "Stop on flag", "Beep on flag", "Beep on move",
        "Low-time warn", "Brightness"
    };
    char vals[5][8];
    snprintf(vals[0], 8, "%s", opt.stop_on_flag  ? "ON"  : "OFF");
    snprintf(vals[1], 8, "%s", opt.beep_on_flag  ? "ON"  : "OFF");
    snprintf(vals[2], 8, "%s", opt.beep_on_move  ? "ON"  : "OFF");
    snprintf(vals[3], 8, "%s", opt.low_time_warn ? "ON"  : "OFF");
    snprintf(vals[4], 8, "%u", opt.brightness);

    // show 3 lines centered on opt_field
    int start = (opt_field <= 1) ? 0 : (opt_field >= 4 ? 2 : opt_field - 1);
    for (int i = 0; i < 3; i++) {
        int idx = start + i;
        if (idx >= 5) break;
        int y = 26 + i * 14;
        char line[28];
        snprintf(line, sizeof(line), "%c%-13s %s",
                 (idx == opt_field) ? '>' : ' ', names[idx], vals[idx]);
        d.drawStr(2, y, line);
    }
    d.sendBuffer();
}

void render() {
    switch (state) {
    case State::MENU:
        drawMenu(oled1, 0);
        drawMenu(oled2, 1);
        break;
    case State::OPTIONS:
        drawOptions(oled1, 0);
        drawOptions(oled2, 1);
        break;
    default:
        drawClockFace(oled1, 0, active == 0);
        drawClockFace(oled2, 1, active == 1);
        break;
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

void cycleMode() {
    // only allow mode change when the game is not actively running
    if (state == State::RUNNING) return;
    switch (state) {
    case State::IDLE:
    case State::PAUSED:
    case State::FLAG:
        state = State::MENU;
        menu_field = 0;
        break;
    case State::MENU:
        state = State::OPTIONS;
        opt_field = 0;
        break;
    case State::OPTIONS:
        savePrefs();
        resetClocks();
        break;
    default: break;
    }
}

void handleSetButton() {
    if (state == State::MENU) {
        menu_field = (menu_field + 1) % 4;
    } else if (state == State::OPTIONS) {
        opt_field = (opt_field + 1) % 5;
    }
}

void handleUpDown(int delta) {
    if (state == State::MENU) {
        int side = menu_field / 2;
        bool isInc = (menu_field % 2) == 1;
        if (isInc) {
            int64_t v = (int64_t)tc.inc_ms[side] + delta * 1000;
            if (v < 0) v = 0;
            if (v > 60000) v = 60000;
            tc.inc_ms[side] = v;
        } else {
            int64_t v = (int64_t)tc.base_ms[side] + delta * 60000;
            if (v < 60000) v = 60000;             // 1 min minimum
            if (v > 180LL * 60 * 1000) v = 180LL * 60 * 1000;  // 3hr max
            tc.base_ms[side] = v;
        }
    } else if (state == State::OPTIONS) {
        switch (opt_field) {
        case 0: opt.stop_on_flag  = !opt.stop_on_flag;  break;
        case 1: opt.beep_on_flag  = !opt.beep_on_flag;  break;
        case 2: opt.beep_on_move  = !opt.beep_on_move;  break;
        case 3: opt.low_time_warn = !opt.low_time_warn; break;
        case 4: {
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
}

void handleCenterPress() {
    if (state == State::RUNNING) {
        state = State::PAUSED;
    } else if (state == State::PAUSED) {
        last_tick_us = esp_timer_get_time();
        state = State::RUNNING;
    }
}

void handleCenterLong() {
    // long-press: hard reset to loaded TC, regardless of state
    savePrefs();
    resetClocks();
    beep(1800, 40);
    delay(50);
    beep(2400, 40);
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
    oled1.setContrast(opt.brightness);
    oled2.setContrast(opt.brightness);

    setupButtons();

    ledcSetup(0, 2000, 8);
    ledcAttachPin(PIN_BUZZER, 0);

    loadPrefs();
    resetClocks();
    last_tick_us = esp_timer_get_time();

    render();
}

void loop() {
    for (Btn* b : allBtns) b->b.update();

    if (p1.b.fell())     onPlayerPress(0);
    if (p2.b.fell())     onPlayerPress(1);
    if (bMode.b.fell())  cycleMode();
    if (bSet.b.fell())   handleSetButton();
    if (bUp.b.fell())    handleUpDown(+1);
    if (bDown.b.fell())  handleUpDown(-1);

    // center button: short vs long press
    if (bCenter.b.fell()) {
        center_down_ms = millis();
        center_long_fired = false;
    }
    if (bCenter.b.read() == LOW && !center_long_fired &&
        (millis() - center_down_ms) > 1000) {
        handleCenterLong();
        center_long_fired = true;
    }
    if (bCenter.b.rose()) {
        if (!center_long_fired) handleCenterPress();
    }

    tickClocks();

    static uint32_t last_render = 0;
    uint32_t now = millis();
    if (now - last_render >= 50) {  // 20 Hz
        last_render = now;
        render();
    }
}
