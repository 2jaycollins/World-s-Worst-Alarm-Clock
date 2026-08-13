#include "clock.h"
#include "troll_events.h"   // drawTrollOverlay -- background trolls paint over the clock

bool showDate = true;
bool showAMPM = true;
int  tempMode = TEMP_HIGH;

bool tempShown() { return tempMode != TEMP_OFF; }

// Short label for the Display menu row.
const char* tempModeLabel() {
    switch (tempMode) {
        case TEMP_OFF:     return "Off";
        case TEMP_CURRENT: return "Curr";
        default:           return "High";
    }
}

static int  lastDrawnHour   = -1;
static int  lastDrawnMinute = -1;
static bool forceRedraw     = false;

// ============================================================================
//  RENDERING
// ============================================================================

// Has anything changed since the last paint? The clock only ticks once a
// minute, so most frames do no work at all. A ringing or snoozed alarm keeps
// returning true, since both animate.
bool clockNeedsRedraw() {
    int hour   = getDisplayHour();
    int minute = getDisplayMinute();

    if (forceRedraw || hour != lastDrawnHour || minute != lastDrawnMinute
        || snoozeActive || alarmActive) {
        lastDrawnHour   = hour;
        lastDrawnMinute = minute;
        forceRedraw     = false;
        return true;
    }
    return false;
}

void forceClockRedraw() {
    forceRedraw = true;
}

// The normal clock screen, repainted only when something has changed.
void displayClockScreen() {
    if (!clockNeedsRedraw()) return;

    u8g2.clearBuffer();
    drawClockFace();
    drawTrollOverlay();   // a background troll draws its scene on top
    u8g2.sendBuffer();
}

// The face itself, with no buffer management. Overlay trolls call this to put
// the real clock underneath their animation.
void drawClockFace() {
    displayWeather();
    drawClock();
    drawDate();
    drawAlarmIndicator();
    drawAMPM();
}

// The clock, flashing. The flash toggles the panel's hardware inverse rather
// than redrawing anything, so we only resend on a phase change.
void displayAlarmClock() {
    displayClockScreen();

    static int lastPhase = -1;
    int phase = (millis() / ALARM_FLASH_MS) % 2;
    if (phase != lastPhase) {
        lastPhase = phase;
        u8g2.sendF("c", phase ? 0xA7 : 0xA6);   // SSD1309: inverse / normal
    }
}

void clearAlarmInvert() {
    u8g2.sendF("c", 0xA6);
}

// ============================================================================
//  THE FOUR ELEMENTS
// ============================================================================

// Weather, bottom right. In TEMP_CURRENT it is just the current reading; in
// TEMP_HIGH it is the current reading paired with the day's high, switching to
// tomorrow's high in the evening once today's is history.
void displayWeather() {
    if (!tempShown()) return;
    if (isNightMode()) return;   // night mode shows the time and nothing else

    int ox   = clockElemOffX[CE_TEMP];
    int tempY = CLOCK_ROW_Y + clockElemOffY[CE_TEMP];
    const int right = SCREEN_W - CLOCK_EDGE_INSET;

    u8g2.setFont(u8g2_font_6x10_tf);

    // drawUTF8 and getUTF8Width, so the multi-byte degree sign renders as one
    // glyph and the right-edge alignment uses on-screen width, not byte count.
    char temp[16];
    if (clockFakeTemp) {
        // A troll owns the reading: shown alone whatever the mode, so the
        // nonsense number reads as one absurd temperature and not a forecast.
        sprintf(temp, "%.0f°F", clockFakeTempNow);
    } else if (tempMode == TEMP_CURRENT) {
        sprintf(temp, "%.0f°F", currentTemp);
    } else {
        float high = (getCurrentTime().hour() <= WEATHER_TOMORROW_AFTER_HOUR)
                   ? todayHighTemp : tomorrowHighTemp;
        sprintf(temp, "%.0f|%.0f°F", currentTemp, high);
    }
    u8g2.drawUTF8(right - u8g2.getUTF8Width(temp) + ox, tempY, temp);

    // Refreshes have been failing: mark the readout stale, so it is clear the
    // numbers may be old rather than simply wrong.
    if (weatherUnavailable)
        u8g2.drawStr(right - u8g2.getStrWidth("X") + ox, tempY - 10, "X");
}

