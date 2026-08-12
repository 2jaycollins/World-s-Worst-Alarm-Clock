#include "bluetooth.h"
#include "BluetoothA2DPSink.h"

#include "display.h"
#include "clock_logic.h"   // getFormattedTime, disconnectWifi, reconnectWifiAsync
#include "alarm.h"         // the alarm indicator on the Bluetooth screen
#include "audio.h"         // the I2S handoff
#include "globals.h"

bool bluetoothActive = false;

static BluetoothA2DPSink a2dp;

static bool    btConnected      = false;   // a phone is linked
static bool    disconnectPrompt = false;   // the "Disconnect?" overlay is up
static bool    isPlaying        = true;    // playback state, as reported by the phone
static uint8_t btVolume         = BT_VOLUME_DEFAULT;
static char    trackTitle[48]   = "";      // latest AVRCP metadata
static char    trackArtist[48]  = "";

static unsigned long volumeChangedAt = 0;  // when the volume bar was last summoned

// Bottom "title - artist" marquee. Scrolls only while playing, freezes on pause.
static uint32_t scrollAccumMs = 0;         // accumulated playing time driving the scroll
static uint32_t scrollLastMs  = 0;         // for the per-frame delta
static char     lastNowPlaying[100] = "";  // to detect a track change and restart

// ============================================================================
//  VOLUME
//  A plain A2DP sink tops out at full-scale PCM, which through this amplifier
//  is too quiet. This control lets the volume factor exceed unity and CLIPS the
//  result -- louder but gritty, the same trade the wired path makes. See
//  BT_OVERDRIVE_MAX in values.h.
// ============================================================================
class OverdriveVolumeControl : public A2DPVolumeControl {
public:
    OverdriveVolumeControl() { volumeFactor = 0x1000; is_volume_used = true; }

    void set_volume(uint8_t volume) override {
        volumeFactor = (int32_t)volume * BT_OVERDRIVE_MAX / 127;
        is_volume_used = true;
    }

    void update_audio_data(Frame* data, uint16_t frameCount) override {
        if (!data || frameCount == 0) return;
        for (int i = 0; i < frameCount; i++) {
            int32_t l = data[i].channel1;
            int32_t r = data[i].channel2;
            if (mono_downmix) l = r = (l + r) / 2;
            l = l * volumeFactor / volumeFactorMax;
            r = r * volumeFactor / volumeFactorMax;
            if (l >  32767) l =  32767; else if (l < -32768) l = -32768;   // clip: the grit
            if (r >  32767) r =  32767; else if (r < -32768) r = -32768;
            data[i].channel1 = (int16_t)l;
            data[i].channel2 = (int16_t)r;
        }
    }
};
static OverdriveVolumeControl overdriveVol;

// ============================================================================
//  A2DP CALLBACKS
//  These run in the Bluetooth task, so they only ever copy a value and return.
// ============================================================================

// A phone connected or dropped.
static void onConnectionStateChanged(esp_a2d_connection_state_t state, void *ptr) {
    btConnected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
    if (btConnected) {
        // Assume paused until the phone reports its real status, which arrives
        // just after connecting. Otherwise the marquee scrolls a title that is
        // not actually playing yet.
        isPlaying = false;
    } else {
        trackTitle[0]  = '\0';
        trackArtist[0] = '\0';
    }
    DEBUG_PRINTF("[BT] connection state %d\n", (int)state);
}

// Track metadata, one attribute at a time.
static void onAvrcMetadata(uint8_t id, const uint8_t *text) {
    if (!text) return;
    if (id == ESP_AVRC_MD_ATTR_TITLE) {
        strncpy(trackTitle, (const char*)text, sizeof(trackTitle) - 1);
        trackTitle[sizeof(trackTitle) - 1] = '\0';
    } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
        strncpy(trackArtist, (const char*)text, sizeof(trackArtist) - 1);
        trackArtist[sizeof(trackArtist) - 1] = '\0';
    }
}

