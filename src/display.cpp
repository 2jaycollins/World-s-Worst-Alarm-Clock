#include "display.h"

U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RESET);

// Bring up the panel and put a splash on it. setFontPosTop() is what makes
// every text y in this project the top of the line rather than a baseline.
void setupDisplay() {
    u8g2.begin();
    u8g2.setFontPosTop();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.clearBuffer();
    centerText("Loading...");
    u8g2.sendBuffer();
    DEBUG_PRINTLN("Display initialized");
}

// Text centred on both axes, for splash screens and placeholder cards.
void centerText(const char* msg) {
    int x = (SCREEN_W - u8g2.getStrWidth(msg)) / 2;
    int y = (SCREEN_H - u8g2.getMaxCharHeight()) / 2;
    u8g2.drawStr(x, y, msg);
}

// ============================================================================
//  TIMING
//  See the header for what these are for and how to combine them.
// ============================================================================

// Sawtooth 0->1 over periodMs, then snaps back. For anything that travels one
// way and restarts: a scrolling banner, a rotating dial, a repeating march.
float animPhase(uint32_t periodMs) {
    if (periodMs == 0) return 0.0f;
    return (float)(millis() % periodMs) / (float)periodMs;
}

// Triangle 0->1->0 over periodMs. For anything that oscillates: a bobbing
// sprite, a pulsing box, something sliding out and back.
float animPing(uint32_t periodMs) {
    float t = animPhase(periodMs);
    return t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
}

// One-shot 0->1 over durMs from startMs, then holds at 1.0 forever. durMs = 0
// returns 1.0, which is why an endless troll reports itself as always done.
float animSince(unsigned long startMs, uint32_t durMs) {
    if (durMs == 0) return 1.0f;
    unsigned long elapsed = millis() - startMs;
    if (elapsed >= durMs) return 1.0f;
    return (float)elapsed / (float)durMs;
}

// True for the first half of each period -- flashing without tracking a toggle.
// Below ~150ms it reads as a glitch rather than a blink.
bool blink(uint32_t periodMs) {
    return animPhase(periodMs) < 0.5f;
}

// millis() snapped down to a stepMs grid, for a deliberate low-framerate look.
unsigned long animQuantize(uint32_t stepMs) {
    if (stepMs == 0) return millis();
    return (millis() / stepMs) * stepMs;
}

// ============================================================================
//  INTERPOLATION
// ============================================================================

// Slow-fast-slow. The all-purpose easing, for anything that starts from rest
// and comes to rest.
float easeInOut(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

// Fast, then decelerating hard into the finish. Spends most of its time near
// the end, so the landing reads clearly.
float easeOut(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - powf(1.0f - t, 3.0f);
}

// How far between a and b you are at t. lerpI rounds rather than truncating, so
// slow movement does not stutter.
float lerpF(float a, float b, float t) { return a + (b - a) * t; }
int   lerpI(int a, int b, float t)     { return (int)lroundf(a + (b - a) * t); }

// Random offset in [-amount, amount], re-rolled on every call.
int jitter(int amount) {
    if (amount <= 0) return 0;
    return (int)random(-amount, amount + 1);
}

// ============================================================================
//  DRAWING
// ============================================================================

// Horizontally centred text, saving the getStrWidth dance every time.
void drawStrCentered(int y, const char* s) {
    u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(s)) / 2, y, s);
}

// Outlined bar with a fill, t = 0..1. The 2px inset keeps the fill off the
// frame. For a cruel fake-loading bar, feed it something other than linear t.
void drawProgressBar(int x, int y, int w, int h, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    u8g2.drawFrame(x, y, w, h);
    int fill = (int)(t * (w - 4));
    if (fill > 0) u8g2.drawBox(x + 2, y + 2, fill, h - 4);
}

