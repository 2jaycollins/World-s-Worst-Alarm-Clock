#include "menu.h"

static int menuState     = MAIN_MENU;
static int selectedIndex = 0;

bool devMode             = false;
bool exitMenuFlag        = false;
int  requestedTrollLaunch = -1;
bool requestedBluetooth  = false;

// ============================================================================
//  SHARED CHROME
//  Every screen is a title, a "back" hint and a rule, over a scrolling list.
//  Keeping that in three functions is what stops the eight screens below
//  drifting apart by a pixel each.
// ============================================================================

// Up-arrow and "back" pinned to the top right, pointing at the physical BRIGHT
// button above the screen to advertise it as the way out.
static void drawBackIndicator() {
    u8g2.setFont(u8g2_font_5x7_tf);
    int w = u8g2.getStrWidth("back");
    int x = SCREEN_W - 1 - w;
    int cx = x + w / 2;
    u8g2.drawTriangle(cx, 0, cx - 4, 4, cx + 4, 4);
    u8g2.drawStr(x, 5, "back");
}

// Title, back hint and separator. The rule sits low enough that the "back"
// label does not touch it.
static void drawMenuHeader(const char* title) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 2, title);
    drawBackIndicator();
    u8g2.drawHLine(0, MENU_HEADER_Y, SCREEN_W);
}

// The scrolling list itself, shared by every screen that has one. labelFn gives
// row i's text; markFn, if supplied, gives a marker for the right-hand column
// (or nullptr for no marker on that row). The window follows selectedIndex.
static void drawRows(int count, const char* (*labelFn)(int),
                     const char* (*markFn)(int) = nullptr) {
    int first = 0;
    if (selectedIndex >= MENU_VISIBLE) first = selectedIndex - MENU_VISIBLE + 1;

    u8g2.setFont(u8g2_font_6x10_tf);
    for (int row = 0; row < MENU_VISIBLE; row++) {
        int idx = first + row;
        if (idx >= count) break;

        int y = MENU_START_Y + row * MENU_ROW_H;
        if (idx == selectedIndex) {
            u8g2.drawBox(0, y, SCREEN_W, MENU_ROW_H);
            u8g2.setDrawColor(0);   // black text on the highlight bar
        } else {
            u8g2.setDrawColor(1);
        }

        u8g2.drawStr(3, y + 1, labelFn(idx));
        if (markFn) {
            const char* mark = markFn(idx);
            if (mark) u8g2.drawStr(120, y + 1, mark);
        }
        u8g2.setDrawColor(1);
    }
}

// A complete list screen: header, rows, send.
static void drawListScreen(const char* title, int count, const char* (*labelFn)(int),
                           const char* (*markFn)(int) = nullptr) {
    u8g2.clearBuffer();
    drawMenuHeader(title);
    drawRows(count, labelFn, markFn);
    u8g2.sendBuffer();
}

// ============================================================================
//  MAIN MENU
//  selectedIndex maps straight into this table, and clicking jumps menuState to
//  the row's .state. The troll row is only counted in devMode, and lives last
//  so the visible indices line up for everyone else.
// ============================================================================

struct MenuItem {
    const char* label;
    int         state;
};

static const MenuItem mainMenuItems[] = {
    {"Set Alarm", SET_ALARM},
    {"Volume",    SET_VOLUME},
    {"Display",   SET_DISPLAY},
    {"Locations", LOCATION_MENU},
    {"Bluetooth", BLUETOOTH_MODE},
    {"Troll",     TROLL_MENU},   // devMode only, must stay last
};
static const int MAIN_MENU_COUNT = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

static int mainMenuCount() { return devMode ? MAIN_MENU_COUNT : MAIN_MENU_COUNT - 1; }

static const char* mainMenuLabel(int i) { return mainMenuItems[i].label; }

static void drawMainMenu() {
    drawListScreen("Menu", mainMenuCount(), mainMenuLabel);
}

// ============================================================================
//  TROLL MENU LAYOUT
//  Two levels. The top level is a "Settings" row plus one row per category;
//  picking either opens a sublist. With thirty-odd trolls a single flat list
//  had become unusable.
// ============================================================================

#define TROLL_ROW_SETTINGS 0   // opens TROLL_SETTINGS
#define TROLL_ROW_FIRST    1   // first category row

