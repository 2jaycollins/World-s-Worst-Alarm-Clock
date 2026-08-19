// ============================================================================
//  troll_events.h -- the part that makes it the worst alarm clock.
//
//  A "troll" is one gag: a fish swims past, the time slides off the screen, the
//  alarm goes off nine minutes late. They are all rows in one table
//  (trollEvents[] in troll_events.cpp), all individually switchable from the
//  hidden dev menu, and one runs at a time.
//
//  Nothing outside this file knows what any individual troll does. Trolls reach
//  into the clock through the hooks in clock.h -- fake times, element offsets,
//  layout flags -- all of which are reset automatically when an event ends, so
//  a troll can never permanently break the clock.
//
//  Odds, durations and each troll's individual dials are all in values.h.
// ============================================================================

#ifndef TROLL_EVENTS_H
#define TROLL_EVENTS_H

#include "display.h"
#include "clock_logic.h"
#include "clock.h"

#include <Preferences.h>

// ============================================================================
//  ADDING A TROLL
//
//    1. Add an ID to the TrollId enum below.
//    2. Add a matching row to trollEvents[] in troll_events.cpp, IN THE SAME
//       ORDER. A static_assert complains at compile time if the two disagree.
//    3. Write its trigger() and draw() in the implementations section of
//       troll_events.cpp, and point the row at them.
//
//  Everything else -- the menu row, saving its on/off state, the random roll,
//  instant-launch, the input gates -- follows automatically.
//
//  Add new trolls at the END of the enum. Saved on/off states are keyed by
//  index, so inserting in the middle shuffles everyone's saved settings.
// ============================================================================

enum TrollId {
    // ---- events: these take over, or draw over, the screen -----------------
    TR_FAKE_UPDATE = 0, // an hour-long firmware update that never finishes
    TR_ERROR_POPUP,     // a countdown to a reset that never comes
    TR_WRONG_TIME,      // the clock shows a random time for two hours
    TR_FISH_SWIM,       // a school of fish crosses the clock
    TR_FONT_CYCLE,      // the clock font changes several times a second
    TR_SLIDE_OFF,       // the time laps the screen until you catch it
    TR_SNOOZE_WHEEL,    // (fundamental) a slot machine picks your snooze length
    TR_CLOCK_DRIFT,     // (fundamental) the clock is quietly minutes wrong
    TR_ALARM_VOLUME,    // (fundamental) each ring is inaudible or deafening
    TR_EYE_STARE,       // a giant eye watches you
    TR_EYE_STARE_PLUS,  // the same eye, but you cannot dismiss it
    TR_DIGIT_SCROLL,    // the digits race too fast to read
    TR_CLOCK_FLEES,     // the time hides until you ask for it back
    TR_ALL_ADRIFT,      // every element wanders at its own speed
    TR_LOSE_DATE,       // the date slides away and stays gone
    TR_LOSE_TEMP,       // as does the temperature
    TR_LOSE_ALARM,      // and this one disarms the alarm for real
    TR_FISH_SWARM,      // a shoal hides the clock, carries it off, then patrols
    TR_DVD_MODE,        // a small clock bounces around like a DVD logo

    // ---- dailies: rolled each morning, run all day, always in the background
    TR_AQUARIUM,        // a fish tank in front of the still-working clock
    TR_TEMP_INSANE,     // the temperature reads somewhere in -1000..1000F
    TR_DATE_WRONG,      // the date is stuck on a memorable one
    TR_TINY_CLOCK,      // the clock shrinks, and font cycling is disabled
    TR_UPSIDE_DOWN,     // the whole display is rotated 180 degrees

    // ---- more fundamentals, all hooking the alarm --------------------------
    TR_ALARM_SOUND,     // the alarm plays a random clip from data/
    TR_ALARM_DRIFT,     // the alarm goes off up to ten minutes early or late
    TR_SNOOZE_GAMES,    // you have to win a minigame to earn a snooze

    // ---- weather scenes, in ascending severity -----------------------------
    TR_WX_DRIZZLE,
    TR_WX_RAIN,
    TR_WX_SHOWERS,
    TR_WX_SNOW,
    TR_WX_STORM,

