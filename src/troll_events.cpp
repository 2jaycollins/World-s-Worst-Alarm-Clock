#include "troll_events.h"
#include "application_code.h"   // currentState + STATE_* for the state transitions
#include "animations.h"         // frame data for the sprite-based trolls

// Implementations all live at the bottom, in TROLL IMPLEMENTATIONS. They are
// declared here so the table below can point at them.
static void triggerFakeUpdate();    static void drawFakeUpdate();
static void triggerErrorPopup();    static void drawErrorPopup();
static void triggerWrongTime();     static void updateWrongTime();
static void triggerFishSwim();      static void drawFishSwim();
static void triggerFontCycle();     static void drawFontCycle();
static void triggerSlideOff();      static void drawSlideOff();      static void inputSlideOff();
static void triggerEyeStare();      static void drawEyeStare();
static void triggerDigitScroll();   static void drawDigitScroll();
static void triggerClockFlees();    static void drawClockFlees();    static void inputClockFlees();
static void triggerAllAdrift();     static void drawAllAdrift();
static void triggerFishSwarm();     static void drawFishSwarm();
static void endFishAmbience();
static void                         drawDvdMode();
static void triggerAquarium();      static void drawAquarium();
static void triggerTempInsane();    static void updateTempInsane();
static void triggerDateWrong();
static void triggerTinyClock();
static void triggerUpsideDown();                                     static void endUpsideDown();
static void triggerWeatherScene();  static void drawWeatherScene();
static void triggerSelfDestruct();  static void drawSelfDestruct();
static void triggerCatBounce();     static void drawCatBounce();
static void triggerTurtleWalk();    static void drawTurtleWalk();
static void triggerEmailIcon();     static void drawEmailIcon();
static void triggerCatClock();      static void drawCatClock();
static void triggerFrogSit();       static void drawFrogSit();
static void triggerDateSwap();      static void drawDateSwap();
static void triggerWalker();        static void drawWalker();

static void drawLoseDate();   static void endLoseDate();   static bool canLoseDate();
static void drawLoseTemp();   static void endLoseTemp();   static bool canLoseTemp();
static void drawLoseAlarm();  static void endLoseAlarm();  static bool canLoseAlarm();