// Rows on the TROLL_SETTINGS screen.
#define TSET_ALL       0   // master on/off for every troll
#define TSET_ALARM_OK  1   // safety switch: the alarm rings even through a major
#define TSET_SOUND     2   // sound on / off / daytime only
#define TSET_NIGHT     3   // may the random roll run while in night mode?
#define TSET_COUNT     4

static const TrollKind TROLL_CATS[] = {
    TROLL_MINOR, TROLL_MAJOR, TROLL_DAILY, TROLL_WEATHER, TROLL_FUND,
};
static const char* const TROLL_CAT_NAMES[] = {
    "Minor", "Major", "Daily", "Weather", "Fundamental",
};
static const int TROLL_CAT_COUNT = sizeof(TROLL_CATS) / sizeof(TROLL_CATS[0]);

static int trollCat = 0;   // which category TROLL_LIST is showing

static int trollMenuRowCount() { return TROLL_CAT_COUNT + TROLL_ROW_FIRST; }

// How many trolls belong to a kind.
static int trollsInKind(TrollKind k) {
    int n = 0;
    for (int i = 0; i < TROLL_EVENT_COUNT; i++) if (trollEvents[i].kind == k) n++;
    return n;
}

// The nth troll of a kind, as an index into trollEvents[], or -1 if out of
// range. The filtered lists only ever hold indices, so nothing else needs to
// know the table is unsorted.
static int trollAtInKind(TrollKind k, int nth) {
    for (int i = 0; i < TROLL_EVENT_COUNT; i++) {
        if (trollEvents[i].kind != k) continue;
        if (nth-- == 0) return i;
    }
    return -1;
}

// ============================================================================
//  SCREENS
// ============================================================================

// ---- editable working copies -----------------------------------------------
// SET_ALARM and SET_VOLUME edit a value rather than scrolling a list, so they
// hold their own state and intercept the encoder.
static bool  setHour = true;                  // true = editing the hour, false = the minute
static int   editAlarmHour   = 0;
static int   editAlarmMinute = 0;
static float menuGain = AUDIO_DEFAULT_GAIN;

// Big 12-hour time with an AM/PM readout, and an underline showing which field
// the encoder is currently driving.
static void drawSetAlarm() {
    u8g2.clearBuffer();
    drawMenuHeader("Set Alarm");

    int dispHour = editAlarmHour % 12;
    if (dispHour == 0) dispHour = 12;
    bool pm = editAlarmHour >= 12;

    char hh[4], mm[4];
    sprintf(hh, "%02d", dispHour);
    sprintf(mm, "%02d", editAlarmMinute);

    u8g2.setFont(u8g2_font_logisoso24_tf);
    int hw = u8g2.getStrWidth(hh);
    int cw = u8g2.getStrWidth(":");
    int mw = u8g2.getStrWidth(mm);
    int total = hw + cw + mw;
    int x = (SCREEN_W - total) / 2 - 8;   // nudged left to leave room for AM/PM
    int y = 22;

    u8g2.drawStr(x, y, hh);
    u8g2.drawStr(x + hw, y, ":");
    u8g2.drawStr(x + hw + cw, y, mm);

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(x + total + 6, y + 14, pm ? "PM" : "AM");

    int underY = y + 28;
    if (setHour) u8g2.drawHLine(x, underY, hw);
    else         u8g2.drawHLine(x + hw + cw, underY, mw);

    u8g2.sendBuffer();
}

// A bar and a percentage. The gain is applied live as you turn, so there is
// nothing to confirm.
static void drawSetVolume() {
    u8g2.clearBuffer();
    drawMenuHeader("Volume");

    const int barX = 8, barW = 112, barY = 28, barH = 16;
    u8g2.drawFrame(barX, barY, barW, barH);
    int fill = (int)((menuGain / AUDIO_MAX_GAIN) * (barW - 2));
    if (fill > 0) u8g2.drawBox(barX + 1, barY + 1, fill, barH - 2);

    char pct[8];
    sprintf(pct, "%d%%", (int)((menuGain / AUDIO_MAX_GAIN) * 100));
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(pct)) / 2, 50, pct);

    u8g2.sendBuffer();
}

// ---- display settings ------------------------------------------------------
#define DISPLAY_MENU_COUNT 6

