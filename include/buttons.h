// ============================================================================
//  buttons.h -- the three front buttons.
//
//  Each button is an interrupt that sets a latch. The latch is read once per
//  frame and cleared by reading it, so a press is never seen twice and never
//  missed between frames. Pins and the debounce window live in values.h.
//
//  Wiring: all three need 10k pull-ups and switch to ground.
// ============================================================================

#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include "globals.h"

void setupButtons();

// ---- press latches ---------------------------------------------------------
// True once per press, then false again until the next one. Reading consumes
// the press, so call each of these exactly once per frame:
//
//     if (wasAlarmPressed()) handleAlarmClick();
//
bool wasAlarmPressed();
bool wasSnoozePressed();
bool wasBrightPressed();

#endif