// ============================================================================
//  THE TROLL TABLE -- this is the part you edit.
//  Rows MUST stay in the same order as the TrollId enum in troll_events.h.
//
//    overClock   true  the real clock face is drawn under your animation
//                false your draw() owns the whole blank screen
//    durationMs  auto-ends after this long; 0 runs until cancelled. Several
//                trolls overwrite their own row here in trigger(), to roll a
//                random length or to fit the length of an animation.
//    draw        nullptr gives a placeholder card with the troll's name, so an
//                unwritten troll is still launchable and testable.
//
//  The trailing end / onInput / canFire / background members are optional; see
//  the struct in the header for what each does.
//
//         label            enabled  kind          overClock  durationMs               trigger              draw
// ============================================================================
TrollEvent trollEvents[] = {
    { "Fake update",   false, TROLL_MAJOR,   false, TR_FAKE_UPDATE_MS,      triggerFakeUpdate,  drawFakeUpdate },
    { "Error popup",   false, TROLL_MAJOR,   true,  0,                      triggerErrorPopup,  drawErrorPopup },
    { "Wrong time",    false, TROLL_MAJOR,   false, TR_WRONG_TIME_MS,       triggerWrongTime,   updateWrongTime,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Fish swim",     false, TROLL_MINOR,   true,  0,                      triggerFishSwim,    drawFishSwim,
                                                                            endFishAmbience },
    { "Font cycle",    false, TROLL_MINOR,   true,  TROLL_PATIENT_MINOR_MS, triggerFontCycle,   drawFontCycle },
    { "Slide off",     false, TROLL_MINOR,   true,  TROLL_BLIND_MINOR_MS,   triggerSlideOff,    drawSlideOff,
                                                                            nullptr, inputSlideOff },
    { "Snooze wheel",  false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },
    { "Clock drift",   false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },
    { "Alarm volume",  false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },
    { "Eye stare",     false, TROLL_MINOR,   false, 0,                      triggerEyeStare,    drawEyeStare },
    { "Eye stare+",    false, TROLL_MAJOR,   false, 0,                      triggerEyeStare,    drawEyeStare },
    { "Digit scroll",  false, TROLL_MINOR,   true,  TROLL_BLIND_MINOR_MS,   triggerDigitScroll, drawDigitScroll },
    { "Clock flees",   false, TROLL_MINOR,   true,  TROLL_BLIND_MINOR_MS,   triggerClockFlees,  drawClockFlees,
                                                                            nullptr, inputClockFlees },
    { "All adrift",    false, TROLL_MINOR,   true,  TROLL_PATIENT_MINOR_MS, triggerAllAdrift,   drawAllAdrift },

    // These three slide an element away and then LEAVE it gone: the animation
    // is all draw() does, and the consequence is committed in end().
    { "Lose date",     false, TROLL_MAJOR,   true,  TR_LOSE_SLIDE_MS,       nullptr,            drawLoseDate,
                                                                            endLoseDate,  nullptr, canLoseDate },
    { "Lose temp",     false, TROLL_MAJOR,   true,  TR_LOSE_SLIDE_MS,       nullptr,            drawLoseTemp,
                                                                            endLoseTemp,  nullptr, canLoseTemp },
    { "Lose alarm",    false, TROLL_MAJOR,   true,  TR_LOSE_SLIDE_MS,       nullptr,            drawLoseAlarm,
                                                                            endLoseAlarm, nullptr, canLoseAlarm },

    { "Fish swarm",    false, TROLL_MINOR,   true,  TROLL_BLIND_MINOR_MS,   triggerFishSwarm,   drawFishSwarm,
                                                                            endFishAmbience },
    { "DVD mode",      false, TROLL_MINOR,   false, TROLL_PATIENT_MINOR_MS, nullptr,            drawDvdMode },

    // ---- dailies: one roll each morning, all day, all in the background ----
    { "Aquarium",      false, TROLL_DAILY,   false, TR_AQUARIUM_MS,         triggerAquarium,    drawAquarium,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    // A minor rather than a daily, but background all the same, so the clock,
    // menu and alarm keep working while it lies about the weather.
    { "Mad temp",      false, TROLL_MINOR,   false, TR_MAD_TEMP_MS,         triggerTempInsane,  updateTempInsane,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Wrong date",    false, TROLL_DAILY,   false, TR_DAILY_MS,            triggerDateWrong,   nullptr,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Tiny clock",    false, TROLL_DAILY,   false, TR_DAILY_MS,            triggerTinyClock,   nullptr,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Upside down",   false, TROLL_DAILY,   false, TR_DAILY_MS,            triggerUpsideDown,  nullptr,
                                                                            endUpsideDown, nullptr, nullptr, /*background=*/true },

    // ---- fundamentals: toggle-only, hooked into the alarm path -------------
    { "Alarm sound",   false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },
    { "Alarm drift",   false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },
    { "Snooze games",  false, TROLL_FUND,    false, 0,                      nullptr,            nullptr },

    // ---- weather scenes ----------------------------------------------------
    // Never rolled: driven by the real forecast, or launched by hand. All five
    // share one draw(), and the row's index picks its style. Background,
    // because they can be up for hours and everything must keep working
    // underneath. Duration 0: they last as long as the weather does.
    { "Wx drizzle",    false, TROLL_WEATHER, false, 0,                      triggerWeatherScene, drawWeatherScene,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Wx rain",       false, TROLL_WEATHER, false, 0,                      triggerWeatherScene, drawWeatherScene,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Wx showers",    false, TROLL_WEATHER, false, 0,                      triggerWeatherScene, drawWeatherScene,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Wx snow",       false, TROLL_WEATHER, false, 0,                      triggerWeatherScene, drawWeatherScene,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },
    { "Wx storm",      false, TROLL_WEATHER, false, 0,                      triggerWeatherScene, drawWeatherScene,
                                                                            nullptr, nullptr, nullptr, /*background=*/true },

    { "Self destruct", false, TROLL_MAJOR,   true,  0,                      triggerSelfDestruct, drawSelfDestruct },
    { "Cat bounce",    false, TROLL_MAJOR,   false, 0,                      triggerCatBounce,    drawCatBounce },
    { "Turtle walk",   false, TROLL_MAJOR,   false, 0,                      triggerTurtleWalk,   drawTurtleWalk },
    { "Cool emoji",    false, TROLL_MAJOR,   false, 0,                      triggerEmailIcon,    drawEmailIcon },
    { "Cat clock",     false, TROLL_MINOR,   true,  TR_CAT_CLOCK_MS,        triggerCatClock,     drawCatClock },
    { "Frog",          false, TROLL_MINOR,   true,  TROLL_PATIENT_MINOR_MS, triggerFrogSit,      drawFrogSit },
    { "Date swap",     false, TROLL_MINOR,   true,  TROLL_PATIENT_MINOR_MS, triggerDateSwap,     drawDateSwap },
    { "Walker",        false, TROLL_MINOR,   false, TROLL_BLIND_MINOR_MS,   triggerWalker,       drawWalker },
};
const int TROLL_EVENT_COUNT = sizeof(trollEvents) / sizeof(trollEvents[0]);

// If this fires, you added a row to one of the two lists and not the other.
static_assert(sizeof(trollEvents) / sizeof(trollEvents[0]) == TR_COUNT,
              "trollEvents[] and the TrollId enum are out of sync");

int        activeEventIndex     = -1;
TrollGates trollGates           = {false, false, false, false, false};
bool       trollAlarmAlwaysRings = true;   // default: never let a troll eat the alarm
bool       trollsAtNight         = true;   // default: keep rolling in night mode
bool       trollManualLaunch     = false;

static unsigned long eventStartMs = 0;   // millis() when the running event began

// Defined in the SCHEDULING section below, but needed before it.
static int      lastDailyDay = -1;   // calendar day the daily roll last happened on
static uint32_t msUntilDailyEnd();

// A fundamental is the only kind that never actually runs. Dailies do run, as
// background events, so they are launchable from the dev menu like anything else.
bool isEventKind(TrollKind k) { return k != TROLL_FUND; }

bool trollEnabled(TrollId id) {
    if (id < 0 || id >= TR_COUNT) return false;
    return trollEvents[id].enabled;
}

// ============================================================================
//  SAVED SETTINGS
//  Keyed by index rather than by label, so renaming a troll does not lose its
//  state.
// ============================================================================

// Load the saved switches, and seed today so a reboot does not count as a fresh
// day. Booting before the morning window leaves the day unclaimed, so that
// day's roll still happens when the hour comes round.
void setupTrollEvents() {
    randomSeed(esp_random());   // otherwise every boot rolls the same trolls
    loadTrollSettings();

    DateTime boot = getCurrentTime();
    lastDailyDay = (boot.hour() >= WAKE_UP_HOUR) ? boot.day() : -1;
}

void saveTrollSettings() {
    prefs.begin("troll", false);
    for (int i = 0; i < TROLL_EVENT_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        prefs.putBool(key, trollEvents[i].enabled);
    }
    prefs.putBool("alarmsafe", trollAlarmAlwaysRings);
    prefs.putBool("night", trollsAtNight);
    prefs.end();
}

void loadTrollSettings() {
    prefs.begin("troll", true);
    for (int i = 0; i < TROLL_EVENT_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        trollEvents[i].enabled = prefs.getBool(key, trollEvents[i].enabled);
    }
    trollAlarmAlwaysRings = prefs.getBool("alarmsafe", trollAlarmAlwaysRings);
    trollsAtNight         = prefs.getBool("night", trollsAtNight);
    prefs.end();
}

bool allTrollsEnabled() {
    for (int i = 0; i < TROLL_EVENT_COUNT; i++)
        if (!trollEvents[i].enabled) return false;
    return true;
}

// The menu's master switch.
void setAllTrolls(bool on) {
    for (int i = 0; i < TROLL_EVENT_COUNT; i++) trollEvents[i].enabled = on;
    saveTrollSettings();
}

// ============================================================================
//  INPUT GATES
// ============================================================================

void clearTrollGates() {
    trollGates = {false, false, false, false, false};
}

// Freeze every button, so nothing dismisses the event. Deliberately does NOT
// touch blockAlarmFire: "you cannot press your way out of this" and "the alarm
// does not ring" are separate decisions, and most trolls only want the first.
void blockAllTrollInput() {
    trollGates.blockEncoder   = true;
    trollGates.blockAlarmBtn  = true;
    trollGates.blockSnoozeBtn = true;
    trollGates.blockBrightBtn = true;
}

bool allTrollInputBlocked() {
    return trollGates.blockEncoder && trollGates.blockAlarmBtn
        && trollGates.blockSnoozeBtn && trollGates.blockBrightBtn;
}

// The alarm is only swallowed if a troll asked for it AND the safety switch is
// off. The single place that decision is made.
bool trollSuppressesAlarm() {
    return activeEventIndex != -1 && trollGates.blockAlarmFire && !trollAlarmAlwaysRings;
}

// ============================================================================
//  LIFECYCLE
// ============================================================================

unsigned long trollElapsedMs() {
    if (activeEventIndex == -1) return 0;
    return millis() - eventStartMs;
}

float trollProgress() {
    if (activeEventIndex == -1) return 1.0f;
    return animSince(eventStartMs, trollEvents[activeEventIndex].durationMs);
}

// Start an event. Owns the state change, so a background troll can never be
// handed the screen by an over-eager caller.
void triggerEventByIndex(int index, bool manual) {
    if (index < 0 || index >= TROLL_EVENT_COUNT) return;
    if (!isEventKind(trollEvents[index].kind)) return;   // fundamentals never run

    clearTrollGates();                 // start with everything allowed...
    activeEventIndex  = index;
    trollManualLaunch = manual;
    eventStartMs      = millis();

    // Every daily ends at bedtime, whenever it started. Done centrally, so the
    // daily triggers do not each have to work it out.
    if (trollEvents[index].kind == TROLL_DAILY)
        trollEvents[index].durationMs = msUntilDailyEnd();

    if (trollEvents[index].trigger) trollEvents[index].trigger();   // ...trigger raises gates

    if (!trollEvents[index].background) {
        currentState = STATE_TROLL_EVENT;
    } else if (currentState == STATE_MENU) {
        // Launched by hand from the dev menu: drop back to the clock so you can
        // actually see the effect.
        currentState = STATE_NORMAL_CLOCK;
        forceClockRedraw();
    }

    DEBUG_PRINTF("Troll event started: %s%s\n", trollEvents[index].label,
                 trollEvents[index].background ? " (background)" : "");
}

// Stop the running event WITHOUT touching currentState -- used when cancelling
// from the dev menu, where we want to stay in the menu.
void stopTrollEvent() {
    if (activeEventIndex == -1) return;

    // end() runs before the state is torn down, so it can still tell which
    // troll it belongs to. This is where a troll commits a lasting consequence.
    if (trollEvents[activeEventIndex].end) trollEvents[activeEventIndex].end();

    applyBrightness(brightnessLevel);   // in case a troll dimmed the panel
    clearTrollGates();
    resetClockOffsets();                // undo any shoved-around clock elements

    DEBUG_PRINTF("Troll event stopped: %s\n", trollEvents[activeEventIndex].label);
    activeEventIndex = -1;
}

// Stop the running event AND return to the clock.
void exitTrollEvent() {
    // Only claw the screen back if the troll actually had it. A background
    // troll ending, or a foreground one timing out while you are in the menu,
    // must not yank you somewhere you did not ask to go.
    bool hadScreen = (currentState == STATE_TROLL_EVENT);

    stopTrollEvent();
    forceClockRedraw();

    // If the alarm is ringing it owns the screen. A troll timing out underneath
    // it must not pull the state back to the clock, or the flashing stops and
    // playAlarmSound() -- only called from the alarm state -- never re-queues,
    // silently killing the ring.
    if (alarmActive || !hadScreen) return;
    currentState = STATE_NORMAL_CLOCK;
}

// Per-frame upkeep: force the redraw a background troll needs, and end the
// event once its duration is up.
void updateActiveTrollEvent() {
    if (activeEventIndex == -1) return;
    const TrollEvent &t = trollEvents[activeEventIndex];

    // A background troll's draw() is called from the clock's render path, which
    // normally only repaints on a minute change. Force it, or the overlay would
    // be frozen.
    if (t.background) forceClockRedraw();

    if (t.durationMs > 0 && millis() - eventStartMs >= t.durationMs) exitTrollEvent();
}

// ============================================================================
//  SCHEDULING
// ============================================================================

// Milliseconds from now until tonight's bedtime. A daily always ends then,
// regardless of when it started, so a manual launch mid-afternoon still stops
// at the right time. Launched after bedtime -- i.e. you are testing -- it just
// runs for an hour.
static uint32_t msUntilDailyEnd() {
    DateTime now = getCurrentTime();
    int left = NIGHT_MODE_HOUR * 60 - (now.hour() * 60 + now.minute());
    if (left <= 0) left = 60;
    return (uint32_t)left * 60000UL;
}

// Fire a random enabled event of a given kind. Returns false if none qualify.
static bool fireRandomOfKind(TrollKind kind) {
    int candidates[TR_COUNT];
    int count = 0;

    for (int i = 0; i < TROLL_EVENT_COUNT; i++) {
        if (!trollEvents[i].enabled || trollEvents[i].kind != kind) continue;
        // Skip trolls with nothing to do right now -- "Lose alarm" with no
        // alarm set -- so they do not consume the roll and waste the minute.
        if (trollEvents[i].canFire && !trollEvents[i].canFire()) continue;
        candidates[count++] = i;
    }

    if (count == 0) return false;
    triggerEventByIndex(candidates[random(0, count)]);
    return true;
}

// One minute's dice: a major first, then a minor.
void rollForTrollEvent() {
    if (activeEventIndex != -1) return;   // one at a time
    if (random(0, TROLL_MAJOR_ODDS) == 0 && fireRandomOfKind(TROLL_MAJOR)) return;
    if (random(0, TROLL_MINOR_ODDS) == 0) fireRandomOfKind(TROLL_MINOR);
}

// Is this a good moment to troll? Only on the plain clock screen, with nothing
// else going on.
static bool trollRollAllowed() {
    if (activeEventIndex != -1)             return false;
    if (currentState != STATE_NORMAL_CLOCK) return false;   // covers menu, alarm and BT
    if (alarmActive || snoozeActive)        return false;
    if (bluetoothActive)                    return false;
    // A dimmed screen means you are asleep. This gates the RANDOM roll only;
    // launching by hand still works, so you can test at night.
    if (!trollsAtNight && isNightMode())    return false;
    return true;
}

// The morning roll for the all-day moods. Fires the first time we see a new
// calendar day at or after the wake-up hour, so it lands as you get up rather
// than in the middle of the night.
static void rollForDailyTroll() {
    DateTime now = getCurrentTime();
    if (now.hour() < WAKE_UP_HOUR) return;   // too early; the day stays unclaimed

    int today = now.day();
    if (today == lastDailyDay) return;       // already had our shot today
    lastDailyDay = today;

    if (activeEventIndex != -1) return;
    if (random(0, TROLL_DAILY_ODDS) != 0) return;
    fireRandomOfKind(TROLL_DAILY);
}

static bool isWeatherTroll(int idx) {
    return idx >= TR_WX_DRIZZLE && idx <= TR_WX_STORM;
}

// Keep the running weather scene in step with the actual forecast. Not a roll:
// there is no chance involved, it just mirrors weatherCode.
static void updateWeatherAuto() {
    if (alarmActive) return;   // the ring owns everything

    int want = weatherTrollForCode(weatherCode);
    // Do not animate weather we cannot vouch for, and respect the per-scene
    // toggle, so you can switch off just the ones you dislike.
    if (weatherUnavailable)                      want = -1;
    if (want >= 0 && !trollEvents[want].enabled) want = -1;

    if (activeEventIndex == -1) {
        if (want < 0) return;
    } else {
        // A hand-launched troll is a demo; real weather does not get to cut it short.
        if (trollManualLaunch) return;

        if (isWeatherTroll(activeEventIndex)) {
            if (activeEventIndex == want) return;   // already showing the right one
            exitTrollEvent();                       // conditions changed or cleared
            if (want < 0) return;
        } else {
            if (want < 0) return;
            // Weather outranks a MINOR and cuts it short -- but only a minor.
            // Majors are the headline gags (a fake update should not be rained
            // off) and dailies were deliberately given the whole day.
            if (trollEvents[activeEventIndex].kind != TROLL_MINOR) return;
            exitTrollEvent();
        }
    }

    triggerEventByIndex(want);
}

// Called every frame; does its work once a minute. A blocked minute is simply
// skipped rather than queued, so leaving the menu does not dump a backlog of
// trolls on you.
void updateTrollTimer() {
    static unsigned long lastRollMs = 0;
    if (millis() - lastRollMs < TROLL_ROLL_INTERVAL_MS) return;
    lastRollMs = millis();

    // Nobody has been here in days: shut everything down and stay quiet until a
    // button wakes us. Checked before the weather too -- an empty room does not
    // need to be told it is raining.
    if (idleMode) {
        if (activeEventIndex != -1) exitTrollEvent();
        return;
    }

    updateWeatherAuto();   // mirror the forecast before rolling for anything else
    rollForDailyTroll();   // checked every minute; catches the morning window

    if (!trollRollAllowed()) return;
    rollForTrollEvent();
}

// ============================================================================
//  RENDERING AND INPUT
// ============================================================================

// The STATE_TROLL_EVENT screen.
void drawTrollEvent(int index) {
    if (index < 0 || index >= TROLL_EVENT_COUNT) return;
    const TrollEvent &t = trollEvents[index];

    u8g2.clearBuffer();
    if (t.overClock) drawClockFace();   // overlay gags sit on top of the real clock

    if (t.draw) {
        t.draw();
    } else {
        // Placeholder for a troll with no draw() written yet, so you can still
        // launch it and see that the plumbing works.
        u8g2.setFont(u8g2_font_6x10_tf);
        centerText(t.label);
        if (!allTrollInputBlocked()) {
            u8g2.setFont(u8g2_font_5x7_tf);
            u8g2.drawStr(2, 56, "any button: exit");
        }
    }
    u8g2.sendBuffer();
}

// Background trolls paint over the finished clock face.
void drawTrollOverlay() {
    if (activeEventIndex == -1) return;
    const TrollEvent &t = trollEvents[activeEventIndex];
    if (t.background && t.draw) t.draw();
}

// Any button dismisses the event -- unless the troll supplied an onInput(), in
// which case it decides what a press means, and is then responsible for ending
// the event itself.
//
// These are only reached for buttons whose gate is down; the app loop checks
// first, so a troll that called blockAllTrollInput() never sees them. They stay
// as four entry points so a troll can block one button, or give an individual
// button its own meaning.
static void trollButtonPressed() {
    if (activeEventIndex == -1) return;
    if (trollEvents[activeEventIndex].onInput) trollEvents[activeEventIndex].onInput();
    else                                       exitTrollEvent();
}

void handleTrollEventClick()  { trollButtonPressed(); }
void handleTrollEventSnooze() { trollButtonPressed(); }
void handleTrollEventAlarm()  { trollButtonPressed(); }
void handleTrollEventBright() { trollButtonPressed(); }

// ############################################################################
//
//   TROLL IMPLEMENTATIONS
//
//   One trigger()/draw() pair per troll. draw() runs every frame and should be
//   a pure function of trollProgress() or trollElapsedMs() -- no frame
//   counters, no state to reset. The buffer is already cleared, and the clock
//   face already drawn if overClock is true; just draw, and do not send.
//
//   Copy either of these two as a template:
//     drawFishSwim    an overlay gag, harmless, times out on its own
//     drawFakeUpdate  a full-screen takeover that freezes the inputs
//
// ############################################################################

// ============================================================================
//  FULL-SCREEN TAKEOVERS
// ============================================================================

// ---- Fake update -----------------------------------------------------------
// The headline troll: a progress bar that takes an age and locks the clock out
// while it runs. The dev code is the only way out early.

static void triggerFakeUpdate() {
    blockAllTrollInput();
    trollGates.blockAlarmFire = true;   // ignored unless "Alarm OK" is switched off
}

static void drawFakeUpdate() {
    float t = trollProgress();

    u8g2.setFont(u8g2_font_6x10_tf);
    drawStrCentered(8, "Updating firmware");
    drawProgressBar(8, 26, 112, 12, t);

    char line[24];
    snprintf(line, sizeof(line), "%d%% complete", (int)(t * 100));
    drawStrCentered(42, line);

    u8g2.setFont(u8g2_font_5x7_tf);
    if (blink(1200)) drawStrCentered(55, "Do not unplug");
}

// ---- Eye stare / Eye stare+ ------------------------------------------------
// A giant eye watches you, blinks like a real one, and occasionally whispers.
// TWO table rows share this implementation, because `kind` is read by the
// random roll BEFORE trigger() runs -- a troll cannot pick its own severity
// after the fact, and an hour-long lockout firing at minor odds would be
// unbearable. Splitting it also gives you two independent switches, so the
// harmless one can stay armed while the long one is off.
//
// Only the major locks you in. The minor is an ordinary dismissible minor,
// which matters now that it runs for half an hour: an unblinkable eye you could
// not press your way out of would be a major in all but name.

static uint32_t lockoutDuration();   // defined with the icon lockouts below

// All timings below are ms SINCE THE EVENT STARTED, so they line up directly
// with trollElapsedMs().
static unsigned long eyeNextBlinkAt   = 0;
static unsigned long eyeBlinkStartMs  = 0;
static bool          eyeBlinking      = false;
static unsigned long eyeNextWhisperAt = 0;

static void triggerEyeStare() {
    bool major = (activeEventIndex == TR_EYE_STARE_PLUS);

    trollEvents[activeEventIndex].durationMs = major ? lockoutDuration()
                                                     : TROLL_BLIND_MINOR_MS;
    if (major) {
        blockAllTrollInput();
        trollGates.blockAlarmFire = true;
    }

    eyeBlinking      = false;
    eyeNextBlinkAt   = random(EYE_GAP_MIN, EYE_GAP_MAX);
    eyeNextWhisperAt = random(EYE_WHISPER_MIN, EYE_WHISPER_MAX);

    DEBUG_PRINTF("Eye stare%s: %lu ms\n", major ? "+" : "",
                 (unsigned long)trollEvents[activeEventIndex].durationMs);
}

static void drawEyeStare() {
    unsigned long e = trollElapsedMs();
    uint32_t blinkLen = animLengthMs(ANIM_EYE_BLINK);

    // Hold the eye open, fire one blink, then roll the next gap. Blinking
    // occasionally twice in quick succession is what stops the cadence reading
    // as a machine.
    if (!eyeBlinking && e >= eyeNextBlinkAt) {
        eyeBlinking     = true;
        eyeBlinkStartMs = e;
    }
    if (eyeBlinking && e - eyeBlinkStartMs >= blinkLen) {
        eyeBlinking = false;
        eyeNextBlinkAt = e + (random(0, EYE_DOUBLE_ODDS) == 0
                              ? EYE_DOUBLE_GAP
                              : (uint32_t)random(EYE_GAP_MIN, EYE_GAP_MAX));
    }

    // Whispers at any hour: the eye telling you to wake up in the middle of the
    // afternoon is funnier, not less.
    if (e >= eyeNextWhisperAt) {
        eyeNextWhisperAt = e + random(EYE_WHISPER_MIN, EYE_WHISPER_MAX);
        queueSound(SND_WHISPER, GAIN_WHISPER);
    }

    // sinceMs = 0 parks on step 0, the eye held open. 64px wide, so x = 32.
    drawAnim(ANIM_EYE_BLINK, 32, 0, eyeBlinking ? e - eyeBlinkStartMs : 0, false);
}

// ---- Walker ----------------------------------------------------------------
// A figure paces across the display, off one edge and back on the other, with a
// footstep sound on each footfall. The sprite walks IN PLACE -- the travel is
// this troll sliding x along, so the two have to agree or the figure moonwalks.
// WALK_STRIDE_PX is what keeps them honest.

// Total elapsed animation steps at the last drawn frame, so we can tell how
// many footfalls went by. -1 means "first frame, nothing has gone by yet".
static long walkLastTotalStep;

// How many times step `target` has come round by absolute step `total`.
// Counting rather than testing for equality is what makes this skip-proof: the
// render loop is slower than one animation step, so a plain equality check
// would silently drop footfalls whenever a frame straddled the planted pose.
static long walkFootfallsBy(long total, int target, int count) {
    if (total < target) return 0;
    return (total - target) / count + 1;
}

static void triggerWalker() {
    walkLastTotalStep = -1;
}

static void drawWalker() {
    unsigned long e     = trollElapsedMs();
    const Anim&   a     = ANIM_WALK;
    long          total = (long)(e / a.frameMs);   // the same arithmetic drawAnim uses

    // Two strides per cycle, so a full pass carries the figure two strides.
    // Integer maths throughout: at these speeds the rounding is under a pixel,
    // and it keeps floats out of the render path.
    uint32_t cycle  = animLengthMs(a);
    long     travel = (long)((e * (2UL * WALK_STRIDE_PX)) / cycle);

    int span = SCREEN_W + a.w;   // walk on at the left, off at the right, repeat
    drawAnim(a, (int)(travel % span) - a.w, 0, e, true);

    // Both planted poses are checked separately because they are not evenly
    // spaced in the cycle, so the gap between footfalls alternates slightly --
    // which is what a real gait does.
    if (walkLastTotalStep >= 0) {
        bool landed = false;
        for (int t : { WALK_STEP_A, WALK_STEP_B }) {
            if (walkFootfallsBy(total, t, a.stepCount) >
                walkFootfallsBy(walkLastTotalStep, t, a.stepCount)) landed = true;
        }
        // One call even if a long stall swallowed both footfalls: playSound
        // restarts the clip anyway, so a second would just cut the first off.
        if (landed) playSound(SND_FOOTSTEP, GAIN_FOOTSTEP);
    }
    walkLastTotalStep = total;
}

// ============================================================================
//  COUNTDOWNS
//  Two trolls that put a timer on screen and make you watch it.
// ============================================================================

// Shared beeping. A marker beep every `markEverySec`, then one on EVERY
// remaining second once inside the final ten -- the acceleration is what makes
// a countdown feel like a countdown.
//
// Driven by REMAINING seconds rather than elapsed, so the final-ten rule lines
// up with the number on screen. Tracking the last second we fired on means a
// long frame can neither skip a beep nor fire two.
static int cdLastSec = -1;

static void countdownBeeps(uint32_t remMs, int markEverySec) {
    int rem = (int)((remMs + 999) / 1000);   // round up, so it ends on 1 and not 0
    if (rem == cdLastSec) return;
    cdLastSec = rem;
    if (rem <= 0) return;
    if (rem <= 10 || rem % markEverySec == 0) playSound(SND_BEEP, GAIN_COUNTDOWN);
}

// Seconds and milliseconds, big enough that the millisecond digits actually
// read. Shared by both countdowns.
static void drawCountdownNumber(int y, uint32_t remMs) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu.%03lu",
             (unsigned long)(remMs / 1000), (unsigned long)(remMs % 1000));
    u8g2.setFont(u8g2_font_10x20_tf);
    drawStrCentered(y, buf);
}

