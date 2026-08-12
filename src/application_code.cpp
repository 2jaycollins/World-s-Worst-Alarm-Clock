#include "application_code.h"

int currentState = STATE_NORMAL_CLOCK;
int lastState    = STATE_NORMAL_CLOCK;

// ============================================================================
//  THE SECRET DEV-MENU UNLOCK
//  ALARM, SNOOZE, ALARM, then an encoder press. Works from any state except the
//  menu itself, and always bypasses a troll's input gates -- which is how you
//  cancel a troll that has frozen every button.
// ============================================================================

static bool devMenuArmed  = false;
static int  devCodeStep   = 0;   // 0 idle, 1 got A, 2 got A+S, 3 armed
static unsigned long devCodeLastMs = 0;

// Advance the unlock sequence. Any wrong button, or too long a pause, resets it.
static void updateDevCode(bool alarmClick, bool snoozeClick) {
    unsigned long now = millis();
    if (devCodeStep > 0 && now - devCodeLastMs > DEV_CODE_WINDOW_MS) {
        devCodeStep  = 0;
        devMenuArmed = false;
    }
    if (!alarmClick && !snoozeClick) return;

    if      (devCodeStep == 0 && alarmClick)  devCodeStep = 1;
    else if (devCodeStep == 1 && snoozeClick) devCodeStep = 2;
    else if (devCodeStep == 2 && alarmClick){ devCodeStep = 3; devMenuArmed = true; }
    else if (alarmClick)                      devCodeStep = 1;   // a stray ALARM restarts it
    else                                      devCodeStep = 0;   // anything else resets

    devCodeLastMs = now;
}

// ============================================================================
//  RENDERING
// ============================================================================

// Draw whichever state owns the screen.
static void renderCurrentState() {
    switch (currentState) {
        case STATE_NORMAL_CLOCK:
            displayClockScreen();   // only actually repaints when something changed
            break;

        case STATE_TROLL_EVENT:
            drawTrollEvent(activeEventIndex);
            break;

        case STATE_MENU:
            // Check the exit flag first, so we do not draw a stale menu frame on
            // the way out. Return to wherever the menu was opened from.
            if (exitMenuFlag) {
                exitMenuFlag = false;
                currentState = lastState;
                // We came from a troll event, but it was cancelled while we were
                // in the menu -- there is nothing to go back to.
                if (currentState == STATE_TROLL_EVENT && activeEventIndex == -1)
                    currentState = STATE_NORMAL_CLOCK;
                if (currentState == STATE_NORMAL_CLOCK) forceClockRedraw();
            } else {
                drawMenu();
            }
            break;

        case STATE_ALARM:
            displayAlarmClock();
            playAlarmSound();
            break;

        case STATE_BLUETOOTH:
            drawBluetoothScreen();
            break;
    }
}

// ============================================================================
//  INPUT
// ============================================================================

// Read every input once, then route it. Latching all four up front means the
// dev-code detector and the handlers below all see the same snapshot.
static void handleButtons() {
    bool encoderClick = wasEncoderPressed();
    bool snoozeClick  = wasSnoozePressed();
    bool alarmClick   = wasAlarmPressed();
    bool brightClick  = wasBrightPressed();

    // Any press counts as "someone is here", including ones swallowed by a gate
    // below -- pressing a dead button is still a sign of life, so this has to
    // happen before any of the early returns.
    if (encoderClick || snoozeClick || alarmClick || brightClick) noteUserInput();

    // A ringing alarm takes over all input. The other presses are simply
    // dropped, already consumed above, so a press during the ring does not fire
    // the instant we return to the clock.
    if (alarmActive) {
        if (snoozeClick)     snoozeAlarm();
        else if (alarmClick) dismissAlarm();
        else return;

        clearAlarmInvert();
        currentState = STATE_NORMAL_CLOCK;
        forceClockRedraw();
        return;
    }

    // Bluetooth mode likewise owns everything. Confirming the disconnect leaves
    // bluetoothActive false, which is how we fall back to the clock here.
    if (currentState == STATE_BLUETOOTH) {
        handleBluetoothButtons(snoozeClick, alarmClick, brightClick, encoderClick);
        if (!bluetoothActive) {
            currentState = STATE_NORMAL_CLOCK;
            forceClockRedraw();
        }
        return;
    }

    if (currentState != STATE_MENU) updateDevCode(alarmClick, snoozeClick);

    if (encoderClick) {
        if (devMenuArmed && currentState != STATE_MENU) {
            devMenuArmed = false;
            devCodeStep  = 0;
            lastState    = currentState;   // so leaving returns where we came from
            currentState = STATE_MENU;
            openDevMenu();
        } else if (currentState == STATE_NORMAL_CLOCK) {
            lastState    = currentState;
            currentState = STATE_MENU;
            openMenu();
        } else if (currentState == STATE_MENU) {
            handleMenuClick();
            // "Bluetooth" was selected. The menu only raises a flag; entering
            // the mode means leaving the menu, so we own the transition.
            if (requestedBluetooth) {
                requestedBluetooth = false;
                exitMenu();
                exitMenuFlag = false;   // we are handling this, not the menu renderer
                enterBluetoothMode();
                currentState = STATE_BLUETOOTH;
            }
        } else if (currentState == STATE_TROLL_EVENT) {
            if (!trollGates.blockEncoder) handleTrollEventClick();
        }
    }

    if (snoozeClick) {
        if (currentState == STATE_MENU) {
            handleMenuSnooze();
            // A troll was instant-launched from the dev menu. Same deal as
            // Bluetooth: leave the menu first, then start the event.
            if (requestedTrollLaunch >= 0) {
                int idx = requestedTrollLaunch;
                requestedTrollLaunch = -1;
                exitMenu();
                exitMenuFlag = false;
                // manual = true, so the troll may relax its conditions for a
                // demo. triggerEventByIndex also owns the state change, since a
                // background troll must not be given the screen.
                triggerEventByIndex(idx, true);
            }
        } else if (currentState == STATE_TROLL_EVENT) {
            if (!trollGates.blockSnoozeBtn) handleTrollEventSnooze();
        }
    }

    if (alarmClick) {
        if (currentState == STATE_NORMAL_CLOCK) {
            toggleAlarmEnabled();
            forceClockRedraw();
            playSound(SND_BEEP, GAIN_ALARM_TOGGLE);
        } else if (currentState == STATE_MENU) {
            handleMenuAlarm();
        } else if (currentState == STATE_TROLL_EVENT) {
            if (!trollGates.blockAlarmBtn) handleTrollEventAlarm();
        }
    }

    if (brightClick) {
        if (currentState == STATE_NORMAL_CLOCK) {
            cycleBrightness();
            // The boing gets louder as the screen does, but must stay under
            // AUDIO_MAX_GAIN. Night mode keeps it quiet.
            float gain = GAIN_BRIGHT_BOING;
            if (brightnessLevel > 0 && !isNightMode()) gain = (float)brightnessLevel;
            playSound(SND_BOING, gain);
        } else if (currentState == STATE_MENU) {
            // Back. handleMenuBright() either drops a level (staying in the
            // menu) or flags an exit; the actual exit happens in the renderer,
            // so we must NOT change state here.
            handleMenuBright();
        } else if (currentState == STATE_TROLL_EVENT) {
            if (!trollGates.blockBrightBtn) handleTrollEventBright();
        }
    }
}