// Rows 0 and 1 are settings, 2 to 4 toggle individual clock elements, and row 5
// arms the timed night-mode transitions.
static const char* displayLabel(int i) {
    static char row[20];
    switch (i) {
        case 0: snprintf(row, sizeof(row), "Time:   %s", twelveHourFormat ? "12hr" : "24hr"); break;
        case 1: if (isNightMode()) snprintf(row, sizeof(row), "Bright: Night");
                else               snprintf(row, sizeof(row), "Bright: %d/4", brightnessLevel + 1);
                break;
        case 2: snprintf(row, sizeof(row), "Date:   %s", showDate ? "On" : "Off"); break;
        case 3: snprintf(row, sizeof(row), "Temp:   %s", showTemp ? "On" : "Off"); break;
        case 4: snprintf(row, sizeof(row), "AM/PM:  %s", showAMPM ? "On" : "Off"); break;
        default: snprintf(row, sizeof(row), "Night:  %s", autoNightMode ? "Auto" : "Off"); break;
    }
    return row;
}

static void drawSetDisplay() {
    drawListScreen("Display", DISPLAY_MENU_COUNT, displayLabel);
}

// ---- locations -------------------------------------------------------------
static const char* locationLabel(int i) { return LOCATIONS[i].name; }

static void drawLocationMenu() {
    drawListScreen("Locations", LOCATION_COUNT, locationLabel);
}

// ---- troll screens ---------------------------------------------------------

// The running-event banner across the top of both troll screens: SNOOZE either
// plays the highlighted troll or cancels the one already running.
static void drawTrollBanner() {
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* label = (activeEventIndex != -1) ? "cancel" : "play";
    int w = u8g2.getStrWidth(label);
    u8g2.drawTriangle(63, 0, 59, 4, 67, 4);
    u8g2.drawStr(63 - w / 2, 5, label);
}

// A category row shows the name and how many of that kind are switched on.
static const char* trollCatLabel(int i) {
    static char row[26];
    if (i == TROLL_ROW_SETTINGS) return "Settings";

    int c = i - TROLL_ROW_FIRST;
    TrollKind k = TROLL_CATS[c];
    int on = 0;
    for (int t = 0; t < TROLL_EVENT_COUNT; t++)
        if (trollEvents[t].kind == k && trollEvents[t].enabled) on++;

    snprintf(row, sizeof(row), "%-11s %d/%d", TROLL_CAT_NAMES[c], on, trollsInKind(k));
    return row;
}

static const char* trollCatMark(int i) { return i == TROLL_ROW_SETTINGS ? ">" : nullptr; }

static void drawTrollMenu() {
    u8g2.clearBuffer();
    drawMenuHeader("Trolls");
    drawRows(trollMenuRowCount(), trollCatLabel, trollCatMark);
    drawTrollBanner();
    u8g2.sendBuffer();
}

// The four global switches, on their own screen so the top level stays a clean
// list of categories.
static const char* trollSettingLabel(int row) {
    static char out[26];
    switch (row) {
        case TSET_ALL:      snprintf(out, sizeof(out), "All: %s", allTrollsEnabled() ? "on" : "off"); break;
        case TSET_ALARM_OK: snprintf(out, sizeof(out), "Alarm OK: %s", trollAlarmAlwaysRings ? "on" : "off"); break;
        case TSET_SOUND:    snprintf(out, sizeof(out), "Sound: %s", soundModeLabel()); break;
        default:            snprintf(out, sizeof(out), "T@night: %s", trollsAtNight ? "Yes" : "No"); break;
    }
    return out;
}

static void drawTrollSettings() {
    drawListScreen("Troll setup", TSET_COUNT, trollSettingLabel);
}

// Second level: the trolls of one category, with their checkboxes. The marker
// column shows what is running, and what cannot be launched at all.
static const char* trollListLabel(int i) {
    static char row[26];
    int t = trollAtInKind(TROLL_CATS[trollCat], i);
    if (t < 0) return "";
    snprintf(row, sizeof(row), "%-4s%s", trollEvents[t].enabled ? "[x]" : "[ ]", trollEvents[t].label);
    return row;
}