// ---- Error popup -----------------------------------------------------------
// A panel over the middle of the clock counting down to a reset, then a fake
// reboot. The panel is inset on purpose, so the date and temperature stay
// visible in the corners: it reads as a dialog on top of a working clock rather
// than a whole new screen.

static uint32_t errCountdownMs = 0;

static void triggerErrorPopup() {
    errCountdownMs = (uint32_t)random(CD_MIN_SECONDS, CD_MAX_SECONDS + 1) * 1000UL;
    trollEvents[TR_ERROR_POPUP].durationMs = errCountdownMs + ERR_REBOOT_MS;
    cdLastSec = -1;

    blockAllTrollInput();
    trollGates.blockAlarmFire = true;

    DEBUG_PRINTF("Error popup: %lu s countdown\n", (unsigned long)(errCountdownMs / 1000));
}

static void drawErrorPopup() {
    unsigned long e = trollElapsedMs();

    // Phase 2: the fake reboot -- a dead screen first, like a real reset, then
    // the same splash setupDisplay() shows.
    if (e >= errCountdownMs) {
        u8g2.clearBuffer();   // wipe the clock face overClock already drew
        if (e - errCountdownMs >= ERR_BLACK_MS) {
            u8g2.setFont(u8g2_font_ncenB08_tr);
            centerText("Loading...");
        }
        return;
    }

    // Phase 1: the countdown.
    uint32_t rem = errCountdownMs - e;
    countdownBeeps(rem, ERR_BEEP_EVERY_S);

    const int bx = 9, by = 6, bw = 110, bh = 48;
    u8g2.setDrawColor(0);
    u8g2.drawBox(bx, by, bw, bh);      // erase the interior
    u8g2.setDrawColor(1);
    u8g2.drawFrame(bx, by, bw, bh);

    u8g2.setFont(u8g2_font_6x10_tf);
    drawStrCentered(by + 3, "SYSTEM ERROR");
    u8g2.setFont(u8g2_font_5x7_tf);
    drawStrCentered(by + 15, "resetting in");
    drawCountdownNumber(by + 25, rem);
}