    // ---- the rest ----------------------------------------------------------
    TR_SELF_DESTRUCT,   // a countdown, a detonation, then a long dead screen
    TR_CAT_BOUNCE,      // a cat ricochets around, and you cannot stop it
    TR_TURTLE_WALK,     // a turtle plods across the bottom, taking its time
    TR_EMAIL_ICON,      // one emoji, dead centre, nothing works
    TR_CAT_CLOCK,       // the cat replaces the time, and meows
    TR_FROG_SIT,        // a frog squats over the date and temperature
    TR_DATE_SWAP,       // the date and time trade places and fonts
    TR_WALKER,          // a figure paces across the screen, footsteps and all
    TR_WALK_BY,         // the same figure, crossing the working clock in silence

    TR_COUNT            // must stay last
};

// ============================================================================
//  THE FIVE KINDS
//  `kind` decides how a troll is scheduled, which is read BEFORE it runs -- so
//  a troll cannot choose its own severity after the fact. That is why the two
//  eye trolls are separate rows rather than one row that decides at runtime.
// ============================================================================
enum TrollKind {
    // Harmless one-shot event: a fish swims past. The buttons and the alarm
    // keep working, and any press dismisses it.
    TROLL_MINOR,

    // Disruptive one-shot event: the fake update. Its trigger() raises input
    // gates to freeze the buttons, and sometimes the alarm with them.
    TROLL_MAJOR,

    // Fundamental. Never fired as an event and never drawn by this file -- it
    // is a permanent change in behaviour that some other part of the clock opts
    // into by asking trollEnabled(). The snooze wheel is the worked example.
    TROLL_FUND,

    // Rolled ONCE a day, in the morning, and running until bedtime. These are
    // all-day moods rather than events: the clock keeps working, the menu
    // opens, the alarm rings -- the world is just slightly wrong until tomorrow.
    TROLL_DAILY,

    // Weather scenes. Deliberately not part of any roll: these mirror the real
    // forecast, or you launch one by hand. They exist as rows so each condition
    // is individually switchable and individually testable.
    TROLL_WEATHER,
};

// ============================================================================
//  THE TROLL RECORD
//  The last four members are optional -- omit them for nullptr/false, which is
//  the ordinary behaviour. Only fill them in when a troll needs something
//  unusual.
// ============================================================================
struct TrollEvent {
    const char* label;      // shown in the menu; keep it under about 14 characters
    bool        enabled;    // default state, overridden by saved settings at boot
    TrollKind   kind;
    bool        overClock;  // true: the real clock face is drawn under your animation
    uint32_t    durationMs; // auto-ends after this long; 0 runs until cancelled

    void (*trigger)();      // once on activation -- raise input gates here
    void (*draw)();         // every frame while active; nullptr gives a name card
    void (*end)();          // when the event stops: clean up, or COMMIT a lasting
                            // consequence. Also runs on a dev-menu cancel, so a
                            // troll that happened still costs you.
    void (*onInput)();      // a button was pressed. nullptr means the event ends,
                            // which is what almost everything wants. Supply one to
                            // react instead -- to animate back before exiting, say.
    bool (*canFire)();      // is this troll eligible right now? nullptr means always.
                            // The roll skips candidates that say no, so a troll with
                            // nothing to do does not waste the roll.

    // BACKGROUND trolls never take the screen. The clock screen stays up, so
    // the menu, the buttons and the alarm all keep working for the whole
    // duration -- you can go and set an alarm while one is still running. The
    // effect has to work through the clock hooks instead, because none of your
    // drawing replaces the clock.
    //
    // For these, draw() is called from the CLOCK's render path and paints ON TOP
    // of the finished face. So a background troll can be entirely invisible and
    // just feed the hooks (Wrong time), or draw a whole scene over the working
    // clock (Aquarium).
    bool background;
};

extern TrollEvent trollEvents[];
extern const int  TROLL_EVENT_COUNT;
extern int        activeEventIndex;   // the running event, or -1 for none

