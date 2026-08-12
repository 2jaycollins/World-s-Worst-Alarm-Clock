// ============================================================================
//  application_code.h -- the main loop, and where input goes.
//
//  Everything the clock does hangs off one state variable. Exactly one state
//  owns the screen and the buttons at a time, and the transitions between them
//  all happen in this file rather than being scattered across the modules that
//  request them -- the menu and the trolls raise flags, and the loop here turns
//  those into real state changes.
//
//  Priority, highest first: the alarm overrides everything, then Bluetooth
//  mode, then a troll event, then the menu, then the plain clock.
//
//  The frame budget is in values.h.
// ============================================================================

#ifndef APPLICATION_CODE_H
#define APPLICATION_CODE_H

#include "clock.h"
#include "alarm.h"
#include "troll_events.h"
#include "menu.h"
#include "encoder.h"
#include "buttons.h"
#include "audio.h"
#include "bluetooth.h"

// ============================================================================
//  STATES
//  currentState decides what gets drawn and who receives button presses.
//  lastState remembers where the menu was opened from, so closing it returns
//  there rather than always dropping to the clock.
// ============================================================================
#define STATE_NORMAL_CLOCK 0
#define STATE_TROLL_EVENT  1
#define STATE_MENU         2
#define STATE_ALARM        3
#define STATE_BLUETOOTH    4

extern int currentState;
extern int lastState;

void mainLoop();

#endif
