// ============================================================================
//  alarm.h -- setting, checking and ringing the alarm.
//
//  The alarm is checked against the RTC, not against what the clock is
//  DISPLAYING. That distinction is deliberate: a troll can have the screen
//  showing any time it likes and the alarm still goes off at the moment you
//  actually set, which is what makes those trolls safe to leave switched on.
//
//  Three states matter. alarmEnabled is "armed for tomorrow", alarmActive is
//  "ringing right now", and snoozeActive is "silenced, and due to ring again".
//
//  Defaults live in values.h; the clip the alarm plays is at the bottom of
//  alarm.cpp.
// ============================================================================

#ifndef ALARM_H
#define ALARM_H

#include "clock_logic.h"
#include "audio.h"

void setupAlarm();   // load the saved time and armed state
void saveAlarm();

// ---- state -----------------------------------------------------------------
extern int  alarmHour;
extern int  alarmMinute;
extern int  snoozeDuration;   // minutes
extern bool alarmEnabled;
extern bool alarmActive;
extern bool snoozeActive;

// ---- setting it ------------------------------------------------------------
void setAlarmHour(int hour);
void setAlarmMinute(int minute);
void toggleAlarmEnabled();
void enableAlarm();
void disableAlarm();

// ---- ringing ---------------------------------------------------------------
// checkAlarm() runs every frame and returns true on the single frame the alarm
// should start ringing -- either the scheduled minute arriving, or a snooze
// running out. The caller owns the transition into the alarm screen.
bool checkAlarm();

void triggerAlarm();   // start ringing
void snoozeAlarm();    // silence and re-arm for a few minutes
void dismissAlarm();   // stop, and disarm entirely

// ---- the sound -------------------------------------------------------------
// The clip sequence is rebuilt and looped for as long as the alarm rings; call
// this every frame from the alarm screen.
void playAlarmSound();

extern const char* alarmName;               // the name clip the alarm greets you with
void setAlarmSound(const char* filename);   // change it at runtime

#endif
