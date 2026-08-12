#include "buttons.h"

// One latch per button, set by its ISR and cleared when read.
static volatile bool snoozePressed = false;
static volatile bool alarmPressed  = false;
static volatile bool brightPressed = false;

// A bouncing contact fires several edges within a millisecond or two. Ignoring
// everything within BTN_DEBOUNCE_MS of the last accepted edge collapses them
// into one press. Each button keeps its own `last`, passed in by reference.
static bool IRAM_ATTR debounced(unsigned long &last) {
    unsigned long now = millis();
    if (now - last <= BTN_DEBOUNCE_MS) return false;
    last = now;
    return true;
}

static void IRAM_ATTR snoozeISR() {
    static unsigned long last = 0;
    if (debounced(last)) snoozePressed = true;
}

static void IRAM_ATTR alarmISR() {
    static unsigned long last = 0;
    if (debounced(last)) alarmPressed = true;
}

static void IRAM_ATTR brightISR() {
    static unsigned long last = 0;
    if (debounced(last)) brightPressed = true;
}

// Arm all three buttons as falling-edge interrupts with internal pull-ups.
void setupButtons() {
    pinMode(PIN_BTN_ALARM,  INPUT_PULLUP);
    pinMode(PIN_BTN_SNOOZE, INPUT_PULLUP);
    pinMode(PIN_BTN_BRIGHT, INPUT_PULLUP);
    attachInterrupt(PIN_BTN_ALARM,  alarmISR,  FALLING);
    attachInterrupt(PIN_BTN_SNOOZE, snoozeISR, FALLING);
    attachInterrupt(PIN_BTN_BRIGHT, brightISR, FALLING);
    DEBUG_PRINTLN("Buttons initialized with interrupts");
}

// Read and clear a latch. Returns true only for the frame a press arrived in.
static bool consume(volatile bool &flag) {
    if (!flag) return false;
    flag = false;
    return true;
}

bool wasSnoozePressed() { return consume(snoozePressed); }
bool wasAlarmPressed()  { return consume(alarmPressed); }
bool wasBrightPressed() { return consume(brightPressed); }