// ---- Self destruct ---------------------------------------------------------
// The error popup's nastier sibling, in four phases:
//   1. COUNTDOWN  the same length, but beeping the whole way down
//   2. DETONATE   the screen strobes and the bang goes off at full volume
//   3. DEAD       ten full seconds of absolutely nothing
//   4. REBUILD    the clock's four elements slide back in from off screen
//
// The dead screen is the point. The flash and the bang are over in two seconds,
// and then you are just staring at a black panel wondering if you killed it.

// Where each element gets blown to. Different directions, so the rebuild reads
// as four separate pieces coming back rather than one image sliding in.
static const int SD_OFF_X[CE_COUNT] = {   0, -90,  90,   0 };
static const int SD_OFF_Y[CE_COUNT] = { -70,   0,   0,  40 };

static uint32_t sdCountdownMs = 0;
static bool     sdBoomed      = false;

static void triggerSelfDestruct() {
    sdCountdownMs = (uint32_t)random(CD_MIN_SECONDS, CD_MAX_SECONDS + 1) * 1000UL;
    trollEvents[TR_SELF_DESTRUCT].durationMs =
        sdCountdownMs + SD_FLASH_MS + SD_DEAD_MS + SD_BACK_MS;
    cdLastSec = -1;
    sdBoomed  = false;

    blockAllTrollInput();
    trollGates.blockAlarmFire = true;

    DEBUG_PRINTF("Self destruct: %lu s\n", (unsigned long)(sdCountdownMs / 1000));
}

static void drawSelfDestruct() {
    unsigned long e = trollElapsedMs();

    // Phase 1: the countdown, in a double-bordered panel.
    if (e < sdCountdownMs) {
        uint32_t rem = sdCountdownMs - e;
        countdownBeeps(rem, SD_BEEP_EVERY_S);

        const int bx = 7, by = 5, bw = 114, bh = 50;
        u8g2.setDrawColor(0);
        u8g2.drawBox(bx, by, bw, bh);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(bx, by, bw, bh);
        u8g2.drawFrame(bx + 2, by + 2, bw - 4, bh - 4);

        u8g2.setFont(u8g2_font_6x10_tf);
        drawStrCentered(by + 5, "SELF DESTRUCT");
        u8g2.setFont(u8g2_font_5x7_tf);
        if (blink(700)) drawStrCentered(by + 16, "DETONATION IN");
        drawCountdownNumber(by + 26, rem);
        return;
    }

    unsigned long since = e - sdCountdownMs;

    // Phase 2: detonation. The offsets are set once here and held through the
    // dead phase, then lerped back in phase 4.
    if (since < SD_FLASH_MS) {
        if (!sdBoomed) {
            sdBoomed = true;
            playSound(SND_BOMB, AUDIO_MAX_GAIN);
            for (int i = 0; i < CE_COUNT; i++) {
                clockElemOffX[i] = SD_OFF_X[i];
                clockElemOffY[i] = SD_OFF_Y[i];
            }
        }
        u8g2.clearBuffer();
        if ((since / SD_STROBE_MS) % 2 == 0) u8g2.drawBox(0, 0, SCREEN_W, SCREEN_H);
        return;
    }

    // Phase 3: the long nothing.
    if (since < SD_FLASH_MS + SD_DEAD_MS) {
        u8g2.clearBuffer();
        return;
    }

    // Phase 4: the clock crawls back. These offsets are read by drawClockFace()
    // on the NEXT frame, a lag nobody can see across a 1.6 second slide.
    float t = (float)(since - SD_FLASH_MS - SD_DEAD_MS) / SD_BACK_MS;
    if (t > 1.0f) t = 1.0f;
    float k = easeInOut(t);
    for (int i = 0; i < CE_COUNT; i++) {
        clockElemOffX[i] = lerpI(SD_OFF_X[i], 0, k);
        clockElemOffY[i] = lerpI(SD_OFF_Y[i], 0, k);
    }
}

// ============================================================================
//  FISH
//  Three trolls share the drawing here: a school that crosses the clock, a
//  shoal that carries it off, and an all-day aquarium.
// ============================================================================

// ---- one fish --------------------------------------------------------------
// Drawn with primitives rather than a sprite, in three passes, so it stays
// readable on top of the clock digits:
//   1. an oversized silhouette in draw colour 0, which ERASES a fish-shaped
//      hole in whatever is behind it, giving a dark border
//   2. the real body in colour 1, sitting inside that hole -- a solid white fish
//   3. the eye back in colour 0, punching a dark hole in the white body
// Pass 1 is invisible over a blank screen and only shows where it eats into a
// lit digit, which is exactly what you want.
//
// Mirroring is just multiplying every x OFFSET by d. The body is symmetric, so
// only the tail and the eye actually move.
static void drawOneFish(int x, int y, int d) {
    u8g2.setDrawColor(0);                                              // 1. dark halo
    u8g2.drawFilledEllipse(x, y, 9, 6);
    u8g2.drawTriangle(x - 2*d, y, x - 15*d, y - 7, x - 15*d, y + 7);

    u8g2.setDrawColor(1);                                              // 2. body and tail
    u8g2.drawFilledEllipse(x, y, 7, 4);
    u8g2.drawTriangle(x - 4*d, y, x - 13*d, y - 5, x - 13*d, y + 5);

    // 3. the eye. drawBox takes a TOP-LEFT corner, so the mirrored one starts
    // 4px back rather than 3px forward.
    u8g2.setDrawColor(0);
    u8g2.drawBox(x + (d > 0 ? 3 : -4), y - 2, 2, 2);

    u8g2.setDrawColor(1);   // always restore: the draw colour is global
}

// ---- bubbles ---------------------------------------------------------------
// A tiny particle pool shared by the shoal and the aquarium. Bubbles rise,
// wobble sideways, and expire. A free slot is one with lifeMs == 0.

struct Bubble {
    int16_t       x, y0;
    unsigned long startMs;
    uint16_t      lifeMs;   // 0 means the slot is free
    uint8_t       r;
};
static Bubble bubbles[BUBBLE_MAX];

static void resetBubbles() {
    for (int i = 0; i < BUBBLE_MAX; i++) bubbles[i].lifeMs = 0;
}

// Claim the first free slot, or silently drop the bubble if the pool is full --
// one missing bubble is invisible.
static void spawnBubble(int x, int y) {
    for (int i = 0; i < BUBBLE_MAX; i++) {
        if (bubbles[i].lifeMs) continue;
        bubbles[i].x       = (int16_t)x;
        bubbles[i].y0      = (int16_t)y;
        bubbles[i].startMs = millis();
        bubbles[i].lifeMs  = (uint16_t)random(BUBBLE_LIFE_MIN, BUBBLE_LIFE_MAX);
        bubbles[i].r       = (uint8_t)random(1, 3);
        return;
    }
}

// Bubbles come in BURSTS, not a steady stream: a puff of a few at once, then
// nothing for ages. The scatter is what makes them read as one exhale rather
// than three unrelated dots.
static void spawnBubbleBurst(int x, int y) {
    int n = (int)random(3, 6);
    for (int i = 0; i < n; i++)
        spawnBubble(x + jitter(3), y - i * 2 - (int)random(0, 3));
}

// Rise, wobble, expire. The +i in the sine keeps them out of sync with each
// other.
static void drawBubbles() {
    for (int i = 0; i < BUBBLE_MAX; i++) {
        Bubble &b = bubbles[i];
        if (!b.lifeMs) continue;

        unsigned long e = millis() - b.startMs;
        if (e >= b.lifeMs) { b.lifeMs = 0; continue; }

        float t = (float)e / b.lifeMs;
        int y = b.y0 - (int)(t * BUBBLE_RISE_PX);
        int x = b.x + (int)(3.0f * sinf(t * 6.0f + i));
        u8g2.drawCircle(x, y, b.r);
    }
}

// ---- lone-fish patrol ------------------------------------------------------
// One fish at a time, alternating direction, with irregular gaps between runs.
// Used by the shoal's final act and by the aquarium, at a slower pace. Only one
// troll runs at a time, so a single set of state is safe to share.

static bool          patrolActive   = false;
static int           patrolY        = 32;
static int           patrolDir      = -1;   // flipped before each run, so the first is +1
static unsigned long patrolStartMs  = 0;
static uint32_t      patrolTravelMs = 0;
static unsigned long patrolNextAt   = 0;

static void patrolReset(unsigned long firstAt) {
    patrolActive = false;
    patrolDir    = -1;
    patrolNextAt = firstAt;
}

// e is ms since the event started. Speed and gap are ranges, so callers set the
// pace.
static void patrolTick(unsigned long e, uint32_t travelMin, uint32_t travelMax,
                       uint32_t gapMin, uint32_t gapMax, int yMin, int yMax,
                       bool blowBubbles) {
    if (!patrolActive && e >= patrolNextAt) {
        patrolActive   = true;
        patrolStartMs  = e;
        patrolY        = (int)random(yMin, yMax);
        patrolDir      = -patrolDir;   // take turns, back and forth
        patrolTravelMs = (uint32_t)random(travelMin, travelMax);
    }
    if (!patrolActive) return;

    float t = (float)(e - patrolStartMs) / patrolTravelMs;
    if (t >= 1.0f) {
        patrolActive = false;
        patrolNextAt = e + (uint32_t)random(gapMin, gapMax);   // intermittent, not rhythmic
        return;
    }

    int x = patrolDir > 0 ? lerpI(-16, SCREEN_W + 12, t) : lerpI(SCREEN_W + 12, -16, t);
    int y = patrolY + (int)(5.0f * sinf(t * 8.0f));
    drawOneFish(x, y, patrolDir);

    // A very occasional puff from the mouth, which flips with direction. Rare
    // enough to be a surprise rather than a feature.
    if (blowBubbles && random(0, PATROL_BUBBLE_ODDS) == 0)
        spawnBubbleBurst(x + patrolDir * 7, y - 3);
}

// ---- Fish swim -------------------------------------------------------------
// A school crosses the clock and leaves. Every launch rolls a new school: how
// many, and per fish its height, direction, speed, entry delay and bob.
//
// This state is the ONE exception to "draw() holds no state". It is rolled once
// in trigger() and only read afterwards, so draw() is still a pure function of
// (time, school) -- which is what keeps the fish frame-rate independent and
// lets them swim at different speeds with no bookkeeping.

struct Fish {
    int           y;          // swim height, centre of the body
    int           dir;        // +1 swims right, -1 swims left
    unsigned long startMs;    // absolute time this one enters
    uint32_t      travelMs;   // how long it takes to cross, i.e. its speed
    float         bobAmp;     // vertical wobble, in pixels
    float         bobRate;    // wobble cycles across the crossing
};
static Fish fish[FISH_MAX];
static int  fishCount = 0;