// ============================================================================
//  INPUT DURING AN EVENT
//  By default ANY button press cancels the running event and returns to the
//  clock. Encoder rotation is ignored -- it is too easy to nudge by accident.
//
//  To make a troll you cannot dismiss, call blockAllTrollInput() in its
//  trigger(); to block one button, set that gate directly. The secret dev-menu
//  code ALWAYS bypasses every gate, so a frozen troll can still be cancelled.
// ============================================================================
struct TrollGates {
    bool blockEncoder;
    bool blockAlarmBtn;
    bool blockSnoozeBtn;
    bool blockBrightBtn;
    bool blockAlarmFire;   // suppress the alarm ringing during the event
};

extern TrollGates trollGates;

void clearTrollGates();
void blockAllTrollInput();     // freeze all four buttons -- NOT the alarm, see blockAlarmFire
bool allTrollInputBlocked();
bool trollSuppressesAlarm();   // should the alarm be swallowed right now?

// ============================================================================
//  GLOBAL SWITCHES
//  The four rows on the troll settings screen.
// ============================================================================
extern bool trollAlarmAlwaysRings;   // safety: the alarm rings even mid-troll. On by default.
extern bool trollsAtNight;           // may the random roll run while the screen is dimmed?

bool allTrollsEnabled();
void setAllTrolls(bool on);
bool trollEnabled(TrollId id);

void setupTrollEvents();
void saveTrollSettings();
void loadTrollSettings();

// ============================================================================
//  SCHEDULING AND LIFECYCLE
// ============================================================================
void updateTrollTimer();        // per frame: owns the minute roll, the daily roll
                                // and keeping the weather scene in step
void rollForTrollEvent();
void updateActiveTrollEvent();  // per frame: ends the running event when its time is up

// Start a specific event. `manual` marks it as launched by hand from the dev
// menu rather than rolled, which lets a troll relax its real-world conditions
// so you can see what it does without waiting until 2am.
void triggerEventByIndex(int index, bool manual = false);
extern bool trollManualLaunch;

void stopTrollEvent();   // stop it, but leave the current screen alone
void exitTrollEvent();   // stop it AND return to the clock

bool isEventKind(TrollKind k);   // true for anything except a fundamental

// Maps an open-meteo weather code to the matching weather scene, or -1 for
// clear skies.
int weatherTrollForCode(int code);

// ============================================================================
//  RENDERING
// ============================================================================
void drawTrollEvent(int index);   // the STATE_TROLL_EVENT screen
void drawTrollOverlay();          // called after the clock face is drawn, so a
                                  // background troll can paint over it

void handleTrollEventClick();
void handleTrollEventSnooze();
void handleTrollEventAlarm();
void handleTrollEventBright();

// ---- helpers for writing a draw() ------------------------------------------
unsigned long trollElapsedMs();   // ms since the running event started
float         trollProgress();    // 0..1 across its duration (1.0 if endless)

// ============================================================================
//  THE ALARM FUNDAMENTALS
//  These are called from alarm.cpp, which does not know or care whether the
//  trolls behind them are switched on -- each returns the ordinary answer when
//  its troll is off.
// ============================================================================

// How long a snooze actually lasts. Returns snoozeDuration normally; with the
// snooze wheel on, spins a slot machine and returns whatever it lands on.
int trollRollSnoozeMinutes();

// May this snooze happen at all? False means you lost the minigame, and the
// caller must disarm the alarm entirely. Returns true when the troll is off.
bool trollSnoozeChallenge();

// Roll this ring's clip and volume. Called from triggerAlarm() so the choice is
// made ONCE per ring -- playAlarmSound() re-queues in a loop, and rolling there
// would change the sound every few seconds.
void        trollRollAlarmRing();
float       trollAlarmGain();   // gain for this ring, or -1 for the normal volume
const char* trollAlarmClip();   // clip for this ring, or nullptr for the normal one

// How many minutes early or late the alarm goes off. Re-rolled whenever the
// alarm is armed, so the offset cannot be learned. Zero when the troll is off.
void trollRollAlarmDrift();
int  trollAlarmDrift();

#endif
