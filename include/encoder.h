// ============================================================================
//  encoder.h -- the rotary encoder and its push switch.
//
//  Rotation is decoded in an interrupt into a running signed delta; the push
//  switch latches like the front buttons do. Both are drained once per frame.
//
//  The knob emits ENCODER_STEPS_PER_DETENT transitions per physical click, so
//  callers accumulate the delta and act on whole detents rather than on every
//  transition -- see handleEncoder() in application_code.cpp.
//
//  Pins and debounce timing live in values.h.
// ============================================================================

#ifndef ENCODER_H
#define ENCODER_H

#include <ESP32Encoder.h>
#include <Arduino.h>
#include "globals.h"

void setupEncoder();

// Transitions since the last call, signed for direction. Reading clears it:
//
//     int delta = getEncoderDelta();
//     if (delta != 0) handleEncoderRotation(delta);
//
int getEncoderDelta();

// True once per press of the knob, like the front-button latches.
bool wasEncoderPressed();

#endif