static void triggerFishSwim() {
    fishCount = (int)random(FISH_MIN_COUNT, FISH_MAX + 1);

    uint32_t longestMs = 0;
    for (int i = 0; i < fishCount; i++) {
        Fish &f = fish[i];
        uint32_t delay = (uint32_t)random(0, FISH_STAGGER);

        f.y        = (int)random(FISH_Y_MIN, FISH_Y_MAX);
        f.dir      = random(0, 2) ? 1 : -1;
        f.travelMs = (uint32_t)random(FISH_FAST_MS, FISH_SLOW_MS);
        f.startMs  = millis() + delay;
        f.bobAmp   = (float)random(2, 7);
        f.bobRate  = (float)random(6, 15);

        if (delay + f.travelMs > longestMs) longestMs = delay + f.travelMs;
    }

    // Stretch the event to fit the slowest fish, or the school gets cut off
    // mid-screen. The table is not const and trollProgress() reads durationMs
    // live, so writing it here just works.
    trollEvents[TR_FISH_SWIM].durationMs = longestMs + FISH_MARGIN;

    // The ambience, once. It is longer than most schools, so end() cuts it off
    // rather than letting it run on over the clock after the fish have gone.
    playSound(SND_UNDERWATER, GAIN_UNDERWATER);

    DEBUG_PRINTF("Fish swim: %d fish, %lu ms\n", fishCount,
                 (unsigned long)(longestMs + FISH_MARGIN));
}

// Silence the ambience when a fish troll ends, however it ends -- otherwise a
// clip that just restarted plays on over the clock for another minute. Shared
// by Fish swim and Fish swarm.
//
// Skipped while the alarm is ringing: a troll timing out underneath a ring must
// not cut the alarm clip off. (playAlarmSound() would re-queue it within a
// frame, but silencing the alarm even briefly is not worth it.)
static void endFishAmbience() {
    if (alarmActive) return;
    stopSound();
}

static void drawFishSwim() {
    for (int i = 0; i < fishCount; i++) {
        Fish &f = fish[i];

        // Not in the water yet. A SIGNED compare, because millis() minus a
        // future startMs underflows, and animSince() would read that as "long
        // finished" and park the fish at the far edge.
        if ((long)(millis() - f.startMs) < 0) continue;

        float t = animSince(f.startMs, f.travelMs);
        if (t >= 1.0f) continue;   // already swum off

        int x = f.dir > 0 ? lerpI(-16, SCREEN_W + 12, t) : lerpI(SCREEN_W + 12, -16, t);
        int y = f.y + (int)(f.bobAmp * sinf(t * f.bobRate));
        drawOneFish(x, y, f.dir);
    }
}

// ---- Fish swarm ------------------------------------------------------------
// Three acts:
//   1. SWARM   a shoal floods across and completely covers the clock
//   2. REVEAL  they leave, and the clock is gone with them
//   3. PATROL  one lone fish at a time cruises past until you press something
//
// The whole shoal swims ONE way, fast, in staggered rows. The rows matter: with
// random heights the shoal left gaps and the clock showed through. Assigning
// each fish a band guarantees the screen is covered before anything vanishes.
//
// The trick in act 2 is that the clock is not animated away -- at peak density
// every element is SNAPPED off screen using the element offsets. The snap is
// completely hidden behind the fish, so it reads as though they carried it off.
// The offsets reset themselves when the event ends, so a button brings the
// clock straight back.

struct SwarmFish {
    int           y;
    int           dir;
    unsigned long startMs;    // ms since the event started
    uint32_t      travelMs;
};
static SwarmFish swarm[SWARM_MAX];
static int       swarmCount    = 0;
static uint32_t  swarmEndMs    = 0;   // when act 1 is fully over
static uint32_t  swarmHideAtMs = 0;   // when to snap the clock away
static bool      swarmHidden   = false;

static void triggerFishSwarm() {
    swarmCount   = (int)random(SWARM_MIN_COUNT, SWARM_MAX + 1);
    int swarmDir = random(0, 2) ? 1 : -1;   // the whole shoal moves as one

    uint32_t longest = 0;
    for (int i = 0; i < swarmCount; i++) {
        SwarmFish &f = swarm[i];
        uint32_t delay = (uint32_t)random(0, SWARM_STAGGER);
        f.y        = 8 + (i % SWARM_ROWS) * 9 + jitter(2);
        f.dir      = swarmDir;
        f.travelMs = (uint32_t)random(SWARM_FAST_MS, SWARM_SLOW_MS);
        f.startMs  = delay;
        if (delay + f.travelMs > longest) longest = delay + f.travelMs;
    }

    swarmEndMs    = longest + 200;
    swarmHideAtMs = swarmEndMs * SWARM_HIDE_PCT / 100;
    swarmHidden   = false;

    resetBubbles();
    patrolReset(swarmEndMs + (uint32_t)random(600, 1900));

    playSound(SND_FISH_SWARM, GAIN_FISH_SWARM);   // the rush, over act 1
}

// Shove every clock element far off screen, not animated -- see above.
static void hideClockElements() {
    for (int i = 0; i < CE_COUNT; i++) clockElemOffY[i] = -200;
}

static void drawFishSwarm() {
    unsigned long e = trollElapsedMs();

    if (!swarmHidden && e >= swarmHideAtMs) swarmHidden = true;
    if (swarmHidden) hideClockElements();   // re-assert every frame

    // Acts 1 and 2: the shoal.
    for (int i = 0; i < swarmCount; i++) {
        SwarmFish &f = swarm[i];
        if (e < f.startMs) continue;
        float t = (float)(e - f.startMs) / f.travelMs;
        if (t >= 1.0f) continue;

        int x = f.dir > 0 ? lerpI(-16, SCREEN_W + 12, t) : lerpI(SCREEN_W + 12, -16, t);
        int y = f.y + (int)(4.0f * sinf(t * 9.0f));
        drawOneFish(x, y, f.dir);

        // One of the shoal might puff on the way past. Rare on purpose: with
        // thirty fish on screen, even a small per-fish chance becomes a fog.
        if ((i & 3) == 0 && random(0, SWARM_BUBBLE_ODDS) == 0) spawnBubbleBurst(x + f.dir * 7, y - 3);
    }

    drawBubbles();

    // Act 3: the lone patrol, on an empty screen, with the ambience running
    // underneath it. There is no loop flag in the audio engine, so looping is
    // just re-queueing whenever it falls idle -- audioBusy() covers the gap
    // between clips as well as the clip itself, so this cannot stack up. It
    // also waits out the swarm rush from trigger(), which is still playing.
    if (e < swarmEndMs) return;
    if (!audioBusy()) queueSound(SND_UNDERWATER, GAIN_UNDERWATER);

    patrolTick(e, PATROL_SWARM_FAST_MS, PATROL_SWARM_SLOW_MS,
               PATROL_SWARM_GAP_MIN, PATROL_SWARM_GAP_MAX, FISH_Y_MIN, FISH_Y_MAX, true);
}

// ---- Aquarium --------------------------------------------------------------
// A whole scene drawn over the STILL-WORKING clock, all day. The clock keeps
// time, the menu opens, the alarm rings -- there is just a fish tank in the way.
// This is what background trolls are for.
//
// The floor is a filled shape across the bottom, which buries the date,
// temperature and alarm indicator. They are immediately redrawn in draw colour
// 0, so they come back as black text on the white sand and stay perfectly
// legible. Calling the clock's own draw functions means their positions and
// rules -- night mode, the snooze blink -- are inherited for free.

static const int AQ_FLOOR_TOP = AQ_FLOOR_LOW - AQ_FLOOR_RANGE;   // highest crest

static unsigned long aqNextBubbleAt = 0;

// Sand height at x. Two sines at unrelated frequencies, so the ridge never
// visibly repeats across the screen -- one sine alone read as a regular wave.
static int aqFloorY(int x) {
    float f = sinf(x * 0.075f) * 0.60f + sinf(x * 0.031f + 1.3f) * 0.40f;   // -1..1
    return AQ_FLOOR_LOW - (int)((f * 0.5f + 0.5f) * AQ_FLOOR_RANGE);
}

// A swaying strand, rooted on the sand wherever it happens to be.
static void drawSeaweed(int baseX, int height, float phase) {
    int base = aqFloorY(baseX);
    int prevX = baseX, prevY = base;
    const int SEGS = 6;
    for (int i = 1; i <= SEGS; i++) {
        float f = (float)i / SEGS;
        int y = base - (int)(height * f);
        int x = baseX + (int)(6.0f * f * sinf(phase + f * 2.5f));
        u8g2.drawLine(prevX, prevY, x, y);
        prevX = x; prevY = y;
    }
}

static void triggerAquarium() {
    clockSmallMode = true;   // a small clock all day; the tank is the feature
    resetBubbles();
    aqNextBubbleAt = (uint32_t)random(4000, 20000);
    patrolReset((uint32_t)random(1500, 5000));
}

static void drawAquarium() {
    unsigned long e = trollElapsedMs();

    // Sand: a filled column per x, following the rolling surface.
    for (int x = 0; x < SCREEN_W; x++) {
        int surf = aqFloorY(x);
        u8g2.drawVLine(x, surf, SCREEN_H - surf);
    }

    // Punch the buried readouts back in as black, so they read as printed on it.
    u8g2.setDrawColor(0);
    drawDate();
    displayWeather();
    drawAlarmIndicator();
    u8g2.setDrawColor(1);

    float s = e / 1000.0f;
    drawSeaweed(7,   26, s * 1.10f);
    drawSeaweed(120, 20, s * 0.80f + 1.7f);

    // Bubbles, in threes, rarely.
    if (e >= aqNextBubbleAt) {
        aqNextBubbleAt = e + (uint32_t)random(AQ_BUBBLE_GAP_MIN, AQ_BUBBLE_GAP_MAX);
        for (int i = 0; i < 3; i++) {
            int bx = (int)random(12, SCREEN_W - 12);
            spawnBubble(bx, aqFloorY(bx) - 1);
        }
        // With an alarm armed you are presumably trying to sleep near this
        // thing, so the tank shuts up and just looks pretty.
        if (!alarmEnabled) playSound(SND_BUBBLES, GAIN_BUBBLES);
    }
    drawBubbles();

    // The residents: the same patrol as the shoal's last act, but ambling.
    patrolTick(e, PATROL_AQ_FAST_MS, PATROL_AQ_SLOW_MS,
               PATROL_AQ_GAP_MIN, PATROL_AQ_GAP_MAX, FISH_Y_MIN, AQ_FLOOR_TOP - 8, true);
}

// ============================================================================
//  ICON LOCKOUTS
//  Trolls that put a single u8g2 icon on an otherwise empty screen and give you
//  nothing to do about it for a long time. Every button is dead; the dev code
//  is the only escape.
//
//  GLYPH NOTE: u8g2's streamline icon fonts pack their icons into consecutive
//  character codes starting at '0', so the Nth icon is 0x30 + (N-1). If a glyph
//  below draws the wrong picture, step the constant.
// ============================================================================

#define PETS_FONT    u8g2_font_streamline_pet_animals_t
#define CAT_GLYPH    0x32   // 3rd icon
#define FROG_GLYPH   0x35   // 6th icon, and it faces LEFT
#define TURTLE_GLYPH 0x3B   // 12th icon, and it faces LEFT
#define EMAIL_FONT   u8g2_font_streamline_email_t
#define EMAIL_GLYPH  0x32   // 3rd icon

