#include "audio.h"
#include <driver/i2s.h>
#include "clock_logic.h"   // prefs, getCurrentTime, isNightHour -> the quiet-hours window
#include "alarm.h"         // alarmActive -> the alarm is exempt from the mute switch

// ============================================================================
//  I2S OUTPUT
//
//  The stock AudioOutputI2S::stop() leaves the driver installed but flags it
//  stopped, so the next begin() reinstalls it -- which logs an error AND pops
//  audibly, turning a short beep into a chirp. PersistentI2S overrides stop()
//  to silence the DMA buffer and keep the peripheral running, so begin()
//  becomes a no-op after the first install. Idle output is silence.
// ============================================================================
class PersistentI2S : public AudioOutputI2S {
public:
    PersistentI2S() : AudioOutputI2S(0, EXTERNAL_I2S, AUDIO_DMA_BUFFERS) {}

    // When true, stop() leaves the DMA buffer intact instead of zeroing it. Set
    // for a NATURAL clip end so the audio still sitting in the buffer -- the
    // tail of the clip -- finishes playing rather than being wiped. Without it
    // every clip loses its last word. Left false for hard stops, where instant
    // silence is exactly what we want.
    bool keepTail = false;

    bool stop() override {
        if (!i2sOn) return false;
        if (!keepTail) i2s_zero_dma_buffer((i2s_port_t)portNo);
        return true;   // leave i2sOn true on purpose: the driver stays installed
    }

    // For the Bluetooth handoff. After the driver is uninstalled so A2DP can
    // install its own, flip i2sOn back to false so a later begin() does a full
    // reinstall instead of no-op'ing.
    void markUninstalled() { i2sOn = false; }
    int  port() const { return portNo; }
};

// gen points at whichever decoder the current clip needs. Both derive from
// AudioGenerator and share the same interface, so the rest of this file is
// format-agnostic. It is built per clip and freed when the clip ends, which
// keeps the MP3 decoder's ~25KB heap allocation off the books whenever we are
// only playing short WAV beeps.
static AudioGenerator*          gen  = nullptr;
static PersistentI2S*           out  = nullptr;
static AudioFileSourceLittleFS* file = nullptr;
static unsigned long            playStartMs = 0;

// Play queue: a small ring of clips waiting their turn.
struct QueueItem { const char* file; float gain; };
static QueueItem queue[AUDIO_QUEUE_MAX];
static int       qHead  = 0;   // next clip to play
static int       qCount = 0;   // how many are waiting

// Earliest the next queued clip may start -- see AUDIO_TAIL_DRAIN_MS.
static unsigned long nextClipDueMs = 0;

static float baseGain       = AUDIO_DEFAULT_GAIN;  // persistent volume
static bool  tempGainActive = false;               // a per-clip override is in effect

static float clampGain(float g) {
    if (g > AUDIO_MAX_GAIN) g = AUDIO_MAX_GAIN;
    if (g < 0.0f) g = 0.0f;
    return g;
}

// True if the filename ends in ".mp3", case-insensitively.
static bool isMP3(const char* filename) {
    const char* dot = strrchr(filename, '.');
    return dot && strcasecmp(dot, ".mp3") == 0;
}

// ============================================================================
//  MUTE SWITCH
//  Enforced in playSound()/queueSound(), the only two ways a clip ever starts.
//  Gating there rather than deeper down means a muted sound is never queued at
//  all, so nothing backs up waiting to blurt out when sound returns.
// ============================================================================

int soundMode = SOUND_ON;

// Read the saved mode at boot, falling back to SOUND_ON if it is out of range.
static void loadSoundMode() {
    prefs.begin("audio", true);
    soundMode = prefs.getInt("mode", SOUND_ON);
    prefs.end();
    if (soundMode < 0 || soundMode >= SOUND_MODE_COUNT) soundMode = SOUND_ON;
}

// Advance to the next mode and persist it.
void cycleSoundMode() {
    soundMode = (soundMode + 1) % SOUND_MODE_COUNT;
    prefs.begin("audio", false);
    prefs.putInt("mode", soundMode);
    prefs.end();
    DEBUG_PRINTF("Sound mode: %s\n", soundModeLabel());
}

// Short label for the menu row. The daytime one is built from the night-mode
// constants rather than written out, so moving the window cannot leave it lying.
const char* soundModeLabel() {
    static char dayLabel[12];
    switch (soundMode) {
        case SOUND_OFF: return "Off";
        case SOUND_DAY:
            snprintf(dayLabel, sizeof(dayLabel), "%da-%dp",
                     WAKE_UP_HOUR, NIGHT_MODE_HOUR - 12);
            return dayLabel;
        default:        return "On";
    }
}

// May a non-alarm sound play right now? The alarm always rings regardless.
bool soundAllowed() {
    if (alarmActive) return true;
    switch (soundMode) {
        case SOUND_OFF: return false;
        case SOUND_DAY: return !isNightHour(getCurrentTime().hour());
        default:        return true;
    }
}

// ============================================================================
//  ENGINE
// ============================================================================

// Mount the filesystem and install the I2S driver, once, for good.
void setupAudio() {
    loadSoundMode();

    if (!LittleFS.begin()) {
        DEBUG_PRINTLN("Audio: LittleFS mount FAILED (did you upload the filesystem image?)");
        return;
    }

    out = new PersistentI2S();
    out->SetPinout(PIN_AUDIO_BCLK, PIN_AUDIO_WCLK, PIN_AUDIO_DOUT);
    out->SetOutputModeMono(true);   // one speaker, mono content
    out->SetGain(baseGain);
    out->begin();

    DEBUG_PRINTLN("Audio OK (persistent I2S)");
}