// Real play/pause state from the phone. Drives both the button behaviour and
// whether the title marquee scrolls.
static void onPlayStatus(esp_avrc_playback_stat_t playback) {
    isPlaying = (playback == ESP_AVRC_PLAYBACK_PLAYING);
}

// ============================================================================
//  ENTER AND EXIT
// ============================================================================

// Free the radio and the I2S peripheral, then start advertising for a pairing.
void enterBluetoothMode() {
    if (bluetoothActive) return;
    DEBUG_PRINTLN("[BT] entering Bluetooth mode");

    disconnectWifi();    // free the 2.4GHz radio and the RAM for the BT stack
    audioReleaseI2S();   // uninstall our I2S so A2DP can own it

    btConnected      = false;
    disconnectPrompt = false;
    isPlaying        = true;
    trackTitle[0]     = '\0';
    trackArtist[0]    = '\0';
    lastNowPlaying[0] = '\0';
    scrollAccumMs     = 0;

    i2s_pin_config_t pins = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = PIN_AUDIO_BCLK,
        .ws_io_num    = PIN_AUDIO_WCLK,
        .data_out_num = PIN_AUDIO_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };
    a2dp.set_pin_config(pins);
    a2dp.set_volume_control(&overdriveVol);
    a2dp.set_on_connection_state_changed(onConnectionStateChanged);
    a2dp.set_avrc_metadata_callback(onAvrcMetadata);
    a2dp.set_avrc_rn_playstatus_callback(onPlayStatus);
    a2dp.set_volume(btVolume);
    a2dp.start(BT_DEVICE_NAME);

    bluetoothActive = true;
    DEBUG_PRINTF("[BT] ready to pair as '%s'\n", BT_DEVICE_NAME);
}

// Stop A2DP and take the I2S back.
//
// end(false) on purpose: end(true) releases the controller's memory, which
// permanently disables restarting Bluetooth for the rest of the session. With
// false the stack stays initialised (~50KB resident, which we can afford) so
// re-entering the mode works, and it still frees the I2S peripheral.
void exitBluetoothMode(bool reconnectWifi) {
    if (!bluetoothActive) return;
    DEBUG_PRINTLN("[BT] leaving Bluetooth mode");

    a2dp.end(false);
    audioReacquireI2S();
    if (reconnectWifi) reconnectWifiAsync();

    bluetoothActive  = false;
    disconnectPrompt = false;
    btConnected      = false;
}

// ============================================================================
//  INPUT
//  Left to right, matching the on-screen arrows:
//    SNOOZE  play / pause      ALARM  skip back      BRIGHT  skip forward
//  Encoder press opens "Disconnect?"; press again confirms, BRIGHT cancels.
//  Encoder turn is volume.
// ============================================================================

void handleBluetoothButtons(bool snoozeClick, bool alarmClick,
                            bool brightClick, bool encoderClick) {
    if (disconnectPrompt) {
        if (encoderClick)     exitBluetoothMode();   // the caller drops us back to the clock
        else if (brightClick) disconnectPrompt = false;
        return;
    }

    if (encoderClick) { disconnectPrompt = true; return; }

    if (snoozeClick) {
        if (isPlaying) a2dp.pause(); else a2dp.play();
        isPlaying = !isPlaying;
    }
    if (alarmClick)  a2dp.previous();
    if (brightClick) a2dp.next();
}

// One detent of the knob, clamped to the A2DP volume range.
void bluetoothEncoderRotate(int dir) {
    if (disconnectPrompt) return;   // do not change volume while confirming

    int v = (int)btVolume + dir * BT_VOLUME_STEP;
    if (v < 0)   v = 0;
    if (v > 127) v = 127;
    btVolume = (uint8_t)v;

    a2dp.set_volume(btVolume);
    volumeChangedAt = millis();   // summon the volume bar
}

// ============================================================================
//  DISPLAY
// ============================================================================

static void drawCentered(const char* s, int y) {
    u8g2.drawStr((SCREEN_W - u8g2.getStrWidth(s)) / 2, y, s);
}