static const char* trollListMark(int i) {
    int t = trollAtInKind(TROLL_CATS[trollCat], i);
    if (t < 0) return nullptr;
    if (t == activeEventIndex) return ">";
    if (trollEvents[t].kind == TROLL_FUND) return ".";
    return nullptr;
}

static void drawTrollList() {
    u8g2.clearBuffer();
    drawMenuHeader(TROLL_CAT_NAMES[trollCat]);
    drawRows(trollsInKind(TROLL_CATS[trollCat]), trollListLabel, trollListMark);
    drawTrollBanner();
    u8g2.sendBuffer();
}

// ============================================================================
//  LIFECYCLE
// ============================================================================

void openMenu() {
    menuState     = MAIN_MENU;
    selectedIndex = 0;
    setHour       = true;
    exitMenuFlag  = false;
}

// Secret-code entry point: open straight into the troll list, in devMode.
void openDevMenu() {
    devMode       = true;
    menuState     = TROLL_MENU;
    selectedIndex = 0;
    setHour       = true;
    exitMenuFlag  = false;
}

// Flag the exit; the app loop consumes it and owns the state change.
void exitMenu() {
    devMode       = false;
    selectedIndex = 0;
    menuState     = MAIN_MENU;
    exitMenuFlag  = true;
}

// Draw whichever screen is current.
void drawMenu() {
    switch (menuState) {
        case MAIN_MENU:      drawMainMenu();     break;
        case SET_ALARM:      drawSetAlarm();     break;
        case SET_VOLUME:     drawSetVolume();    break;
        case SET_DISPLAY:    drawSetDisplay();   break;
        case LOCATION_MENU:  drawLocationMenu(); break;
        case TROLL_MENU:     drawTrollMenu();    break;
        case TROLL_LIST:     drawTrollList();    break;
        case TROLL_SETTINGS: drawTrollSettings(); break;
    }
}

// ============================================================================
//  INPUT
// ============================================================================

// Encoder press: select a row, toggle a setting, or commit an edit.
void handleMenuClick() {
    switch (menuState) {
        case MAIN_MENU: {
            int chosen = mainMenuItems[selectedIndex].state;
            if (chosen == BLUETOOTH_MODE) {
                // Not a screen: flag it and let the app loop switch modes.
                requestedBluetooth = true;
                break;
            }
            menuState = chosen;
            selectedIndex = 0;
            if (menuState == SET_ALARM) {
                editAlarmHour   = alarmHour;    // seed the editable copy
                editAlarmMinute = alarmMinute;
                setHour = true;
            }
            break;
        }

        case SET_ALARM:
            // Commit on every press, then swap fields: pressing on the hour
            // saves and moves to the minute, and vice versa.
            alarmHour   = editAlarmHour;
            alarmMinute = editAlarmMinute;
            saveAlarm();
            setHour = !setHour;
            break;

        case SET_VOLUME:
            break;   // the bar is already live as you turn; nothing to confirm

        case SET_DISPLAY:
            switch (selectedIndex) {
                case 0: twelveHourFormat = !twelveHourFormat; break;
                case 1: cycleBrightness();                    break;   // saves itself
                case 2: showDate = !showDate;                 break;
                case 3: showTemp = !showTemp;                 break;
                case 4: showAMPM = !showAMPM;                 break;
                case 5: setAutoNightMode(!autoNightMode);     break;
            }
            saveDisplayPreferences();
            break;

        case LOCATION_MENU:
            onLocationChanged(selectedIndex);
            break;

        case TROLL_MENU:
            if (selectedIndex == TROLL_ROW_SETTINGS) {
                menuState = TROLL_SETTINGS;
            } else {
                trollCat  = selectedIndex - TROLL_ROW_FIRST;
                menuState = TROLL_LIST;
            }
            selectedIndex = 0;
            break;

        case TROLL_SETTINGS:
            switch (selectedIndex) {
                case TSET_ALL:      setAllTrolls(!allTrollsEnabled()); break;   // saves itself
                case TSET_SOUND:    cycleSoundMode();                  break;   // saves itself
                case TSET_ALARM_OK: trollAlarmAlwaysRings = !trollAlarmAlwaysRings;
                                    saveTrollSettings();               break;
                case TSET_NIGHT:    trollsAtNight = !trollsAtNight;
                                    saveTrollSettings();               break;
            }
            break;

        case TROLL_LIST: {
            int t = trollAtInKind(TROLL_CATS[trollCat], selectedIndex);
            if (t >= 0) {
                trollEvents[t].enabled = !trollEvents[t].enabled;
                saveTrollSettings();
            }
            break;
        }
    }
}