// The "just sit there and take it" duration these share.
static uint32_t lockoutDuration() {
    return (uint32_t)random(TROLL_LOCKOUT_MIN_MINUTES,
                            TROLL_LOCKOUT_MAX_MINUTES + 1) * 60000UL;
}

// Roll a lockout length and freeze every input.
static void beginIconLockout(int idx) {
    trollEvents[idx].durationMs = lockoutDuration();
    blockAllTrollInput();
    trollGates.blockAlarmFire = true;
    DEBUG_PRINTF("%s: %lu min\n", trollEvents[idx].label,
                 (unsigned long)(trollEvents[idx].durationMs / 60000UL));
}

// Icon fonts have no per-glyph width getter in the C++ API, but they are
// effectively monospaced, so the max character box is the right measurement.
static void iconSize(const uint8_t* font, int &w, int &h) {
    u8g2.setFont(font);
    w = u8g2.getMaxCharWidth();
    h = u8g2.getMaxCharHeight();
}

// Travel `dist` px inside 0..range, reflecting off both ends. Shared by the cat
// and by DVD mode.
static int dvdBounce(float dist, int range) {
    if (range <= 0) return 0;
    float span = 2.0f * range;
    float p = fmodf(dist, span);
    return (int)(p <= range ? p : span - p);
}

// ---- Cat bounce ------------------------------------------------------------
// A cat ricochets around the screen, and meows when it hits a corner.

static void triggerCatBounce() { beginIconLockout(TR_CAT_BOUNCE); }

static bool catInCorner = false;   // latch, so one corner hit is one meow

static void drawCatBounce() {
    int w, h;
    iconSize(PETS_FONT, w, h);

    float s = trollElapsedMs() / 1000.0f;
    int x = dvdBounce(s * CAT_VX, SCREEN_W - w);
    int y = dvdBounce(s * CAT_VY, SCREEN_H - h);
    u8g2.drawGlyph(x, y, CAT_GLYPH);

    // A corner hit is touching a vertical AND a horizontal edge in the same
    // frame, with a pixel of slack because integer positions rarely land
    // exactly on the wall.
    bool corner = (x <= 1 || x >= SCREEN_W - w - 1) && (y <= 1 || y >= SCREEN_H - h - 1);
    if (corner && !catInCorner) playSound(SND_MEOW, GAIN_ANIMAL);
    catInCorner = corner;
}

// ---- Turtle walk -----------------------------------------------------------
// One crossing of the bottom, however long that takes. The walk is tied to
// trollProgress(), so it crosses exactly once over the event's whole life: roll
// an hour and it moves about two pixels a minute, which is the joke.

static void triggerTurtleWalk() { beginIconLockout(TR_TURTLE_WALK); }

static void drawTurtleWalk() {
    int w, h;
    iconSize(PETS_FONT, w, h);

    // Right to left, because the glyph faces left -- the other way would have
    // it moonwalking across the screen.
    int x = lerpI(SCREEN_W, -w, trollProgress());
    int lift = (animQuantize(700) / 700) % 2;   // a 1px plod: effort, not vibration
    u8g2.drawGlyph(x, SCREEN_H - h - lift, TURTLE_GLYPH);
}

// ---- Cool emoji ------------------------------------------------------------
// One emoji, dead centre, nothing else.

static void triggerEmailIcon() { beginIconLockout(TR_EMAIL_ICON); }

static void drawEmailIcon() {
    int w, h;
    iconSize(EMAIL_FONT, w, h);
    u8g2.drawGlyph((SCREEN_W - w) / 2, (SCREEN_H - h) / 2, EMAIL_GLYPH);
}

// ============================================================================
//  ANIMALS ON THE CLOCK
//  Two dismissible minors that park an animal on part of the clock face.
// ============================================================================

// ---- Cat clock -------------------------------------------------------------
// The time is replaced by the cat, which sits where the digits were and meows
// now and then. The date and temperature carry on as normal underneath.

static void triggerCatClock() {
    clockElemOffY[CE_TIME] = -200;   // resetClockOffsets() puts it back
}

static void drawCatClock() {
    int w, h;
    iconSize(PETS_FONT, w, h);
    u8g2.drawGlyph((SCREEN_W - w) / 2, clockBandTopFor(h), CAT_GLYPH);

    if (random(0, ANIMAL_NOISE_ODDS) == 0) playSound(SND_MEOW, GAIN_ANIMAL);
}

// ---- Frog ------------------------------------------------------------------
// A frog squats along the bottom, on top of where the date and temperature
// were. The clock is left alone: you can still read the time, there is just an
// amphibian in the way of everything else.

static void triggerFrogSit() {
    clockElemOffY[CE_DATE] = -200;
    clockElemOffY[CE_TEMP] = -200;
}

static void drawFrogSit() {
    int w, h;
    iconSize(PETS_FONT, w, h);
    u8g2.drawGlyph((SCREEN_W - w) / 2, SCREEN_H - h, FROG_GLYPH);

    if (random(0, ANIMAL_NOISE_ODDS) == 0) playSound(SND_RIBBIT, GAIN_ANIMAL);
}

// ============================================================================
//  CLOCK INTERFERENCE
//  Trolls that leave the clock rendering normally and just move, hide, or lie
//  about parts of it, through the hooks in clock.h.
// ============================================================================

// ---- DVD mode --------------------------------------------------------------
// The compact clock bounces around like a DVD logo. Draws its own small clock
// rather than borrowing the real one, because it needs the time at an arbitrary
// position with nothing else on screen.

static void drawDvdMode() {
    float s = trollElapsedMs() / 1000.0f;

    String t = getFormattedTime();
    u8g2.setFont(u8g2_font_6x10_tf);
    int w = u8g2.getStrWidth(t.c_str());
    int h = 10;

    int x = dvdBounce(s * DVD_VX, SCREEN_W - w);
    int y = dvdBounce(s * DVD_VY, SCREEN_H - h);
    u8g2.drawStr(x, y, t.c_str());
}

// ---- Wrong time ------------------------------------------------------------
// For two hours the clock shows a random time, re-rolled whenever the real
// minute ticks over. Nothing you can press fixes it.
//
// This is the worked example of a BACKGROUND troll. It never takes the screen,
// so the menu, the buttons and the alarm all keep working -- you can go and set
// an alarm while it runs, and because checkAlarm() reads the RTC and not the
// display, that alarm goes off at the correct REAL time even though the clock
// is lying to you. That is the whole joke, and it works for free.
//
// updateWrongTime() is the background update hook, so it draws nothing at all.

static int wrongTimeLastMinute = -1;

static void rollWrongTime() {
    // Respect the 12/24h setting, so the fake time is at least plausible.
    clockFakeHour   = twelveHourFormat ? random(1, 13) : random(0, 24);
    clockFakeMinute = random(0, 60);
    forceClockRedraw();
}

static void triggerWrongTime() {
    clockFakeTime = true;
    wrongTimeLastMinute = getCurrentTime().minute();   // do not re-roll instantly
    rollWrongTime();
}

static void updateWrongTime() {
    int m = getCurrentTime().minute();
    if (m == wrongTimeLastMinute) return;
    wrongTimeLastMinute = m;
    rollWrongTime();
}

// ---- Font cycle ------------------------------------------------------------
// Rips through the clock fonts on its own. cycleClockFont() is the same call the
// encoder makes, so this is just doing it very fast.
//
// Counting steps against elapsed time, rather than checking whether enough time
// has passed since the last frame, keeps it frame-rate independent -- a long
// frame catches up instead of dropping a font.
//
// The font it lands on sticks until you cycle it back or reboot. That is left
// in deliberately: a small souvenir.

static int fontStepsDone = 0;

static void triggerFontCycle() {
    fontStepsDone = 0;
}

static void drawFontCycle() {
    int want = trollElapsedMs() / FONT_STEP_MS;
    while (fontStepsDone < want) { cycleClockFont(1); fontStepsDone++; }
}

// ---- Slide off -------------------------------------------------------------
// The time flies up off the top, reappears from the bottom, and keeps going.
// The FIRST press freezes it wherever it happens to be -- including halfway off
// the screen, or fully gone -- and a SECOND press ends the troll. That is what
// onInput() is for; the default any-button-exits would skip the freeze.

static bool slideFrozen  = false;
static int  slideFrozenY = 0;

static void triggerSlideOff() {
    slideFrozen = false;
}

static void inputSlideOff() {
    if (!slideFrozen) {
        slideFrozen  = true;
        slideFrozenY = clockElemOffY[CE_TIME];   // stop dead, right where it is
    } else {
        exitTrollEvent();
    }
}

static void drawSlideOff() {
    if (slideFrozen) {
        clockElemOffY[CE_TIME] = slideFrozenY;
        return;
    }
    // The half-lap phase offset starts the cycle at offset 0, so the clock
    // begins in its normal spot and flies up from there rather than snapping
    // somewhere odd.
    float p = fmodf(0.5f + (float)trollElapsedMs() / SLIDE_PERIOD_MS, 1.0f);
    clockElemOffY[CE_TIME] = lerpI(SLIDE_BELOW, SLIDE_ABOVE, p);
}

// ---- Digit scroll ----------------------------------------------------------
// The digits race too fast to read a time off. Uses the fake-time hook, so the
// real clock keeps running underneath and only the display lies. Nothing to
// reset: resetClockOffsets() clears the flag when the event ends.

// A pure function of time, so there is no counter to keep and no drift.
static void scrollDigits() {
    clockFakeMinute = (millis() / DIGIT_STEP_MS) % 60;
    clockFakeHour   = (millis() / (DIGIT_STEP_MS * 60)) % 24;
}

static void triggerDigitScroll() {
    clockFakeTime = true;
    scrollDigits();   // seed it now, or the first frame shows a stale 00:00
}

static void drawDigitScroll() {
    scrollDigits();
}

// ---- Clock flees -----------------------------------------------------------
// The time slides up off the screen and stays gone. A button press does not
// just cancel the troll -- it starts the return trip, and the event ends itself
// once the digits are home. Without the onInput(), any button would exit
// instantly and the clock would snap back with no slide.

static bool          fleeReturning     = false;
static unsigned long fleeReturnStartMs = 0;

static void triggerClockFlees() {
    fleeReturning = false;
}

static void inputClockFlees() {
    if (fleeReturning) return;   // already on its way back
    fleeReturning     = true;
    fleeReturnStartMs = trollElapsedMs();
}

static void drawClockFlees() {
    unsigned long e = trollElapsedMs();

    if (!fleeReturning) {
        // Slide away, then hold off screen indefinitely.
        float t = e >= FLEE_MS ? 1.0f : (float)e / FLEE_MS;
        clockElemOffY[CE_TIME] = lerpI(0, FLEE_Y, easeInOut(t));
        return;
    }

    unsigned long r = e - fleeReturnStartMs;
    float t = r >= FLEE_MS ? 1.0f : (float)r / FLEE_MS;
    clockElemOffY[CE_TIME] = lerpI(FLEE_Y, 0, easeInOut(t));
    if (t >= 1.0f) exitTrollEvent();   // home again
}