// An up-arrow and label centred on cx, pointing at the physical button above
// the screen. Matches the menu's "back" indicator.
static void drawButtonHint(int cx, const char* label) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawTriangle(cx, 0, cx - 4, 4, cx + 4, 4);
    u8g2.drawStr(cx - u8g2.getStrWidth(label) / 2, 6, label);
}

// Transient volume bar, top right. Erases behind itself so it stays legible
// over whatever is already there.
static void drawVolumeReadout() {
    const int bw = 44, bh = 8, bx = SCREEN_W - 2 - bw, by = 1;
    u8g2.setDrawColor(0);
    u8g2.drawBox(bx - 2, by - 1, bw + 4, bh + 2);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(bx, by, bw, bh);
    int fill = (btVolume * (bw - 2)) / 127;
    if (fill > 0) u8g2.drawBox(bx + 1, by + 1, fill, bh - 2);
}

// "Title - Artist" along the bottom: centred if it fits, otherwise a marquee
// that scrolls while playing and freezes on pause. Two copies a fixed gap apart
// are what make the wrap seamless.
static void drawNowPlaying(int y) {
    char nowPlaying[100];
    if (trackArtist[0])
        snprintf(nowPlaying, sizeof(nowPlaying), "%s - %s", trackTitle, trackArtist);
    else
        snprintf(nowPlaying, sizeof(nowPlaying), "%s", trackTitle);

    if (strcmp(nowPlaying, lastNowPlaying) != 0) {   // new track: restart the scroll
        strncpy(lastNowPlaying, nowPlaying, sizeof(lastNowPlaying) - 1);
        lastNowPlaying[sizeof(lastNowPlaying) - 1] = '\0';
        scrollAccumMs = 0;
    }

    int tw = u8g2.getStrWidth(nowPlaying);
    if (tw <= SCREEN_W) { drawCentered(nowPlaying, y); return; }

    uint32_t now = millis();
    uint32_t dt  = now - scrollLastMs;
    scrollLastMs = now;
    if (dt > 250) dt = 0;                    // ignore big gaps: just entered, or stalled
    if (isPlaying) scrollAccumMs += dt;

    int period = tw + BT_SCROLL_GAP_PX;
    int off = (int)((scrollAccumMs * BT_SCROLL_PX_SEC) / 1000) % period;
    u8g2.drawStr(-off, y, nowPlaying);
    u8g2.drawStr(period - off, y, nowPlaying);
}

// The Bluetooth screen: the time, plus either the pairing prompt, the current
// track, or the disconnect confirmation.
void drawBluetoothScreen() {
    u8g2.clearBuffer();

    if (disconnectPrompt) {
        drawButtonHint(112, "back");
        u8g2.setFont(u8g2_font_6x10_tf);
        drawCentered("Disconnect?", 32);
        u8g2.setFont(u8g2_font_5x7_tf);
        drawCentered("press = yes", 48);
        u8g2.sendBuffer();
        return;
    }

    u8g2.setFont(u8g2_font_spleen16x32_mn);
    drawCentered(getFormattedTime().c_str(), 12);

    u8g2.setFont(u8g2_font_5x7_tf);
    if (alarmEnabled) u8g2.drawStr(2, 2, "A");

    if (!btConnected) {
        char prompt[48];
        snprintf(prompt, sizeof(prompt), "pair '%s'", BT_DEVICE_NAME);
        drawCentered(prompt, 54);
        drawButtonHint(18,  "play");
        drawButtonHint(64,  "prev");
        drawButtonHint(110, "next");
    } else if (trackTitle[0]) {
        drawNowPlaying(54);
    } else {
        drawCentered("connected", 54);
    }

    // The volume bar pops up for a moment after you turn the knob.
    if ((unsigned long)(millis() - volumeChangedAt) < BT_VOLUME_SHOW_MS) drawVolumeReadout();

    u8g2.sendBuffer();
}