// Accumulate encoder transitions and act once per physical detent.
static void handleEncoder() {
    static int accumulator = 0;

    int delta = getEncoderDelta();
    if (delta != 0) noteUserInput();
    accumulator += delta;

    while (accumulator >= ENCODER_STEPS_PER_DETENT) {
        accumulator -= ENCODER_STEPS_PER_DETENT;
        switch (currentState) {
            case STATE_MENU:         menuScrollDown();          break;
            case STATE_NORMAL_CLOCK: cycleClockFont(1); forceClockRedraw(); break;
            case STATE_BLUETOOTH:    bluetoothEncoderRotate(+1); break;
            default: break;   // ignored during a troll event
        }
    }
    while (accumulator <= -ENCODER_STEPS_PER_DETENT) {
        accumulator += ENCODER_STEPS_PER_DETENT;
        switch (currentState) {
            case STATE_MENU:         menuScrollUp();            break;
            case STATE_NORMAL_CLOCK: cycleClockFont(-1); forceClockRedraw(); break;
            case STATE_BLUETOOTH:    bluetoothEncoderRotate(-1); break;
            default: break;
        }
    }
}

// ============================================================================
//  THE LOOP
// ============================================================================

// One frame: read input, advance every timer, draw, then spend the rest of the
// budget on audio.
void mainLoop() {
    unsigned long frameStart = millis();

    handleButtons();
    handleEncoder();
    updateClock();
    updateIdleMode();           // days without input: dim down, stop trolling
    updateTrollTimer();         // the once-a-minute roll
    updateActiveTrollEvent();   // end the running troll when its time is up

    if (checkAlarm()) {
        // A major troll can swallow the alarm while it runs -- unless the dev
        // menu's "Alarm OK" safety switch is on, which it is by default.
        if (!trollSuppressesAlarm()) {
            // A ring during Bluetooth mode drops the link immediately and takes
            // the I2S back so the alarm can actually sound. The WiFi reconnect
            // is skipped: keep the alarm path lean.
            if (bluetoothActive) exitBluetoothMode(false);

            triggerAlarm();   // owns the brightness override

            // The alarm overrides everything. Close the menu out cleanly if it
            // is open, so it does not linger underneath.
            if (currentState == STATE_MENU) {
                exitMenu();
                exitMenuFlag = false;   // we own this transition, not the renderer
            }
            currentState = STATE_ALARM;
        }
    }

    renderCurrentState();
    updateAudio();   // service at least once, even on an over-budget frame

    // Spend whatever is left of the frame servicing audio rather than sleeping
    // through it. updateAudio() only does work when a clip is playing, so idle
    // frames still just wait -- but a playing one gets pumped at ~1ms
    // granularity instead of once per frame, which cuts playback latency and
    // keeps queued clips flowing back to back.
    while ((int)(millis() - frameStart) < TARGET_FRAME_MS) {
        updateAudio();
        delay(1);
    }
}