// ---- All adrift ------------------------------------------------------------
// Every piece of the clock wanders on its own sine wave. Amplitudes and rates
// are rolled per element per launch, so nothing ever moves in lockstep -- that
// unison is what would make it read as one sliding image rather than four
// things coming loose independently.

static float adriftAmpX[CE_COUNT],  adriftAmpY[CE_COUNT];
static float adriftRateX[CE_COUNT], adriftRateY[CE_COUNT];
static float adriftPhase[CE_COUNT];

static void triggerAllAdrift() {
    for (int i = 0; i < CE_COUNT; i++) {
        adriftAmpX[i]  = (float)random(ADRIFT_AMP_X_MIN, ADRIFT_AMP_X_MAX);
        adriftAmpY[i]  = (float)random(ADRIFT_AMP_Y_MIN, ADRIFT_AMP_Y_MAX);
        adriftRateX[i] = (float)random(ADRIFT_RATE_MIN, ADRIFT_RATE_MAX) / 100.0f;
        adriftRateY[i] = (float)random(ADRIFT_RATE_MIN, ADRIFT_RATE_MAX) / 100.0f;
        adriftPhase[i] = (float)random(0, 628) / 100.0f;   // 0..2pi, so no shared start
    }
}

static void drawAllAdrift() {
    float s = trollElapsedMs() / 1000.0f;
    for (int i = 0; i < CE_COUNT; i++) {
        clockElemOffX[i] = (int)(adriftAmpX[i] * sinf(s * adriftRateX[i]));
        clockElemOffY[i] = (int)(adriftAmpY[i] * sinf(s * adriftRateY[i] + adriftPhase[i]));
    }
}

// ---- Date swap -------------------------------------------------------------
// The date takes the big clock font and slot; the time is exiled to the date's
// corner. All the work is in drawClock()/drawDate() -- see clockSwapWithDate.

static void triggerDateSwap() {
    clockSwapWithDate = true;
}

// Deliberately empty rather than nullptr: a null draw() gets the placeholder
// name card, and this troll's whole output is the clock face underneath.
static void drawDateSwap() { }

// ---- Lose date / temp / alarm ----------------------------------------------
// An element slides off its nearest edge and then STAYS gone: end() flips the
// matching setting off, exactly as if you had turned it off in the Display
// menu, so re-enabling it is the ordinary settings screen. The event itself is
// short -- it is the consequence that lasts, not the troll.
//
// canFire() keeps each one from firing when there is nothing to take, so "Lose
// alarm" with no alarm set does not silently eat a rare major roll.
//
// end() also runs if you cancel from the dev menu. That is deliberate: the
// troll did happen, so it should still cost you.

static void slideElementAway(ClockElement el, int dx, int dy) {
    float t = easeInOut(trollProgress());
    clockElemOffX[el] = lerpI(0, dx, t);
    clockElemOffY[el] = lerpI(0, dy, t);
}

static void drawLoseDate() { slideElementAway(CE_DATE, -50, 0); }   // exits left
static void endLoseDate()  { showDate = false; saveDisplayPreferences(); }
static bool canLoseDate()  { return showDate && !isNightMode(); }

static void drawLoseTemp() { slideElementAway(CE_TEMP, 60, 0); }    // exits right
static void endLoseTemp()  { tempMode = TEMP_OFF; saveDisplayPreferences(); }
static bool canLoseTemp()  { return tempShown() && !isNightMode(); }

static void drawLoseAlarm() { slideElementAway(CE_ALARM, 0, 18); }  // exits downward

// Not just hidden: actually disarmed. "Alarm OK" vetoes it outright, since that
// switch means "trolls never touch my alarm", which has to cover stealing the
// alarm as well as silencing it. Checked here and not only in canFire(), so a
// manual dev-menu launch cannot get round it either.
static void endLoseAlarm() {
    if (trollAlarmAlwaysRings) return;
    disableAlarm();
}

// Only when an alarm is genuinely armed and idle: a snoozed or ringing alarm is
// mid-cycle, and stealing it would be a different, nastier troll.
static bool canLoseAlarm() {
    return alarmEnabled && !snoozeActive && !alarmActive && !trollAlarmAlwaysRings;
}

// ============================================================================
//  THE SIMPLE DAILIES
//  All four are the same shape: flip a clock hook in trigger(), then do nothing
//  for the rest of the day. The clock keeps time, the menu opens, the alarm
//  rings -- one detail of the world is just wrong. resetClockOffsets() handles
//  the cleanup on the way out.
// ============================================================================

// ---- Mad temp: an absurd temperature, re-rolled every real minute so it never
// settles into looking like a stuck sensor.
static int tempInsaneLastMinute = -1;

static void rollInsaneTemp() {
    clockFakeTempNow = (float)random(-1000, 1001);
    forceClockRedraw();
}

static void triggerTempInsane() {
    clockFakeTemp = true;
    tempInsaneLastMinute = getCurrentTime().minute();
    rollInsaneTemp();
}

static void updateTempInsane() {
    int m = getCurrentTime().minute();
    if (m == tempInsaneLastMinute) return;
    tempInsaneLastMinute = m;
    rollInsaneTemp();
}

// ---- Wrong date: stuck on a date with, let us say, connotations. Picked once
// for the day.
static void triggerDateWrong() {
    static const int MONTHS[] = { 12,  1,  9,  4 };
    static const int DAYS[]   = { 25,  1, 11, 20 };
    int i = (int)random(0, 4);
    clockFakeMonth = MONTHS[i];
    clockFakeDay   = DAYS[i];
    clockFakeDate  = true;
    forceClockRedraw();
}

// ---- Tiny clock: the compact readout, with the encoder disabled, so spinning
// it does nothing at all. That lock is the difference between "a small clock"
// and "a small clock you are stuck with".
static void triggerTinyClock() {
    clockSmallMode  = true;
    clockFontLocked = true;
    forceClockRedraw();
}

// ---- Upside down: the whole panel rotated, clock, menu, alarm screen and all.
// u8g2 remaps its drawing internally, so nothing else needs to know. end() puts
// it back, and it runs on a dev-menu cancel too, so this cannot get stuck.
static void triggerUpsideDown() {
    u8g2.setDisplayRotation(U8G2_R2);
    forceClockRedraw();
}

static void endUpsideDown() {
    u8g2.setDisplayRotation(U8G2_R0);
    forceClockRedraw();
}

// ============================================================================
//  WEATHER SCENES
//  Five rows, one per condition, all sharing this code and differing only by
//  the numbers in the style table. They are TROLL_WEATHER so no roll ever picks
//  them: they mirror the real forecast, or you launch one from the dev menu.
//
//  Everything here is a pure function of QUANTISED time, which is what gives
//  the deliberate stop-motion look -- the particles jump a few pixels at a time
//  instead of gliding. stepMs is the main dial for that.
//
//  There is no particle array: each drop's position comes from hashing its
//  index, so N drops cost nothing but a loop, and nothing needs resetting
//  between runs.
// ============================================================================

struct WeatherStyle {
    uint8_t  clouds;     // circles scattered across the top
    uint8_t  drops;      // particles on screen
    uint16_t fallMs;     // ms per pixel fallen -- lower is faster
    uint8_t  streak;     // particle length in px; 1 is a flake or a dot
    int8_t   slant;      // horizontal drift per 8px fallen; wind
    uint16_t stepMs;     // animation quantisation -- bigger is chunkier
    bool     lightning;
};

// Order must match TR_WX_DRIZZLE .. TR_WX_STORM. The cloud counts are high
// enough that the circles overlap into one continuous bank right across the
// width -- at these spacings there are no gaps.
static const WeatherStyle WX_STYLE[] = {
    // clouds drops fallMs streak slant stepMs lightning
    {  15,     14,    26,    2,     0,    140,  false },   // drizzle: sparse and fine
    {  17,     30,    16,    3,     0,    120,  false },   // rain:    more, faster, straight down
    {  19,     52,    10,    4,     2,    100,  false },   // showers: heavy and slanted
    {  15,     26,    46,    1,     0,    180,  false },   // snow:    slow drifting dots
    {  21,     60,     8,    5,     3,     90,  true  },   // storm:   torrential, with flashes
};
static const int WX_STYLE_COUNT = sizeof(WX_STYLE) / sizeof(WX_STYLE[0]);

// A cheap deterministic hash: the same index always gives the same value, so
// drop positions are stable without storing them anywhere.
static int wxHash(int i, int mod) {
    uint32_t h = (uint32_t)i * 2654435761u;
    h ^= h >> 13;
    return (int)(h % (uint32_t)mod);
}

// Cloud cover: a scattered row of circles across the top, each a different size
// at a slightly different height. Static and deterministic, so nothing jitters
// between frames. Drawn without outlines, so overlapping circles merge into one
// cloud mass rather than reading as a string of beads.
static void drawClouds(int count) {
    if (count < 1) count = 1;
    for (int i = 0; i < count; i++) {
        int cx = (SCREEN_W * i) / count + (SCREEN_W / 2) / count + wxHash(i + 11, 7) - 3;
        int r  = 4 + wxHash(i + 23, 6);   // 4..9
        int cy = 1 + wxHash(i + 41, 6);   // 1..6, so they sit at mixed levels
        u8g2.drawDisc(cx, cy, r);
    }
}

// Shrink the clock for the duration. Purely stylistic: a 40px readout with
// weather piled on top of it is unreadable mush, and the small centred clock
// leaves the scene room to breathe.
static void triggerWeatherScene() {
    clockSmallMode  = true;   // compact only; the encoder still picks which font
    clockCenterFull = true;   // rain covers the bottom strip anyway, so use it
    forceClockRedraw();
}

static void drawWeatherScene() {
    int s = activeEventIndex - TR_WX_DRIZZLE;
    if (s < 0 || s >= WX_STYLE_COUNT) return;
    const WeatherStyle &w = WX_STYLE[s];

    unsigned long tq = animQuantize(w.stepMs);

    // Lightning: a full-screen white frame, occasionally. Picked per 2.5s slot
    // by hash, so the flashes are irregular but repeatable rather than a coin
    // flip every frame.
    if (w.lightning) {
        uint32_t slot = tq / 2500;
        if (wxHash((int)slot, 100) < 30 && (tq % 2500) < 130) {
            u8g2.drawBox(0, 0, SCREEN_W, SCREEN_H);
            return;   // blown out; nothing else would read
        }
    }

    // Precipitation, drawn BEFORE the clouds so the drops appear to fall out
    // from behind them. Each drop has its own column and its own head start
    // down the screen.
    const int TOP = 10;   // just under the cloud bank
    for (int i = 0; i < w.drops; i++) {
        int span = SCREEN_H - TOP;
        int fall = (int)((tq / w.fallMs + wxHash(i + 7, span)) % span);
        int y    = TOP + fall;
        int x    = wxHash(i, SCREEN_W) + (w.slant * fall) / 8;
        x = ((x % SCREEN_W) + SCREEN_W) % SCREEN_W;   // wrap sideways with the wind

        if (w.streak <= 1) {
            u8g2.drawPixel(x, y);                          // snow: a single flake
            if ((i & 1) == 0) u8g2.drawPixel(x + 1, y);    // some are fatter
        } else {
            u8g2.drawLine(x, y, x + w.slant, y + w.streak);
        }
    }

    drawClouds(w.clouds);
}