// Stop and free ONLY the current clip, leaving the queue alone, so the queue
// machinery can tear down a finished clip before starting the next. keepTail
// preserves the DMA buffer so a naturally-finished clip plays out.
static void stopCurrent(bool keepTail = false) {
    if (gen && gen->isRunning()) {
        if (out) out->keepTail = keepTail;
        gen->stop();
        if (out) out->keepTail = false;
    }
    if (gen)  { delete gen;  gen  = nullptr; }
    if (file) { delete file; file = nullptr; }

    // On a hard stop, zero the DMA even if there was no live generator, so a
    // tail still draining from a just-finished clip is silenced immediately.
    if (!keepTail && out) out->stop();

    if (tempGainActive) {                  // a one-shot override just ended
        if (out) out->SetGain(baseGain);
        tempGainActive = false;
    }
}

// Start a clip playing right now. Returns false if it could not (missing file
// or bad format), so the queue can skip past a dud.
static bool startClip(const char* filename, float gain) {
    stopCurrent();

    if (gain >= 0.0f) {
        out->SetGain(clampGain(gain));
        tempGainActive = true;
    } else {
        out->SetGain(baseGain);
    }

    file = new AudioFileSourceLittleFS(filename);
    if (!file->isOpen()) {
        DEBUG_PRINTF("Audio: cannot open %s\n", filename);
        delete file; file = nullptr;
        if (tempGainActive) { out->SetGain(baseGain); tempGainActive = false; }
        return false;
    }

    gen = isMP3(filename) ? (AudioGenerator*)new AudioGeneratorMP3()
                          : (AudioGenerator*)new AudioGeneratorWAV();

    if (!gen->begin(file, out)) {
        DEBUG_PRINTF("Audio: failed to play %s (bad format or decoder init failed)\n", filename);
        delete gen;  gen  = nullptr;
        delete file; file = nullptr;
        if (tempGainActive) { out->SetGain(baseGain); tempGainActive = false; }
        return false;
    }

    playStartMs = millis();
    DEBUG_PRINTF("Playing: %s%s\n", filename, tempGainActive ? " (custom gain)" : "");
    return true;
}

// Pull the next queued clip and play it, skipping any that fail to start.
static void playNextInQueue() {
    while (qCount > 0) {
        QueueItem it = queue[qHead];
        qHead = (qHead + 1) % AUDIO_QUEUE_MAX;
        qCount--;
        if (startClip(it.file, it.gain)) return;
    }
}

void clearQueue() {
    qHead  = 0;
    qCount = 0;
}

// Public stop means stop everything, queue included.
void stopSound() {
    clearQueue();
    stopCurrent();
}

// One-shot: overrides anything playing or waiting.
void playSound(const char* filename, float gain) {
    if (!out) {
        DEBUG_PRINTLN("Audio: playSound called before setupAudio()");
        return;
    }
    if (!soundAllowed()) return;
    clearQueue();
    startClip(filename, gain);
}

// Append to the queue, or start immediately if the engine is idle.
void queueSound(const char* filename, float gain) {
    if (!out) {
        DEBUG_PRINTLN("Audio: queueSound called before setupAudio()");
        return;
    }
    if (!soundAllowed()) return;

    if (!isAudioPlaying() && qCount == 0) {
        startClip(filename, gain);
        return;
    }
    if (qCount >= AUDIO_QUEUE_MAX) {
        DEBUG_PRINTF("Audio: queue full (%d), dropping %s\n", AUDIO_QUEUE_MAX, filename);
        return;
    }
    queue[(qHead + qCount) % AUDIO_QUEUE_MAX] = { filename, gain };
    qCount++;
}

void playSequence(const char* const* files, int count, float gain) {
    for (int i = 0; i < count; i++) queueSound(files[i], gain);
}

bool isAudioPlaying() {
    return gen && gen->isRunning();
}

// Playing, queued, or mid tail-drain. See the header for why loops must use
// this rather than isAudioPlaying().
bool audioBusy() {
    return (gen && gen->isRunning())
        || qCount > 0
        || (long)(millis() - nextClipDueMs) < 0;
}

// Set the persistent volume, without stomping an active one-shot override.
void setAudioGain(float gain) {
    baseGain = clampGain(gain);
    if (out && !tempGainActive) out->SetGain(baseGain);
}

// Feed the decoder, and start the next queued clip once the previous one's tail
// has drained out of the DMA buffer.
void updateAudio() {
    if (gen && gen->isRunning()) {
        if (!gen->loop()) {                             // reached end of file
            DEBUG_PRINTF("Audio: clip ended after %lu ms\n", millis() - playStartMs);
            stopCurrent(true);                          // keep the tail; let it drain
            nextClipDueMs = millis() + AUDIO_TAIL_DRAIN_MS;
        }
    } else if (qCount > 0 && (long)(millis() - nextClipDueMs) >= 0) {
        playNextInQueue();
    }
}

// ============================================================================
//  BLUETOOTH HANDOFF
//  The A2DP sink wants the same I2S port and pins we use. Only one driver can
//  be installed at a time, so entering BT mode releases ours and leaving it
//  takes the peripheral back.
// ============================================================================

// Stop everything and free the I2S peripheral for the A2DP sink.
void audioReleaseI2S() {
    stopSound();
    if (out) {
        i2s_driver_uninstall((i2s_port_t)out->port());
        out->markUninstalled();   // so the reacquire below actually reinstalls
        DEBUG_PRINTLN("Audio: released I2S for Bluetooth");
    }
}

// Reinstall our driver and pinout so the alarm and beeps work again.
void audioReacquireI2S() {
    if (out) {
        out->begin();
        out->SetGain(baseGain);
        DEBUG_PRINTLN("Audio: reacquired I2S");
    }
}
