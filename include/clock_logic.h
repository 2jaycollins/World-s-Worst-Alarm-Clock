// ============================================================================
//  clock_logic.h -- keeping time, and knowing the weather.
//
//  Timekeeping is the DS3231's job: it is battery-backed, so the real time
//  survives a power cut. WiFi is only used to correct it from NTP at boot and
//  to fetch the forecast; the radio is switched off in between, which keeps the
//  clock quiet and leaves the 2.4GHz band free for Bluetooth mode.
//
//  WiFi networks and locations are two separate ideas. You pick a network once
//  at startup and it is remembered; the location only drives timezone and
//  weather. That means you can sit in any location's settings without being on
//  that location's WiFi. Both tables live in locations.h.
//
//  Timeouts, retry counts and the day/night hours are all in values.h.
// ============================================================================

#ifndef CLOCK_LOGIC_H
#define CLOCK_LOGIC_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <RTClib.h>

#include "secrets.h"
#include "globals.h"
#include "locations.h"

extern RTC_DS3231 rtc;
extern Preferences prefs;

void setupClock();
void updateClock();   // call every frame: owns the day rollover, the night-mode
                      // transitions and the periodic weather refresh

// ============================================================================
//  TIME
//  getDisplayHour()/getDisplayMinute() are what the clock face shows, which is
//  not necessarily the real time: they include the drift troll's offset. The
//  alarm reads the RTC directly, which is why an alarm still goes off at the
//  correct moment while the display is lying to you.
// ============================================================================
DateTime getCurrentTime();
String   getFormattedTime();   // "HH:MM", already 12/24h adjusted
String   getFormattedDate();   // "MM/DD"
int      getDisplayHour();
int      getDisplayMinute();

extern bool twelveHourFormat;
extern bool isPM;              // set as a side effect of getDisplayHour()

// ---- clock drift (troll) ---------------------------------------------------
// The clock is quietly wrong by a few minutes, re-rolled at midnight.
extern bool clockDriftEnabled;
extern int  fakeDriftMinutes;
void rollDailyDrift();
int  getFakeDrift();

// ---- the night window ------------------------------------------------------
// Used by night mode, by the daytime-only sound setting, and to schedule the
// daily trolls. The window wraps midnight.
bool isNightHour(int hour);
void initDayTriggers(int day);   // seed the once-per-day latches at boot

// Raised at the window boundaries and consumed inside updateClock(), which
// drives enterNightMode()/exitNightMode(). triggerWakeUp is also raised by the
// alarm, so a ring pulls the display out of night mode.
extern bool triggerWakeUp;
extern bool triggerNightMode;

// ============================================================================
//  WEATHER
//  Refreshed on a timer from open-meteo, which is intermittently slow enough
//  that a fetch retries before it gives up. The last known readings stay on
//  screen through a failure; only after several in a row does the display mark
//  them stale.
// ============================================================================
bool fetchWeather();          // true only when the readings were actually updated
bool shouldRefreshWeather();

extern float todayHighTemp;
extern float tomorrowHighTemp;
extern float currentTemp;
extern int   weatherCode;         // open-meteo condition code; drives the weather scenes
extern bool  weatherUnavailable;  // true once refreshes have failed repeatedly

// ============================================================================
//  NETWORK
//  connectWifi() blocks for up to WIFI_CONNECT_TIMEOUT, so it is only ever
//  called at boot and around a weather refresh -- never while Bluetooth mode is
//  active, which shares the radio.
// ============================================================================
bool connectWifi();         // true if connected within the timeout
void disconnectWifi();
void reconnectWifiAsync();  // non-blocking re-enable, used when leaving Bluetooth mode
void syncTimeFromNTP();

extern int currentWifiIndex;      // index into WIFI_NETWORKS
bool selectWifiAtStartup();       // picker shown only when the saved network fails
void loadWifiPreference();
void saveWifiPreference(int index);

// ---- location (timezone + weather, independent of the network) -------------
extern int currentLocationIndex;  // index into LOCATIONS
void onLocationChanged(int newIndex);   // switch, persist, then resync time and weather
void loadLocationPreference();
void saveLocationPreference(int index);

#endif
