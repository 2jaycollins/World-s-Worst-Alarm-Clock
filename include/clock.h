// ============================================================================
//  clock.h -- the main clock screen.
//
//  The face is four elements: the big digits, the date, the temperature and the
//  alarm indicator. The last three share a row across the bottom; the digits
//  are centred in the band above it.
//
//  Everything here is drawn from the current time on demand, and the screen is
//  only repainted when something has actually changed -- see clockNeedsRedraw().
//  Anything that wants an unconditional repaint calls forceClockRedraw().
//
//  Layout coordinates and timings are in values.h.
// ============================================================================

#ifndef CLOCK_H
#define CLOCK_H

#include "display.h"
#include "clock_logic.h"
#include "alarm.h"

// ============================================================================
//  RENDERING
// ============================================================================
void displayClockScreen();   // the normal path: clear, draw, send, if anything changed
void displayAlarmClock();    // the same, plus the whole panel flashing while ringing
void clearAlarmInvert();     // put the panel back to normal after a ring

bool clockNeedsRedraw();
void forceClockRedraw();

// Draw the face into the buffer WITHOUT clearing or sending it, and without the
// dirty-flag check. This is the hook for overlay trolls: clear the buffer, call
// this, draw your gag on top, send. displayClockScreen() is just this with the
// clear, send and dirty flag wrapped around it.
void drawClockFace();

// The individual elements, exposed because the aquarium troll redraws them in
// black on top of its sand.
void drawClock();
void drawDate();
void drawAlarmIndicator();
void drawAMPM();
void displayWeather();

// y of the big centred clock slot, for a font of this height. Shared so the
// date can borrow the exact same slot when the two are swapped.
int clockBandTopFor(int textH);

// ============================================================================
//  ELEMENT VISIBILITY
//  User settings from the Display menu. The alarm indicator is deliberately not
//  included -- it is always shown when there is an alarm to show.
// ============================================================================
extern bool showDate;
extern bool showAMPM;

// The temperature readout has three settings rather than a simple on/off:
//   TEMP_OFF      nothing at all
//   TEMP_CURRENT  just the current reading, e.g. "72°F"
//   TEMP_HIGH     the current reading paired with the day's high, "72|85°F"
// TEMP_HIGH switches to TOMORROW's high in the evening, once today's is
// history -- see WEATHER_TOMORROW_AFTER_HOUR in values.h.
enum TempMode {
    TEMP_OFF = 0,
    TEMP_CURRENT,
    TEMP_HIGH,
    TEMP_MODE_COUNT   // must stay last
};

extern int  tempMode;
bool        tempShown();      // is the readout displayed at all?
const char* tempModeLabel();  // short text for the menu row

void loadDisplayPreferences();
void saveDisplayPreferences();

// ============================================================================
//  BRIGHTNESS AND NIGHT MODE
//  Levels 0..BRIGHTNESS_FULL are real panel steps. BRIGHTNESS_NIGHT is a
//  pseudo-level one past them: it uses the dimmest step AND strips the screen
//  down to just the time. It has no entry in the brightness table, so never
//  index that table with it -- go through setBrightnessLevel(), which clamps.
// ============================================================================
#define BRIGHTNESS_FULL   3
#define BRIGHTNESS_NIGHT  4

extern int brightnessLevel;   // read-only outside clock.cpp; write via setBrightnessLevel()

void setBrightnessLevel(int level);   // assign, push to the panel, and repaint
void applyBrightness(uint8_t level);  // push to the panel only
void cycleBrightness();               // step to the next level and save

bool isNightMode();
void enterNightMode();
void exitNightMode();

extern bool autoNightMode;            // are the timed transitions armed at all?
void setAutoNightMode(bool on);       // Display-menu toggle for them

// Full brightness while the alarm rings, then back to whatever was set before.
void alarmBrightnessOverride();
void restoreBrightnessAfterAlarm();

// ============================================================================
//  IDLE MODE
//  Nobody has touched the clock in days, so assume the room is empty: dim to
//  night mode and suspend every troll until someone presses something. Stops
//  the clock sitting inside a gag for a week while you are away.
// ============================================================================
extern bool idleMode;
void noteUserInput();    // call on ANY button press or encoder movement
void updateIdleMode();   // call every frame

// ============================================================================
//  CLOCK FONTS
//  One flat table in clock.cpp, with each font tagged small, medium or large.
//  The encoder walks the whole table normally, or just the smaller ones while
//  the compact clock is showing. The two selections are remembered separately,
//  so picking a small font at night does not shrink your clock in the morning.
// ============================================================================
void cycleClockFont(int direction);   // 1 for the next font, -1 for the previous
bool clockShowsSmall();               // is the compact clock showing right now?
const uint8_t* activeClockFont();     // the font matching the current mode

// ============================================================================
//  TROLL HOOKS
//  Everything below lets a troll interfere with the clock face without the
//  clock knowing anything about trolls. All of it is cleared by
//  resetClockOffsets() when an event ends, so a troll can never permanently
//  strand the clock in a broken state.
// ============================================================================

// Per-element pixel offsets, so a troll can shove any single piece of the face
// around independently -- slide it off, shake it, let it drift -- without
// touching the others. Zero is the normal position.
enum ClockElement {
    CE_TIME = 0,   // the big digits
    CE_DATE,       // bottom left
    CE_TEMP,       // bottom right
    CE_ALARM,      // bottom centre
    CE_COUNT
};
extern int clockElemOffX[CE_COUNT];
extern int clockElemOffY[CE_COUNT];

void resetClockOffsets();   // clears the offsets AND every flag below

// Show something other than the real time. The values are printed verbatim,
// already 12/24h adjusted, so a troll can scramble them every frame or set one
// wrong time and leave it.
extern bool clockFakeTime;
extern int  clockFakeHour;
extern int  clockFakeMinute;

// The same idea for the other readouts. The fake temperature is shown alone,
// without the usual "now|high" pair, so it reads as one absurd number rather
// than a broken forecast.
extern bool  clockFakeTemp;
extern float clockFakeTempNow;
extern bool  clockFakeDate;
extern int   clockFakeMonth, clockFakeDay;

// ---- layout modes ----------------------------------------------------------
extern bool clockSmallMode;      // compact centred clock, and the encoder is limited to small fonts
extern bool clockFontLocked;     // the encoder does nothing at all -- "a small clock you are stuck with"
extern bool clockCenterFull;     // centre on the WHOLE screen, not just the band above the bottom row
extern bool clockSwapWithDate;   // the date takes the big slot and font; the time shrinks into the corner

#endif
