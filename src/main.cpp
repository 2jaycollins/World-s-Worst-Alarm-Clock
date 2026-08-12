// ============================================================================
//  The Worst Alarm Clock
//
//  An ESP32 alarm clock that keeps perfect time, fetches the weather, works as
//  a Bluetooth speaker -- and occasionally lies to you, hides the time, or
//  makes you win a minigame before it will let you snooze.
//
//  Where things live:
//    values.h            every tunable number, in one place
//    clock_logic.cpp     timekeeping, WiFi and weather
//    clock.cpp           the clock face, brightness and night mode
//    alarm.cpp           setting, checking and ringing the alarm
//    audio.cpp           playing sounds off LittleFS
//    menu.cpp            settings, and the hidden troll menu
//    bluetooth.cpp       A2DP speaker mode
//    troll_events.cpp    the trolls themselves
//    application_code.cpp  the main loop and all state transitions
// ============================================================================

#include "application_code.h"

// Bring up the hardware, then the software that depends on it. The order
// matters: the display comes up first so the later steps have somewhere to show
// their progress, and the clock has to know the time before the trolls can
// schedule anything against it.
void setup() {
    Serial.begin(115200);

    setupDisplay();
    loadDisplayPreferences();
    setupEncoder();
    setupClock();          // RTC, WiFi, NTP, first weather fetch
    setupButtons();
    setupAlarm();
    setupTrollEvents();
    setupAudio();

    clockDriftEnabled = trollEnabled(TR_CLOCK_DRIFT);

    DEBUG_PRINTLN("Setup complete");
    DEBUG_PRINTLN("Current time: " + getFormattedTime());

    forceClockRedraw();
    displayClockScreen();
}

void loop() {
    mainLoop();
}
