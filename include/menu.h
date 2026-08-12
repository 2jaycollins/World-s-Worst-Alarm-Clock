// ============================================================================
//  menu.h -- the settings menu, opened by pressing the encoder.
//
//  One flat state variable picks which screen is showing. The encoder scrolls
//  and selects; the BRIGHT button is "back", and drops one level at a time
//  until it leaves the menu entirely.
//
//  The troll screens are the exception: they are only reachable through the
//  secret dev-menu unlock (ALARM, SNOOZE, ALARM, then an encoder press), which
//  works from any state and lands straight in the troll list.
//
//  Row heights and list geometry live in values.h.
// ============================================================================

#ifndef MENU_H
#define MENU_H

#include "clock_logic.h"
#include "display.h"
#include "audio.h"
#include "alarm.h"
#include "clock.h"
#include "troll_events.h"

// ============================================================================
//  MENU STATES
//  The first six are real screens. BLUETOOTH_MODE is a sentinel rather than a
//  screen -- selecting it raises a flag that the app loop turns into a real
//  mode change, because entering Bluetooth means leaving the menu entirely.
// ============================================================================
#define MAIN_MENU       0
#define SET_ALARM       1
#define SET_VOLUME      2
#define SET_DISPLAY     3
#define LOCATION_MENU   4
#define TROLL_MENU      5   // categories + a link to the global settings
#define BLUETOOTH_MODE  6   // sentinel, not a screen
#define TROLL_LIST      7   // the trolls of one category
#define TROLL_SETTINGS  8   // the four global troll switches

// ============================================================================
//  LIFECYCLE
// ============================================================================
void openMenu();
void openDevMenu();   // straight to the troll list, with devMode on
void exitMenu();
void drawMenu();      // draw whichever screen is current

extern bool devMode;       // the troll screens are visible and reachable
extern bool exitMenuFlag;  // set by exitMenu(); the app loop consumes it

// ============================================================================
//  REQUESTS OUT TO THE APP LOOP
//  Two actions cannot be performed from inside the menu, because they mean
//  leaving it: entering Bluetooth mode and launching a troll. The menu records
//  the request here and the app loop owns the actual transition.
// ============================================================================
extern int  requestedTrollLaunch;   // index into trollEvents[], or -1 for nothing
extern bool requestedBluetooth;

// ============================================================================
//  INPUT
//  One entry point per control, called by the app loop when the menu has focus.
// ============================================================================
void handleMenuClick();    // encoder press: select / toggle / commit
void handleMenuBright();   // BRIGHT: back one level, or out of the menu
void handleMenuSnooze();   // SNOOZE: launch or cancel a troll, on the troll screens
void handleMenuAlarm();    // ALARM: unused, reserved
void menuScrollDown();
void menuScrollUp();
int  getMenuOptionCount();

#endif