// Text scrolling right to left, looping on its own -- one call is the whole
// ticker. Travel is the screen plus the string width, so it exits before it
// reappears.
void drawMarquee(int y, const char* s, uint32_t periodMs) {
    int w = u8g2.getStrWidth(s);
    int x = SCREEN_W - (int)(animPhase(periodMs) * (SCREEN_W + w));
    u8g2.drawStr(x, y, s);
}

// A circle with a hand sweeping round it. t is not clamped, so values above 1
// just keep going round: pass animPhase(1000) to spin once a second.
void drawSpinner(int cx, int cy, int r, float t) {
    float a = t * 2.0f * PI;
    u8g2.drawCircle(cx, cy, r);
    u8g2.drawLine(cx, cy, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r));
}

// Random pixels scattered over the whole screen, re-rolled every frame. Roughly
// 20-50 is dust, 200 is heavy interference, 1000+ is unreadable.
void drawNoise(int count) {
    for (int i = 0; i < count; i++)
        u8g2.drawPixel(random(0, SCREEN_W), random(0, SCREEN_H));
}

// Slot-machine reel. Five rows are drawn around the centre so the strip appears
// to run past, clipped to the window so rows leaving it are cut off cleanly
// rather than scribbling over the rest of the screen.
void drawWheelStrip(int cx, int cy, int w, int count,
                    const char* (*labelFn)(int), float pos) {
    if (count <= 0 || labelFn == nullptr) return;

    const int rowH = 14;
    const int halfWindow = 24;          // how far above/below centre stays visible
    int   base = (int)floorf(pos);
    float frac = pos - base;

    u8g2.setClipWindow(cx - w / 2, cy - halfWindow, cx + w / 2, cy + halfWindow);
    for (int i = -2; i <= 2; i++) {
        int idx = ((base + i) % count + count) % count;
        int y = cy + (int)((i - frac) * rowH);
        const char* s = labelFn(idx);
        u8g2.drawStr(cx - u8g2.getStrWidth(s) / 2, y - 5, s);
    }
    u8g2.setMaxClipWindow();

    u8g2.drawFrame(cx - w / 2, cy - rowH / 2 - 2, w, rowH + 4);   // the "you get this one" window
}

// XBM sprite flipped left to right: (w-1-px) is the whole mirror. Reads through
// pgm_read_byte, so the source array must be PROGMEM.
void drawXbmMirrored(int x, int y, int w, int h, const uint8_t* bits) {
    int bytesPerRow = (w + 7) / 8;
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            uint8_t b = pgm_read_byte(&bits[py * bytesPerRow + (px >> 3)]);
            if (b & (1 << (px & 7)))
                u8g2.drawPixel(x + (w - 1 - px), y + py);
        }
    }
}

// ============================================================================
//  SPRITE ANIMATION
// ============================================================================

// How long one full pass of an animation takes.
uint32_t animLengthMs(const Anim& a) {
    return (uint32_t)a.stepCount * a.frameMs;
}

// Draw the frame that belongs to `sinceMs`. The step comes from elapsed time
// rather than a counter, so it stays correct if a frame runs long or the
// animation restarts.
void drawAnim(const Anim& a, int x, int y, unsigned long sinceMs, bool loop) {
    if (a.stepCount == 0 || a.frameMs == 0) return;

    uint32_t step = sinceMs / a.frameMs;
    if (step >= a.stepCount)
        step = loop ? (step % a.stepCount) : (a.stepCount - 1);

    // The order table is what makes deduping free: several steps can point at
    // the same stored image. Without one, step N is simply frame N.
    uint8_t idx = a.order ? pgm_read_byte(&a.order[step]) : (uint8_t)step;

    // drawBitmap wants BYTES per row, not pixels -- passing the width here is
    // the classic way to get a sliver of the sprite and a lot of confusion.
    int bytesPerRow = (a.w + 7) / 8;
    u8g2.drawBitmap(x, y, bytesPerRow, a.h,
                    a.frames + (uint32_t)idx * bytesPerRow * a.h);
}
