// ============================================================================
//  display.h -- the OLED panel, plus a small toolkit for animating on it.
//
//  Two unrelated things live here. The first is the panel itself: the u8g2
//  object every other file draws through, and its setup. The second is a set of
//  generic animation helpers that know nothing about clocks or trolls.
//
//  Screen geometry, in values.h, is 128x64 with the origin top-left.
//  setupDisplay() calls setFontPosTop(), so a text y is the TOP of the line and
//  not a baseline -- y=0 sits flush against the top edge. Drawing outside the
//  screen is safe; u8g2 clips for you.
// ============================================================================

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "globals.h"

extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2;

void setupDisplay();
void centerText(const char* msg);   // centred both horizontally and vertically

// ============================================================================
//  ANIMATION TOOLKIT
//
//  A draw function is called ~30 times a second and handed nothing. The obvious
//  way to animate is a counter bumped each frame -- but then the animation has
//  state you must declare, reset whenever it restarts, and keep in step with
//  the real clock, and it runs at whatever the frame rate happens to be.
//
//  Everything here exists so you never do that. Each helper turns TIME into a
//  number between 0 and 1, and you compute the whole picture from it:
//
//      float t = trollProgress();     // 0.0 at the start, 1.0 at the end
//      int x = lerpI(-16, 140, t);    // so: off the left edge, to off the right
//      u8g2.drawEllipse(x, 34, 7, 4);
//
//  No variables, nothing to reset, correct at any frame rate, and you can
//  reason about frame 900 without having simulated the 899 before it.
//
//  Two rules when writing a draw function:
//    - Set your font first. None of these do it for you.
//    - Never block. No delay(), no waiting on time. Draw the single frame that
//      belongs to right now, and return.
// ============================================================================

// ---- timing: turn the clock into a 0..1 number -----------------------------
// These read millis() themselves, so calling one twice in a frame gives the
// same answer both times. They are tied to absolute time rather than to when
// your animation started, so two things sharing a period stay in lockstep.

float animPhase(uint32_t periodMs);                     // sawtooth: 0->1, snaps back, repeats
float animPing(uint32_t periodMs);                      // triangle: 0->1->0, repeats
float animSince(unsigned long startMs, uint32_t durMs); // one-shot: 0->1, then holds at 1
bool  blink(uint32_t periodMs);                         // true for the first half of each period

// Snap millis() to a stepMs grid. Feed this into your position maths instead of
// millis() and the movement jumps rather than glides -- the cheap way to get a
// deliberate stop-motion look at any real frame rate. Bigger step, chunkier.
unsigned long animQuantize(uint32_t stepMs);

// ---- interpolation: bend a 0..1, then turn it into pixels ------------------
// Easing takes a linear 0..1 and returns a warped one: same start, same end,
// different feel in between. Movement with easing looks physical; movement
// without it looks like a spreadsheet.

float easeInOut(float t);   // slow, fast, slow -- anything that starts and ends at rest
float easeOut(float t);     // fast, then decelerating hard -- anything that is thrown and lands
float lerpF(float a, float b, float t);   // a at t=0, b at t=1
int   lerpI(int a, int b, float t);       // the same, rounded, for pixel coordinates

// A fresh random offset in [-amount, amount]. Deliberately NOT time-based --
// the point is that it differs every frame. 1-2px is a nervous tremble, 4+ is a
// seizure. Gate it with blink() for a glitch that comes and goes.
int jitter(int amount);

// ---- drawing ---------------------------------------------------------------
// None of these set a font or send the buffer; both are the caller's job.

void drawStrCentered(int y, const char* s);                 // horizontally centred on row y
void drawProgressBar(int x, int y, int w, int h, float t);  // t is 0..1 fill, not a percentage
void drawMarquee(int y, const char* s, uint32_t periodMs);  // text scrolling right to left, forever
void drawSpinner(int cx, int cy, int r, float t);           // rotating dial, t = 0..1 per turn
void drawNoise(int count);                                  // `count` random pixels of static

// Slot-machine reel centred on (cx, cy), `w` px wide, with a frame round the
// centre slot. `pos` is a FLOATING index into the items: the whole part picks
// which item sits in the window, the fraction is how far it has scrolled past.
// It wraps automatically, so pos can grow without bound -- which is the trick
// for spinning. To spin four times and land on item N, animate pos from 0 to
// (count * 4 + N), through easeOut so it decelerates onto the answer.
// labelFn(i) returns the text for item i, and may reuse one static buffer.
void drawWheelStrip(int cx, int cy, int w, int count,
                    const char* (*labelFn)(int), float pos);

// Draw an XBM sprite mirrored left-to-right, so one sprite covers both
// directions of travel. Same bit layout u8g2's drawXBMP expects (LSB first,
// rows padded to whole bytes) -- make them at image2cpp with XBM output. This
// goes pixel by pixel, so it is slower than drawXBMP; fine for a small sprite,
// but pre-flip anything large in the data instead.
void drawXbmMirrored(int x, int y, int w, int h, const uint8_t* bits);

// ---- frame-based sprite animation ------------------------------------------
// A flip-book: N images played in sequence. Bytes are MSB-first, the order that
// Adafruit_GFX's drawBitmap and the Wokwi animator both produce, so exported
// frame arrays drop straight in. The frame data and the Anim definitions live
// in animations.cpp; declare new ones in animations.h.

struct Anim {
    const uint8_t* frames;      // every frame packed back to back
    const uint8_t* order;       // step -> frame index; nullptr means play 1:1
    uint8_t        w, h;        // pixel size of one frame
    uint8_t        stepCount;   // entries in `order` (or frame count, if 1:1)
    uint16_t       frameMs;     // ms per step
};

// Draw the frame belonging to `sinceMs` at (x, y) -- a pure function of elapsed
// time, like everything else here. loop=false holds the final frame once the
// sequence has played out.
//
//     drawAnim(ANIM_EYE_BLINK, 32, 0, trollElapsedMs(), true);
//
void     drawAnim(const Anim& a, int x, int y, unsigned long sinceMs, bool loop);
uint32_t animLengthMs(const Anim& a);   // how long one full pass takes

#endif