// The big digits. Vertically centred by MEASURING the font rather than
// hardcoding a y, which is what lets a 10px and a 48px font sit in the same
// optical place without each needing its own offset.
void drawClock() {
    String timeStr;
    if (clockFakeTime) {
        char buf[6];
        sprintf(buf, "%02d:%02d", clockFakeHour, clockFakeMinute);
        timeStr = String(buf);
    } else {
        timeStr = getFormattedTime();
    }

    int ox = clockElemOffX[CE_TIME];
    int oy = clockElemOffY[CE_TIME];

    // Swapped: the time gives up the big slot and takes the date's corner, in
    // the date's font. drawDate() does the mirror of this.
    if (clockSwapWithDate) {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(CLOCK_EDGE_INSET + ox, CLOCK_ROW_Y + oy, timeStr.c_str());
        return;
    }

    u8g2.setFont(activeClockFont());
    int y = clockBandTopFor(u8g2.getMaxCharHeight());

    if (showAMPM && twelveHourFormat && !clockShowsSmall()) {
        u8g2.drawStr(ox, y + oy, timeStr.c_str());   // left aligned, leaving room for AM/PM
    } else {
        int w = u8g2.getStrWidth(timeStr.c_str());
        u8g2.drawStr((SCREEN_W - w) / 2 + ox, y + oy, timeStr.c_str());
    }
}

// The date, bottom left -- or in the big centred slot, when swapped.
void drawDate() {
    if (!showDate) return;
    if (isNightMode()) return;

    String dateStr;
    if (clockFakeDate) {
        char buf[8];
        sprintf(buf, "%02d/%02d", clockFakeMonth, clockFakeDay);
        dateStr = String(buf);
    } else {
        dateStr = getFormattedDate();
    }

    int ox = clockElemOffX[CE_DATE];
    int oy = clockElemOffY[CE_DATE];

    if (clockSwapWithDate) {
        u8g2.setFont(activeClockFont());
        int y = clockBandTopFor(u8g2.getMaxCharHeight());
        int w = u8g2.getStrWidth(dateStr.c_str());
        u8g2.drawStr((SCREEN_W - w) / 2 + ox, y + oy, dateStr.c_str());
        return;
    }

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(CLOCK_EDGE_INSET + ox, CLOCK_ROW_Y + oy, dateStr.c_str());
}

// The alarm indicator, bottom centre. Blinks while snoozed, shrinks to a single
// letter in night mode, and is absent when there is no alarm to indicate.
void drawAlarmIndicator() {
    if (!alarmEnabled && !alarmActive && !snoozeActive) return;

    int ox = clockElemOffX[CE_ALARM];
    int oy = clockElemOffY[CE_ALARM];
    u8g2.setFont(u8g2_font_6x10_tf);

    if (isNightMode()) {
        u8g2.drawStr(ox, CLOCK_ROW_Y + oy, "A");
        return;
    }
    if (snoozeActive && (millis() / SNOOZE_BLINK_MS) % 2 != 0) return;   // blink off

    u8g2.drawStr(SCREEN_W / 2 - 15 + ox, CLOCK_ROW_Y + oy, "Alarm");
}

// AM/PM, top right. Only meaningful in 12-hour mode.
void drawAMPM() {
    if (!showAMPM) return;
    if (!twelveHourFormat) return;
    if (isNightMode()) return;

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(CLOCK_AMPM_X, CLOCK_AMPM_Y, isPM ? "PM" : "AM");
}

// ============================================================================
//  TROLL HOOKS
//  Written by trolls, read by the draw functions above. All zero and false is
//  the normal clock.
// ============================================================================

int   clockElemOffX[CE_COUNT] = {0, 0, 0, 0};
int   clockElemOffY[CE_COUNT] = {0, 0, 0, 0};
bool  clockFakeTime     = false;
int   clockFakeHour     = 0;
int   clockFakeMinute   = 0;
bool  clockFakeTemp     = false;
float clockFakeTempNow  = 0;
bool  clockFakeDate     = false;
int   clockFakeMonth    = 1;
int   clockFakeDay      = 1;
bool  clockSmallMode    = false;
bool  clockFontLocked   = false;
bool  clockCenterFull   = false;
bool  clockSwapWithDate = false;

// Put every hook back to normal. Called whenever a troll event ends, which is
// what guarantees no troll can permanently break the clock.
void resetClockOffsets() {
    for (int i = 0; i < CE_COUNT; i++) {
        clockElemOffX[i] = 0;
        clockElemOffY[i] = 0;
    }
    clockFakeTime     = false;
    clockFakeTemp     = false;
    clockFakeDate     = false;
    clockSmallMode    = false;
    clockFontLocked   = false;
    clockCenterFull   = false;
    clockSwapWithDate = false;
}

