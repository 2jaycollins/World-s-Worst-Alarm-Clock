#include "encoder.h"

static volatile bool encoderPressed = false;
static volatile int  encoderDelta   = 0;
static uint8_t       prevState      = 0;   // last (clk << 1) | dt reading

// Quadrature decode table: [previous state][new state] -> direction. The zeroes
// are the impossible transitions (both pins changing at once), which is how
// electrical noise gets rejected -- it simply produces no movement.
static const int8_t stateTable[4][4] = {
    { 0, -1,  1,  0},
    { 1,  0,  0, -1},
    {-1,  0,  0,  1},
    { 0,  1, -1,  0}
};

// Fires on every edge of either quadrature pin; accumulates signed transitions.
static void IRAM_ATTR encoderRotationISR() {
    uint8_t clk = digitalRead(PIN_ENCODER_CLK);
    uint8_t dt  = digitalRead(PIN_ENCODER_DT);
    uint8_t newState = (clk << 1) | dt;

    if (newState == prevState) return;
    encoderDelta += stateTable[prevState][newState];
    prevState = newState;
}

// Fires on both edges of the push switch. Tracking the debounced logical state
// is what guarantees exactly one press per physical press: without it, bounce on
// RELEASE (after a hold longer than the debounce window) sneaks in a second
// press that latches and fires later, reopening a menu you just closed.
static void IRAM_ATTR encoderButtonISR() {
    static unsigned long lastChange = 0;
    static bool          isDown     = false;

    unsigned long now = millis();
    if (now - lastChange < ENCODER_BTN_DEBOUNCE_MS) return;
    lastChange = now;

    bool down = (digitalRead(PIN_ENCODER_BUTTON) == LOW);
    if (down == isDown) return;
    isDown = down;

    // Ignore a press that arrives mid-rotation -- that is the knob being pushed
    // sideways while turning, not a deliberate click.
    if (down && encoderDelta == 0) encoderPressed = true;
}

// Seed the quadrature state from the pins as they sit, then arm all three
// interrupts on both edges.
void setupEncoder() {
    pinMode(PIN_ENCODER_CLK,    INPUT_PULLUP);
    pinMode(PIN_ENCODER_DT,     INPUT_PULLUP);
    pinMode(PIN_ENCODER_BUTTON, INPUT_PULLUP);

    prevState = (digitalRead(PIN_ENCODER_CLK) << 1) | digitalRead(PIN_ENCODER_DT);

    attachInterrupt(PIN_ENCODER_CLK,    encoderRotationISR, CHANGE);
    attachInterrupt(PIN_ENCODER_DT,     encoderRotationISR, CHANGE);
    attachInterrupt(PIN_ENCODER_BUTTON, encoderButtonISR,   CHANGE);

    DEBUG_PRINTLN("Encoder initialized");
}

// Hand over the accumulated transitions and reset the count.
int getEncoderDelta() {
    int delta = encoderDelta;
    encoderDelta = 0;
    return delta;
}

// Read and clear the push-switch latch.
bool wasEncoderPressed() {
    if (!encoderPressed) return false;
    encoderPressed = false;
    return true;
}
