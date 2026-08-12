#include "alarm.h"
#include "clock.h"          // brightness override, so a ring is visible from night mode
#include "troll_events.h"   // the alarm fundamentals hook into the functions below

int  alarmHour      = 0;
int  alarmMinute    = 0;
int  snoozeDuration = DEFAULT_SNOOZE_MINUTES;
bool alarmEnabled   = false;
bool alarmActive    = false;
bool snoozeActive   = false;

static DateTime snoozeTarget;      // when the snooze ends and the alarm re-rings
static int lastTriggerMinute = -1; // edge guard, so we fire once per minute

// ============================================================================
//  SAVED STATE
// ============================================================================

void setupAlarm() {
    Preferences prefs;
    prefs.begin("alarm", true);
    alarmHour    = prefs.getInt("hour", DEFAULT_ALARM_HOUR);
    alarmMinute  = prefs.getInt("minute", DEFAULT_ALARM_MINUTE);
    alarmEnabled = prefs.getBool("enabled", false);
    prefs.end();
}

void saveAlarm() {
    Preferences prefs;
    prefs.begin("alarm", false);
    prefs.putInt("hour", alarmHour);
    prefs.putInt("minute", alarmMinute);
    prefs.putBool("enabled", alarmEnabled);
    prefs.end();
}

// ============================================================================
//  ARMING
// ============================================================================

void setAlarmHour(int hour) {
    alarmHour = hour % 24;
    saveAlarm();
}

void setAlarmMinute(int minute) {
    alarmMinute = minute % 60;
    saveAlarm();
}

// Arm the alarm, rolling a fresh drift offset so the troll's error cannot be
// learned from one morning to the next.
void enableAlarm() {
    alarmEnabled = true;
    trollRollAlarmDrift();
    saveAlarm();
}

void disableAlarm() {
    alarmEnabled = false;
    saveAlarm();
}

// The alarm button on the clock screen. Disarming also cancels a ring or a
// pending snooze, so one press always means "stop".
void toggleAlarmEnabled() {
    alarmEnabled = !alarmEnabled;
    if (alarmEnabled) {
        trollRollAlarmDrift();
    } else {
        alarmActive  = false;
        snoozeActive = false;
    }
    saveAlarm();
}

// ============================================================================
//  RINGING
// ============================================================================

// True on the one frame the alarm should start ringing: either a snooze has run
// out, or the scheduled minute has arrived.
bool checkAlarm() {
    if (alarmActive) return false;   // already ringing

    DateTime now = getCurrentTime();

    if (snoozeActive) {
        if (now.unixtime() < snoozeTarget.unixtime()) return false;
        snoozeActive = false;
        return true;
    }

    if (!alarmEnabled) return false;

    // Compared in minutes-of-day so the drift troll can shift the alarm across
    // an hour, or across midnight, with no special cases. trollAlarmDrift() is
    // zero unless that fundamental is switched on.
    int nowMinutes = now.hour() * 60 + now.minute();
    int target = alarmHour * 60 + alarmMinute + trollAlarmDrift();
    target = ((target % 1440) + 1440) % 1440;

    // This runs every frame, so the target minute is true for ~1800 frames.
    // lastTriggerMinute makes sure we only fire on the first of them.
    if (nowMinutes != target) {
        lastTriggerMinute = -1;   // moved off the target minute; re-arm
        return false;
    }
    if (lastTriggerMinute == now.minute()) return false;

    lastTriggerMinute = now.minute();
    return true;
}

// Start ringing. Rolls this ring's clip and volume ONCE, here, because
// playAlarmSound() re-queues in a loop and rolling there would change the sound
// every few seconds.
void triggerAlarm() {
    alarmActive  = true;
    snoozeActive = false;

    // A ring is the clock waking up, so it clears idle mode exactly as a button
    // press would. Otherwise the alarm would pull the panel out of night mode
    // but leave every troll suspended for the rest of the day.
    noteUserInput();

    trollRollAlarmRing();
    alarmBrightnessOverride();
    DEBUG_PRINTLN("Alarm triggered");
}

// Silence the ring and re-arm for a few minutes' time. With the snooze-games
// fundamental on you have to earn it first -- losing means no snooze AND the
// alarm switched off, so it must be re-armed by hand before you sleep again.
void snoozeAlarm() {
    if (!alarmActive) return;

    if (!trollSnoozeChallenge()) {
        alarmActive  = false;
        snoozeActive = false;
        disableAlarm();
        restoreBrightnessAfterAlarm();
        return;
    }

    // Normally snoozeDuration. With the snooze-wheel fundamental on, this spins
    // a slot machine and comes back with whatever it landed on instead.
    int mins = trollRollSnoozeMinutes();
    if (mins < 1) mins = 1;   // guard: zero would re-ring instantly

    snoozeTarget = getCurrentTime() + TimeSpan(0, 0, mins, 0);   // read AFTER the animation
    snoozeActive = true;
    alarmActive  = false;
    restoreBrightnessAfterAlarm();
}

// The alarm button while ringing: stop everything and disarm.
void dismissAlarm() {
    alarmActive  = false;
    snoozeActive = false;
    disableAlarm();
    restoreBrightnessAfterAlarm();
}

// ============================================================================
//  THE ALARM SOUND
//  This is what to edit to change the morning routine. Add, remove or reorder
//  the queueSound() calls below; WAV and MP3 mix freely.
// ============================================================================

// The name clip the alarm greets you with. Point it at any file in data/.
const char* alarmName = SND_NAME;

void setAlarmSound(const char* filename) { alarmName = filename; }

// Build the clip sequence, and rebuild it whenever it finishes, so the alarm
// loops until you snooze or dismiss it.
//
// Guarding on audioBusy() rather than isAudioPlaying() is essential: during the
// gap between clips the decoder is gone but the sequence is not finished, and
// re-queueing then would cut off the tail of the clip still draining.
void playAlarmSound() {
    if (!alarmActive) return;
    if (audioBusy()) return;

    // With the "Alarm sound" and "Alarm volume" fundamentals off these are
    // nullptr and -1: the normal clip, at the normal volume.
    const char* clip = trollAlarmClip();
    queueSound(clip ? clip : SND_WAKEUP, trollAlarmGain());
}