// BRIGHT is "back": drop one level, or leave the menu from the top. Leaving
// SET_ALARM also commits and arms the alarm, so setting a time is enough.
void handleMenuBright() {
    switch (menuState) {
        case MAIN_MENU:
        case TROLL_MENU:
            exitMenu();
            break;

        case TROLL_LIST:
            menuState     = TROLL_MENU;
            selectedIndex = TROLL_ROW_FIRST + trollCat;   // land back on the category
            break;

        case TROLL_SETTINGS:
            menuState     = TROLL_MENU;
            selectedIndex = TROLL_ROW_SETTINGS;
            break;

        case SET_ALARM:
            alarmHour   = editAlarmHour;
            alarmMinute = editAlarmMinute;
            enableAlarm();
            playSound(SND_BEEP, GAIN_ALARM_TOGGLE);
            menuState     = MAIN_MENU;
            selectedIndex = 0;
            setHour       = true;
            break;

        default:
            menuState     = MAIN_MENU;
            selectedIndex = 0;
            setHour       = true;
            break;
    }
}

// SNOOZE on the troll screens: cancel the running event, or launch the
// highlighted one. Cancelling works from either level, since a troll that has
// frozen the buttons is the urgent case -- you secret-code in here and stop it.
void handleMenuSnooze() {
    if (menuState != TROLL_MENU && menuState != TROLL_LIST) return;

    if (activeEventIndex != -1) {
        stopTrollEvent();
        return;
    }

    // Launching is only meaningful in the per-category list. We record the
    // request; the app loop does the actual launch, since it means leaving the
    // menu.
    if (menuState != TROLL_LIST) return;
    int t = trollAtInKind(TROLL_CATS[trollCat], selectedIndex);
    if (t < 0) return;
    if (!isEventKind(trollEvents[t].kind)) return;   // a fundamental cannot be launched
    requestedTrollLaunch = t;
}

// Reserved: the alarm button does nothing inside the menu.
void handleMenuAlarm() {
}

// ---- encoder rotation ------------------------------------------------------

// How many rows the current screen has. SET_ALARM and SET_VOLUME return zero
// because they drive a value rather than a cursor.
int getMenuOptionCount() {
    switch (menuState) {
        case MAIN_MENU:      return mainMenuCount();
        case LOCATION_MENU:  return LOCATION_COUNT;
        case SET_DISPLAY:    return DISPLAY_MENU_COUNT;
        case TROLL_MENU:     return trollMenuRowCount();
        case TROLL_SETTINGS: return TSET_COUNT;
        case TROLL_LIST:     return trollsInKind(TROLL_CATS[trollCat]);
        default:             return 0;
    }
}

// Move the cursor down, or adjust whichever value the current screen edits.
void menuScrollDown() {
    if (menuState == SET_ALARM) {
        if (setHour) editAlarmHour   = (editAlarmHour + 1) % 24;
        else         editAlarmMinute = (editAlarmMinute + 1) % 60;
        return;
    }
    if (menuState == SET_VOLUME) {
        menuGain += MENU_VOLUME_STEP;
        if (menuGain > AUDIO_MAX_GAIN) menuGain = AUDIO_MAX_GAIN;
        setAudioGain(menuGain);
        return;
    }
    int count = getMenuOptionCount();
    if (count > 0) selectedIndex = (selectedIndex + 1) % count;
}

// The mirror of menuScrollDown().
void menuScrollUp() {
    if (menuState == SET_ALARM) {
        if (setHour) editAlarmHour   = (editAlarmHour + 23) % 24;
        else         editAlarmMinute = (editAlarmMinute + 59) % 60;
        return;
    }
    if (menuState == SET_VOLUME) {
        menuGain -= MENU_VOLUME_STEP;
        if (menuGain < 0.0f) menuGain = 0.0f;
        setAudioGain(menuGain);
        return;
    }
    int count = getMenuOptionCount();
    if (count > 0) selectedIndex = (selectedIndex - 1 + count) % count;
}