// Where the big centred text sits vertically, for a font of this height. The
// band is normally everything above the bottom row, or the whole screen when a
// troll has set clockCenterFull.
int clockBandTopFor(int textH) {
    int band = clockCenterFull ? SCREEN_H : CLOCK_ROW_Y;
    int y = (band - textH) / 2;
    return y < 0 ? 0 : y;
}

// ============================================================================
//  BRIGHTNESS
//  Four real panel steps, plus night mode as a fifth pseudo-level that borrows
//  the dimmest of them. Each step is three SSD1309 registers, because contrast
//  alone does not get the panel dark enough to sleep next to.
// ============================================================================

int brightnessLevel = BRIGHTNESS_FULL;

struct BrightnessStep {
    uint8_t contrast;   // 0x81: 0..255
    uint8_t precharge;  // 0xD9: low nibble phase 2, high nibble phase 1
    uint8_t vcomh;      // 0xDB: 0x00 dimmest .. 0x40
};

static const BrightnessStep brightnessSteps[] = {
    {0x00, 0x11, 0x00},  // dimmest -- as dim as the panel will go
    {0x20, 0x22, 0x20},  // low
    {0x80, 0x44, 0x30},  // medium
    {0xFF, 0xF1, 0x40},  // full -- the SSD1309 defaults
};
static const uint8_t kStepCount      = sizeof(brightnessSteps) / sizeof(brightnessSteps[0]);
static const uint8_t kBrightnessCount = kStepCount + 1;   // the levels the cycle walks: 0..NIGHT

bool autoNightMode = true;

// Level to return to when the alarm stops ringing; -1 when no alarm owns the
// panel. Also absorbs night-mode transitions that land mid-ring, so they are
// not lost and do not fight the alarm for the display.
static int restoreLevel = -1;

// Last non-night level, so waking returns to whatever the user actually had set
// rather than a hardcoded step.
static int dayBrightnessLevel = BRIGHTNESS_FULL;

bool isNightMode() { return brightnessLevel == BRIGHTNESS_NIGHT; }

// Push a level's three registers to the panel. Night mode has no entry of its
// own, so it is clamped onto the dimmest step here.
void applyBrightness(uint8_t level) {
    if (level >= kStepCount) level = 0;
    const BrightnessStep &s = brightnessSteps[level];
    u8g2.sendF("ca", 0xD9, s.precharge);
    u8g2.sendF("ca", 0xDB, s.vcomh);
    u8g2.setContrast(s.contrast);
}

// The single owner of brightnessLevel: assigns it, pushes it to the panel and
// repaints. Everything that changes brightness goes through here, so the
// variable and the hardware can never disagree.
void setBrightnessLevel(int level) {
    if (level < 0 || level > BRIGHTNESS_NIGHT) level = BRIGHTNESS_FULL;
    brightnessLevel = level;
    if (level != BRIGHTNESS_NIGHT) dayBrightnessLevel = level;
    applyBrightness(level);
    forceClockRedraw();
}

// Step to the next level, night mode included, and remember it.
void cycleBrightness() {
    setBrightnessLevel((brightnessLevel + 1) % kBrightnessCount);
    saveDisplayPreferences();
}

// ---- night mode ------------------------------------------------------------
// While the alarm is ringing the alarm owns the panel, so a transition arriving
// then is parked in restoreLevel and applied when the ring ends.

void enterNightMode() {
    if (restoreLevel >= 0) { restoreLevel = BRIGHTNESS_NIGHT; return; }
    setBrightnessLevel(BRIGHTNESS_NIGHT);
}

void exitNightMode() {
    if (restoreLevel >= 0) { restoreLevel = dayBrightnessLevel; return; }
    if (isNightMode()) setBrightnessLevel(dayBrightnessLevel);
}

// Display-menu toggle. Switching the feature off while dimmed would otherwise
// strand the screen in night mode until you cycled brightness by hand.
void setAutoNightMode(bool on) {
    autoNightMode = on;
    if (!on) exitNightMode();
}

// ---- alarm override --------------------------------------------------------

// Full brightness for the ring. A ring is a wake-up, so if it caught us in
// night mode we come out of it for good rather than dropping back to a
// stripped-down screen afterwards -- you are up, so you get the full clock.
void alarmBrightnessOverride() {
    if (restoreLevel < 0)   // a re-ring must not stomp the saved level
        restoreLevel = isNightMode() ? dayBrightnessLevel : brightnessLevel;
    setBrightnessLevel(BRIGHTNESS_FULL);
}