// Which scene matches an open-meteo condition code.
int weatherTrollForCode(int code) {
    if (code <= 49) return -1;            // clear, cloudy or fog: nothing to show
    if (code <= 59) return TR_WX_DRIZZLE;
    if (code <= 69) return TR_WX_RAIN;
    if (code <= 79) return TR_WX_SNOW;
    if (code <= 84) return TR_WX_SHOWERS;
    if (code <= 99) return TR_WX_STORM;
    return -1;
}

// ############################################################################
//
//   FUNDAMENTALS
//
//   These are not events -- nothing here runs them. They change how some other
//   part of the clock behaves, and that part asks trollEnabled() first. All of
//   them are called from alarm.cpp.
//
//   The three that draw something do so with a BLOCKING loop, which is normally
//   forbidden. It is safe in these specific cases because the alarm has just
//   been silenced and nothing else needs servicing for a couple of seconds --
//   and updateAudio() is still pumped throughout so any playing clip does not
//   stutter. Never write a draw() like this.
//
// ############################################################################

// ---- Alarm volume / Alarm sound --------------------------------------------
// Both are rolled ONCE per ring, from triggerAlarm(). That timing matters:
// playAlarmSound() re-queues in a loop for as long as the alarm rings, so
// rolling in there would change the volume and the clip every few seconds
// instead of once when it goes off.

// Everything in data/ is fair game. Add files here as you add them to LittleFS.
static const char* const ALARM_CLIP_POOL[] = {
    SND_WAKEUP, SND_NAME, SND_BEEP, SND_BOING, SND_WHISPER,
    SND_BUBBLES, SND_BOMB, SND_MEOW, SND_RIBBIT,
};
static const int ALARM_CLIP_COUNT = sizeof(ALARM_CLIP_POOL) / sizeof(ALARM_CLIP_POOL[0]);

static float       ringGain = -1.0f;     // -1 leaves the normal volume alone
static const char* ringClip = nullptr;   // nullptr uses the normal alarm clip

void trollRollAlarmRing() {
    ringGain = -1.0f;
    ringClip = nullptr;

    if (trollEnabled(TR_ALARM_VOLUME)) {
        ringGain = (random(0, 2) == 0)
                 ? (float)random(ALARM_GAIN_QUIET_MIN, ALARM_GAIN_QUIET_MAX) / 100.0f
                 : (float)random(ALARM_GAIN_LOUD_MIN,  ALARM_GAIN_LOUD_MAX)  / 100.0f;
    }

    if (trollEnabled(TR_ALARM_SOUND))
        ringClip = ALARM_CLIP_POOL[random(0, ALARM_CLIP_COUNT)];

    if (ringGain >= 0 || ringClip)
        DEBUG_PRINTF("Alarm ring: clip=%s gain=%.2f\n",
                     ringClip ? ringClip : "(default)", ringGain);
}

float       trollAlarmGain() { return ringGain; }
const char* trollAlarmClip() { return ringClip; }

// ---- Alarm drift -----------------------------------------------------------
// Re-rolled each time the alarm is armed, so you cannot learn the offset.

static int alarmDriftMin = 0;

void trollRollAlarmDrift() {
    alarmDriftMin = trollEnabled(TR_ALARM_DRIFT)
                  ? (int)random(-ALARM_DRIFT_MAX_MIN, ALARM_DRIFT_MAX_MIN + 1) : 0;
    if (alarmDriftMin) DEBUG_PRINTF("Alarm drift: %+d min\n", alarmDriftMin);
}

// Guarded on the toggle as well as the stored value, so switching the troll off
// mid-day takes effect immediately rather than waiting for the next arming.
int trollAlarmDrift() {
    return trollEnabled(TR_ALARM_DRIFT) ? alarmDriftMin : 0;
}

// ---- Snooze wheel ----------------------------------------------------------
// A slot machine decides how long your snooze actually is.

static char wheelBuf[4];
static const char* snoozeWheelLabel(int i) {
    snprintf(wheelBuf, sizeof(wheelBuf), "%d", i + 1);
    return wheelBuf;
}

int trollRollSnoozeMinutes() {
    if (!trollEnabled(TR_SNOOZE_WHEEL)) return snoozeDuration;

    int mins = (int)random(1, SNOOZE_WHEEL_MAX + 1);

    // We are called from inside snoozeAlarm(), before it has finished tearing
    // the ring down: the panel is still flashing inverted and the alarm clip is
    // still playing. Kill both, or the wheel spins over a strobing screen.
    clearAlarmInvert();
    stopSound();

    // The spin. easeOut is what makes it decelerate onto the answer rather than
    // stopping dead.
    unsigned long start = millis();
    while (millis() - start < SNOOZE_WHEEL_SPIN_MS) {
        float t   = animSince(start, SNOOZE_WHEEL_SPIN_MS);
        float pos = easeOut(t) * (SNOOZE_WHEEL_MAX * SNOOZE_WHEEL_TURNS + (mins - 1));

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        drawStrCentered(2, "SNOOZE");
        drawWheelStrip(64, 34, 44, SNOOZE_WHEEL_MAX, snoozeWheelLabel, pos);
        u8g2.sendBuffer();

        updateAudio();
        delay(20);
    }

    // Hold on the result for a beat, so you can read your sentence.
    char line[20];
    snprintf(line, sizeof(line), "%d minutes", mins);
    start = millis();
    while (millis() - start < SNOOZE_WHEEL_HOLD_MS) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        drawStrCentered(2, "SNOOZE");
        drawWheelStrip(64, 34, 44, SNOOZE_WHEEL_MAX, snoozeWheelLabel, (float)(mins - 1));
        if (blink(300)) drawStrCentered(54, line);
        u8g2.sendBuffer();
        updateAudio();
        delay(20);
    }

    // Drop any presses that piled up during the spin, so a frustrated mash does
    // not fire the instant we hand control back.
    wasSnoozePressed();
    wasAlarmPressed();
    wasBrightPressed();
    wasEncoderPressed();

    forceClockRedraw();
    DEBUG_PRINTF("Snooze wheel landed on %d minutes\n", mins);
    return mins;
}

// ---- Snooze games ----------------------------------------------------------
// You do not get a snooze, you earn one. One of two games is picked at random;
// losing means the alarm is switched OFF entirely with no snooze set, so you
// have to re-arm it by hand before you sleep again.

// Game 1: the word bounces and you press while it is inside the box.
static bool gameCatchSnooze() {
    const int BOX_X = 40, BOX_W = 48, BOX_Y = 26, BOX_H = 16;

    wasSnoozePressed();   // drop anything already latched
    u8g2.setFont(u8g2_font_6x10_tf);
    int wordW = u8g2.getStrWidth("SNOOZE");

    unsigned long start = millis();
    while (millis() - start < SNOOZE_GAME_TIMEOUT_MS) {
        // animPing bounces the word back and forth rather than wrapping, so
        // there are two passes over the box per cycle.
        int x = lerpI(0, SCREEN_W - wordW, animPing(CATCH_BOUNCE_MS));

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_5x7_tf);
        drawStrCentered(2, "CATCH THE SNOOZE");
        u8g2.drawFrame(BOX_X, BOX_Y, BOX_W, BOX_H);
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(x, BOX_Y + 4, "SNOOZE");
        u8g2.sendBuffer();
        updateAudio();

        if (wasSnoozePressed())
            return (x >= BOX_X) && (x + wordW <= BOX_X + BOX_W);

        delay(15);
    }
    return false;   // never pressed at all
}

// Game 2: three reels of scrolling SNZ / NOPE; stop each one on SNZ. Uses the
// same drawWheelStrip the snooze wheel does, so the reels actually roll past
// rather than flicking between two words.

static const int REEL_ITEMS = 6;
static bool reelIsWin(int i) { return (i % 2) == 0; }   // alternating, so even always wins
static const char* reelLabel(int i) { return reelIsWin(i) ? "SNZ" : "NOPE"; }

// Reel position for a given moment. NEGATIVE and decreasing, because
// drawWheelStrip moves items up as pos rises -- counting down scrolls the strip
// top to bottom instead. The per-reel offset keeps the three visibly out of step.
static float reelPos(unsigned long now, float rateMs, int i) {
    return -((float)now / rateMs) - i * 2.0f;
}

static bool gameSnoozeSlots() {
    const int SLOTS = 3;

    wasSnoozePressed();
    float lockedPos[SLOTS] = { 0, 0, 0 };
    int   done = 0;

    unsigned long start = millis();
    while (done < SLOTS) {
        if (millis() - start > SNOOZE_GAME_TIMEOUT_MS) return false;

        // One timestamp for the whole frame, so what you see is exactly what
        // gets checked when you press. Reading millis() twice could disagree.
        unsigned long now = millis();

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_5x7_tf);
        drawStrCentered(0, "STOP ON SNZ");

        u8g2.setFont(u8g2_font_6x10_tf);
        for (int i = 0; i < SLOTS; i++) {
            float pos = (i < done) ? lockedPos[i] : reelPos(now, REEL_RATE_MS[i], i);
            int cx = 22 + i * 42;
            drawWheelStrip(cx, 38, 38, REEL_ITEMS, reelLabel, pos);
            if (i == done) u8g2.drawFrame(cx - 21, 27, 42, 22);   // the live one
        }
        u8g2.sendBuffer();
        updateAudio();

        if (wasSnoozePressed()) {
            float pos = reelPos(now, REEL_RATE_MS[done], done);
            // drawWheelStrip centres whichever item is nearest, so ROUND rather
            // than floor to get the one actually in the window. pos is negative,
            // so the modulo needs the fixup to stay in range.
            int idx   = (int)lroundf(pos);
            int shown = (idx % REEL_ITEMS + REEL_ITEMS) % REEL_ITEMS;
            if (!reelIsWin(shown)) return false;   // stopped on NOPE

            lockedPos[done] = (float)idx;   // hold at a whole index, parking it dead centre
            done++;
        }
        delay(15);
    }
    return true;
}

// Play one of the two games and hold the verdict on screen. Returns false if
// you lost, which tells snoozeAlarm() to disarm the alarm entirely.
bool trollSnoozeChallenge() {
    if (!trollEnabled(TR_SNOOZE_GAMES)) return true;

    // Called from inside snoozeAlarm(), before it has torn the ring down.
    clearAlarmInvert();
    stopSound();

    bool won = random(0, 2) ? gameCatchSnooze() : gameSnoozeSlots();

    unsigned long start = millis();
    while (millis() - start < SNOOZE_GAME_VERDICT_MS) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        drawStrCentered(20, won ? "SNOOZE GRANTED" : "DENIED");
        u8g2.setFont(u8g2_font_5x7_tf);
        if (!won) drawStrCentered(36, "alarm is now off");
        u8g2.sendBuffer();
        updateAudio();
        delay(20);
    }

    // Mashing the button during a game should not fire something afterwards.
    wasSnoozePressed();
    wasAlarmPressed();
    wasBrightPressed();
    wasEncoderPressed();

    forceClockRedraw();
    DEBUG_PRINTF("Snooze challenge: %s\n", won ? "passed" : "failed");
    return won;
}
