// ============================================================================
//  audio.h -- playing sounds off LittleFS.
//
//  One decoder at a time, feeding an I2S output that stays installed for the
//  life of the program. Two ways in: playSound() for one-shot effects, which
//  interrupts whatever is playing, and queueSound() to line clips up so they
//  play back to back. The decoder is chosen by file extension -- ".mp3" decodes
//  as MP3, anything else as WAV.
//
//  Sound file names, per-effect volumes, gain limits and the queue size all
//  live in values.h.
// ============================================================================

#ifndef AUDIO_H
#define AUDIO_H

#include <LittleFS.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

#include "globals.h"

void setupAudio();
void updateAudio();   // call every loop(); feeds the decoder and advances the queue

// ============================================================================
//  MUTE SWITCH
//  A global sound mode, cycled from the troll dev menu. The ALARM is
//  deliberately exempt from all three settings -- a silenced alarm clock is a
//  broken alarm clock. Everything else obeys it.
// ============================================================================
enum SoundMode {
    SOUND_ON = 0,       // everything plays
    SOUND_OFF,          // nothing plays
    SOUND_DAY,          // plays only outside the night window (see values.h)
    SOUND_MODE_COUNT    // must stay last
};

extern int  soundMode;
void        cycleSoundMode();   // advance to the next mode and save it
const char* soundModeLabel();   // short text for the menu row
bool        soundAllowed();     // may a non-alarm sound play right now?

// ============================================================================
//  PLAYBACK
//  Every gain argument is on the 0.0 - AUDIO_MAX_GAIN scale, where 1.0 is clean
//  full scale and anything above it clips (louder, but gritty). Pass -1 to use
//  the volume set in the menu.
// ============================================================================

// Play a clip NOW, interrupting anything playing and clearing the queue. For
// one-shot effects. An explicit gain applies to THIS clip only and reverts when
// it ends. Prefer WAV here -- no decode latency.
//
//     playSound(SND_BOING);            // at the current volume
//     playSound(SND_BOMB, AUDIO_MAX_GAIN);   // blast this one, then back to normal
//
void playSound(const char* filename, float gain = -1.0f);

// Append a clip to the play queue. Starts immediately if nothing is playing,
// otherwise waits its turn. Chain calls to build a sequence:
//
//     queueSound(SND_WAKEUP); queueSound(SND_NAME);
//
void queueSound(const char* filename, float gain = -1.0f);

// Queue a whole array at once -- the same as N queueSound() calls.
void playSequence(const char* const* files, int count, float gain = -1.0f);

void clearQueue();   // drop anything waiting, but let the current clip finish
void stopSound();    // stop the current clip AND clear the queue

void setAudioGain(float gain);   // set the persistent volume

bool isAudioPlaying();   // is a clip actually decoding right now?

// True if the engine is doing ANYTHING: playing, queued, or waiting out the
// gap between clips. Anything that loops a sequence must guard on this rather
// than isAudioPlaying() -- during the gap the decoder is torn down but the
// sequence is not finished, and re-queueing then cuts off the previous clip.
bool audioBusy();

// ---- Bluetooth handoff -----------------------------------------------------
// Only one driver can own the I2S peripheral at a time, so entering Bluetooth
// mode releases ours for the A2DP sink and leaving it takes the peripheral back.
void audioReleaseI2S();
void audioReacquireI2S();

#endif