void restoreBrightnessAfterAlarm() {
    if (restoreLevel < 0) return;
    int level = restoreLevel;
    restoreLevel = -1;
    setBrightnessLevel(level);
}

// ============================================================================
//  IDLE MODE
// ============================================================================

bool idleMode = false;
static unsigned long lastInputMs  = 0;
static bool          idleWasNight = false;

// Called on any input. Wakes the clock back up if it had dozed off.
void noteUserInput() {
    lastInputMs = millis();
    if (!idleMode) return;

    idleMode = false;
    // Only undo the dimming if idle was what caused it. If it was genuinely
    // night when we dozed off, leave night mode to the morning transition.
    if (!idleWasNight) exitNightMode();
    forceClockRedraw();
}

// Called every frame. Dims the panel once nobody has pressed anything for days;
// updateTrollTimer() sees idleMode and stops trolling.
void updateIdleMode() {
    if (idleMode) return;
    if (millis() - lastInputMs < IDLE_AFTER_MS) return;

    idleMode     = true;
    idleWasNight = isNightMode();
    if (!idleWasNight) enterNightMode();
    DEBUG_PRINTLN("Idle: no input for days, trolls suspended");
}

// ============================================================================
//  CLOCK FONTS
//  One flat table so the encoder can walk every font in order, with a size
//  class per row. The class is what lets a troll ask for "a small clock"
//  without naming a specific font.
//
//  To reclassify a font, move its line and change the tag. To add one, add a
//  line -- nothing else needs touching.
// ============================================================================

enum ClockFontSize { CF_SMALL, CF_MEDIUM, CF_LARGE };

struct ClockFontEntry {
    const uint8_t* font;
    uint8_t        size;   // ClockFontSize
};

static const ClockFontEntry clockFonts[] = {
    // ---- small: compact readouts, used by night mode and the troll scenes ---
    // 6x10 stays FIRST: it is the one night mode and the scenes actually use.
    { u8g2_font_6x10_tf,             CF_SMALL  },
    { u8g2_font_t0_11_tn,            CF_SMALL  },
    { u8g2_font_courR10_tn,          CF_SMALL  },
    { u8g2_font_profont15_tn,        CF_SMALL  },
    { u8g2_font_koleeko_tn,          CF_SMALL  },
    { u8g2_font_tenthinguys_tn,      CF_SMALL  },
    { u8g2_font_tenthinnerguys_tn,   CF_SMALL  },
    { u8g2_font_tenfatguys_tr,       CF_SMALL  },
    { u8g2_font_prospero_nbp_tn,     CF_SMALL  },
    { u8g2_font_fancypixels_tr,      CF_SMALL  },
    { u8g2_font_lucasarts_scumm_subtitle_o_tn, CF_SMALL },
    { u8g2_font_smart_patrol_nbp_tn, CF_SMALL  },
    { u8g2_font_missingplanet_tn,    CF_SMALL  },
    { u8g2_font_cube_mel_tn,         CF_SMALL  },
    { u8g2_font_press_mel_tn,        CF_SMALL  },
    { u8g2_font_mademoiselle_mel_tn, CF_SMALL  },
    { u8g2_font_pieceofcake_mel_tn,  CF_SMALL  },
    { u8g2_font_habsburgchancery_tn, CF_SMALL  },
    { u8g2_font_greenbloodserif2_tr, CF_SMALL  },
    { u8g2_font_tooseornament_tn,    CF_SMALL  },

    // ---- medium ------------------------------------------------------------
    { u8g2_font_cardimon_pixel_tn,   CF_MEDIUM },
    { u8g2_font_timB18_tn,           CF_MEDIUM },
    { u8g2_font_lubBI18_tn,          CF_MEDIUM },
    { u8g2_font_lubR18_tn,           CF_MEDIUM },
    { u8g2_font_luBS18_tn,           CF_MEDIUM },
    { u8g2_font_osr18_tn,            CF_MEDIUM },
    { u8g2_font_crox4hb_tn,          CF_MEDIUM },
    { u8g2_font_t0_22_tn,            CF_MEDIUM },
    { u8g2_font_courR24_tn,          CF_MEDIUM },
    { u8g2_font_spleen12x24_mn,      CF_MEDIUM },
    { u8g2_font_mystery_quest_24_tn, CF_MEDIUM },
    { u8g2_font_freedoomr25_tn,      CF_MEDIUM },
    { u8g2_font_mystery_quest_28_tn, CF_MEDIUM },
    { u8g2_font_profont29_tn,        CF_MEDIUM },
    { u8g2_font_osr29_tn,            CF_MEDIUM },
    { u8g2_font_osb29_tn,            CF_MEDIUM },
    { u8g2_font_bubble_tn,           CF_MEDIUM },

    // ---- large: 30px and up ------------------------------------------------
    { u8g2_font_fur30_tn,            CF_LARGE  },
    { u8g2_font_fub30_tn,            CF_LARGE  },
    { u8g2_font_spleen16x32_mn,      CF_LARGE  },
    { u8g2_font_inr33_mn,            CF_LARGE  },
    { u8g2_font_logisoso34_tn,       CF_LARGE  },
    { u8g2_font_logisoso38_tn,       CF_LARGE  },
    { u8g2_font_t0_40_tn,            CF_LARGE  },
    { u8g2_font_mystery_quest_48_tn, CF_LARGE  },
};
static const int CLOCK_FONT_COUNT = sizeof(clockFonts) / sizeof(clockFonts[0]);

// TWO selections, remembered separately: one for the normal clock and one for
// the compact one. That is what stops a font picked during night mode from
// leaving your daytime clock small once morning comes.
static int fontIndexAll   = 0;
static int fontIndexSmall = 0;   // always points at a small-or-medium row

// Is the compact clock showing? Night mode implies it; trolls set the flag.
bool clockShowsSmall() { return clockSmallMode || isNightMode(); }

// Compact mode accepts small AND medium -- it means "not a huge clock", not
// "tiny".
static bool fitsCompact(uint8_t size) { return size != CF_LARGE; }

const uint8_t* activeClockFont() {
    return clockShowsSmall() ? clockFonts[fontIndexSmall].font
                             : clockFonts[fontIndexAll].font;
}

// Step the font selection for whichever mode is showing. In compact mode this
// walks the table but skips the large entries, so the encoder moves through the
// smaller fonts as though they were a list of their own.
void cycleClockFont(int direction) {
    if (clockFontLocked) return;   // the Tiny clock daily owns the font

    if (clockShowsSmall()) {
        int i = fontIndexSmall;
        for (int step = 0; step < CLOCK_FONT_COUNT; step++) {
            i = ((i + direction) % CLOCK_FONT_COUNT + CLOCK_FONT_COUNT) % CLOCK_FONT_COUNT;
            if (fitsCompact(clockFonts[i].size)) break;
        }
        fontIndexSmall = i;
    } else {
        fontIndexAll = ((fontIndexAll + direction) % CLOCK_FONT_COUNT + CLOCK_FONT_COUNT)
                       % CLOCK_FONT_COUNT;
    }
}

// ============================================================================
//  SAVED PREFERENCES
// ============================================================================

void loadDisplayPreferences() {
    Preferences prefs;
    prefs.begin("display", true);
    twelveHourFormat = prefs.getBool("12hr", true);
    showDate         = prefs.getBool("date", true);
    // "tempmode" replaced the older on/off "temp" bool. If only the old key is
    // present, carry the setting over so an existing clock keeps its choice.
    if (prefs.isKey("tempmode"))   tempMode = prefs.getInt("tempmode", TEMP_HIGH);
    else if (prefs.isKey("temp"))  tempMode = prefs.getBool("temp", true) ? TEMP_HIGH : TEMP_OFF;
    else                           tempMode = TEMP_HIGH;
    if (tempMode < 0 || tempMode >= TEMP_MODE_COUNT) tempMode = TEMP_HIGH;
    showAMPM         = prefs.getBool("ampm", true);
    autoNightMode    = prefs.getBool("autonight", true);
    int saved        = prefs.getInt("bright", brightnessLevel);
    prefs.end();
    setBrightnessLevel(saved);   // clamps an out-of-range value and applies it
}

void saveDisplayPreferences() {
    Preferences prefs;
    prefs.begin("display", false);
    prefs.putBool("12hr", twelveHourFormat);
    prefs.putBool("date", showDate);
    prefs.putInt("tempmode", tempMode);
    prefs.putBool("ampm", showAMPM);
    prefs.putBool("autonight", autoNightMode);
    // Never persist night mode: it is a time of day, not a preference. Saving
    // it would mean a reboot at noon came up dark and stripped down.
    prefs.putInt("bright", isNightMode() ? dayBrightnessLevel : brightnessLevel);
    prefs.end();
}
