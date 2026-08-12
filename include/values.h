// ============================================================================
//  values.h -- every tunable number in the clock, in one place.
//
//  This is the file to open when you want to change how the clock BEHAVES
//  without changing how it works. Pin assignments, timeouts, odds, durations,
//  animation speeds, sound file names and volumes all live here; the .cpp files
//  only contain logic that reads them.
//
//  Rules of thumb:
//    - Anything you might plausibly want to retune belongs here.
//    - Anything that is a lookup TABLE (the troll list, the font list, the
//      brightness ramp, the weather-scene styles, the alarm clip pool) stays
//      next to the code that walks it, because the table and its loop have to
//      be read together. Those are all clearly marked where they live.
//    - Locations and WiFi networks live in locations.h, since they carry
//      secrets and are structured records rather than dials.
//
//  Times are milliseconds unless the name says otherwise.
// ============================================================================

#ifndef VALUES_H
#define VALUES_H

#include <stdint.h>

// ============================================================================
//  SCREEN
//  The panel is a 128x64 SSD1309. Origin is top-left, and setupDisplay() calls
//  setFontPosTop(), so a text y is the TOP of the line rather than a baseline.
// ============================================================================
static constexpr int SCREEN_W = 128;   // panel width in pixels
static constexpr int SCREEN_H = 64;    // panel height in pixels

// ============================================================================
//  PIN ASSIGNMENTS
//  Change these to match your wiring. Note the encoder sits on GPIO 25/26,
//  which are also the ESP32's DAC pins -- if you ever want analog out, the
//  encoder has to move first.
// ============================================================================

// OLED, on the hardware SPI bus (MOSI 23, SCK 18, both fixed by the peripheral).
static constexpr int PIN_OLED_RESET = 16;
static constexpr int PIN_OLED_DC    = 4;
static constexpr int PIN_OLED_CS    = 5;

// DS3231 real-time clock, on I2C.
static constexpr int PIN_RTC_SDA = 21;
static constexpr int PIN_RTC_SCL = 22;

// Rotary encoder. The two quadrature pins plus its push switch.
static constexpr int PIN_ENCODER_CLK    = 25;
static constexpr int PIN_ENCODER_DT     = 26;
static constexpr int PIN_ENCODER_BUTTON = 32;

// The three front buttons. All need 10k pull-ups in hardware.
static constexpr int PIN_BTN_ALARM  = 13;
static constexpr int PIN_BTN_SNOOZE = 19;
static constexpr int PIN_BTN_BRIGHT = 14;

// I2S audio out, into a MAX98357A amplifier. All three lines are used -- this
// is real I2S to a DAC, not delta-sigma out of a single pin.
static constexpr int PIN_AUDIO_DOUT = 17;   // MAX98357A DIN
static constexpr int PIN_AUDIO_BCLK = 27;   // MAX98357A BCLK
static constexpr int PIN_AUDIO_WCLK = 33;   // MAX98357A LRC

// The amplifier's own GAIN pin sets the analog output level, and it is the only
// way to get more volume WITHOUT distortion -- the software gains below all
// clip once they pass 1.0. Left floating it is +9dB; tie it to ground for +15dB.

// ============================================================================
//  MAIN LOOP
//  mainLoop() aims for a fixed frame time and spends any leftover budget
//  servicing audio rather than sleeping, so clips start promptly.
// ============================================================================
static constexpr int TARGET_FPS      = 30;                 // render rate for animated screens
static constexpr int TARGET_FRAME_MS = 1000 / TARGET_FPS;  // derived frame budget

// ============================================================================
//  INPUT
//  Buttons are interrupt-driven with a software debounce; the encoder emits
//  four quadrature transitions per physical detent.
// ============================================================================
static constexpr unsigned long BTN_DEBOUNCE_MS = 200;   // ignore repeat edges within this
static constexpr int ENCODER_STEPS_PER_DETENT  = 4;     // transitions per click of the knob
static constexpr unsigned long ENCODER_BTN_DEBOUNCE_MS = 40;   // shorter: the knob switch is cleaner

// Secret dev-menu unlock: ALARM, SNOOZE, ALARM, then an encoder press. The
// sequence resets if you pause longer than this between presses.
static constexpr unsigned long DEV_CODE_WINDOW_MS = 3000;

// ============================================================================
//  DAY / NIGHT
//  One window, used in three places: the display dims to night mode inside it,
//  the SOUND_DAY mute mode goes quiet inside it, and the daily trolls are
//  scheduled to start at WAKE_UP_HOUR and end at NIGHT_MODE_HOUR. Moving these
//  moves all three together, which is the point.
// ============================================================================
static constexpr int NIGHT_MODE_HOUR = 23;   // hour (0-23) the screen dims for the night
static constexpr int WAKE_UP_HOUR    = 7;    // hour (0-23) it comes back up

// ============================================================================
//  WIFI AND WEATHER
//  WiFi is only powered up when it is needed -- at boot, and for each weather
//  refresh -- then switched off again. open-meteo is occasionally very slow, so
//  the timeouts are generous and a fetch retries before giving up.
// ============================================================================
static constexpr unsigned long WIFI_CONNECT_TIMEOUT    = 30000;   // give up on one connect attempt
static constexpr unsigned long WIFI_AUTO_CONNECT_DELAY = 20000;   // startup picker idle time before auto-trying every network

static constexpr unsigned long WEATHER_UPDATE_INTERVAL = 600000;  // how often to refresh (10 min)
static constexpr int           WEATHER_FETCH_ATTEMPTS  = 3;       // tries within a single refresh
static constexpr unsigned long WEATHER_RETRY_DELAY     = 1500;    // pause between those tries
static constexpr int           WEATHER_HTTP_TIMEOUT    = 20000;   // per-request timeout
static constexpr int           WEATHER_FAIL_THRESHOLD  = 3;       // failed refreshes before the stale "X" appears

// Hour after which the weather readout switches from today's high to
// tomorrow's -- by the evening today's high is history.
static constexpr int WEATHER_TOMORROW_AFTER_HOUR = 20;

// ============================================================================
//  CLOCK FACE
//  Layout of the main screen. The date, temperature and alarm indicator share
//  one row across the bottom; the digits are centred in the band above it, so
//  moving the row moves the centring with it.
// ============================================================================
static constexpr int CLOCK_ROW_Y      = 55;   // top of the date / temp / alarm row
static constexpr int CLOCK_EDGE_INSET = 1;    // px in from the bezel for the date and temp
static constexpr int CLOCK_AMPM_X     = 116;  // left edge of the AM/PM indicator
static constexpr int CLOCK_AMPM_Y     = 20;

static constexpr unsigned long ALARM_FLASH_MS  = 500;   // screen inversion period while ringing
static constexpr unsigned long SNOOZE_BLINK_MS = 500;   // "Alarm" label blink period while snoozed

// Real minutes the clock is silently wrong by, re-rolled at midnight. Only
// applied when the Clock drift troll is switched on.
static constexpr int CLOCK_DRIFT_MAX_MIN = 10;   // +/- this many minutes

// ============================================================================
//  IDLE MODE
//  Nobody has touched the clock in days, so assume the room is empty: dim the
//  panel and suspend every troll until someone presses something. Stops the
//  clock sitting inside a gag for a week while you are away. millis() wraps at
//  ~49 days, so keep this comfortably under that.
// ============================================================================
static constexpr unsigned long IDLE_AFTER_MS = 5UL * 24 * 60 * 60 * 1000;   // 5 days

// ============================================================================
//  ALARM
//  Defaults used the first time the clock boots, before anything is saved.
// ============================================================================
static constexpr int DEFAULT_ALARM_HOUR    = 7;
static constexpr int DEFAULT_ALARM_MINUTE  = 0;
static constexpr int DEFAULT_SNOOZE_MINUTES = 9;

// ============================================================================
//  AUDIO ENGINE
//  Digital software gain, where 1.0 is clean full scale. Above that the library
//  clips -- louder but gritty, which suits this clock fine.
//
//  AUDIO_MAX_GAIN must stay below 4.0: the library packs gain into a uint8_t as
//  gain*64, so exactly 4.0 becomes 256, wraps to 0, and you get silence.
// ============================================================================
static constexpr float AUDIO_DEFAULT_GAIN = 1.0f;   // startup volume
static constexpr float AUDIO_MAX_GAIN     = 3.9f;   // ceiling for every gain argument

static constexpr int AUDIO_QUEUE_MAX = 8;   // clips that can wait in the play queue

// I2S DMA depth, in buffers of 128 samples. The 30fps loop only services audio
// once per frame, and the default 8 buffers (~23ms at 44.1kHz) drained between
// frames and chopped short clips into a stutter. 32 is ~93ms, with margin.
static constexpr int AUDIO_DMA_BUFFERS = 32;

// After a clip ends, its last fraction of a second is still draining out of
// that DMA buffer. Starting the next queued clip immediately would collide with
// the tail and swallow the final word, so the queue waits this long between
// clips. It doubles as a natural pause between spoken lines. Raise it if clips
// still get clipped; lower it for tighter back-to-back playback.
static constexpr unsigned long AUDIO_TAIL_DRAIN_MS = 200;

// ============================================================================
//  SOUND FILES
//  Everything in data/, uploaded to LittleFS with "Upload Filesystem Image".
//  Extension picks the decoder: .mp3 decodes, anything else is treated as WAV.
//  Prefer WAV for short effects (no decode latency) and MP3 for long clips.
// ============================================================================
static constexpr const char* SND_BEEP     = "/beep.wav";          // menu / countdown blip
static constexpr const char* SND_BOING    = "/boing.wav";         // brightness button
static constexpr const char* SND_WAKEUP   = "/wakeup.mp3";        // default alarm clip
static constexpr const char* SND_NAME     = "/Hulton.mp3";        // the name the alarm greets you with
static constexpr const char* SND_WHISPER  = "/wakeupwhisper.mp3"; // Eye stare
static constexpr const char* SND_BUBBLES  = "/bubbles.wav";       // Aquarium
static constexpr const char* SND_BOMB     = "/bomb.mp3";          // Self destruct
static constexpr const char* SND_MEOW     = "/meow.mp3";          // Cat trolls
static constexpr const char* SND_RIBBIT   = "/ribbit.mp3";        // Frog
static constexpr const char* SND_FOOTSTEP = "/footstep.mp3";      // Walker
static constexpr const char* SND_FISH_SWARM = "/fishswarm.mp3";   // the shoal rushing past
static constexpr const char* SND_UNDERWATER = "/underwater.mp3";  // long ambience loop

// Per-effect volumes, as absolute gains on the 0.0 - AUDIO_MAX_GAIN scale,
// independent of the volume set in the menu. Use -1 instead to leave an effect
// at whatever the menu volume currently is.
static constexpr float GAIN_ALARM_TOGGLE = 0.5f;   // beep when arming / disarming
static constexpr float GAIN_BRIGHT_BOING = 0.5f;   // brightness button, at the dimmest level
static constexpr float GAIN_COUNTDOWN    = 0.6f;   // countdown beeps
static constexpr float GAIN_WHISPER      = -1;  // the eye, deliberately quiet
static constexpr float GAIN_BUBBLES      = 0.25f;  // aquarium, quieter still
static constexpr float GAIN_ANIMAL       = -1;   // meow / ribbit
static constexpr float GAIN_FOOTSTEP     = -1;   // walker
static constexpr float GAIN_FISH_SWARM   = -1;   // the shoal rushing past
static constexpr float GAIN_UNDERWATER   = -1;   // the ambience loop

// ============================================================================
//  BLUETOOTH SPEAKER MODE
//  A plain A2DP sink tops out at full-scale PCM, which is too quiet through
//  this amplifier. The custom volume control lets the factor exceed unity and
//  clips the result -- the same "louder but gritty" trade the wired path makes.
//  For clean extra loudness the lever is the amplifier's own gain pin.
// ============================================================================
static constexpr const char* BT_DEVICE_NAME = "WorstAlarmClock";   // name shown when pairing

static constexpr int32_t BT_OVERDRIVE_MAX = 0x2800;  // 2.5x (unity is 0x1000)
static constexpr uint8_t BT_VOLUME_DEFAULT = 110;    // 0-127, where the session starts
static constexpr int     BT_VOLUME_STEP    = 8;      // change per encoder detent

static constexpr unsigned long BT_VOLUME_SHOW_MS = 1500;  // how long the volume bar stays up
static constexpr int           BT_SCROLL_PX_SEC  = 26;    // "title - artist" marquee speed
static constexpr int           BT_SCROLL_GAP_PX  = 24;    // gap before the marquee's wrapped copy

// ============================================================================
//  MENU LAYOUT
//  Shared by every list screen, so the spacing stays consistent between them.
// ============================================================================
static constexpr int MENU_ROW_H      = 12;   // height of one list row
static constexpr int MENU_START_Y    = 16;   // top of the first row, under the header rule
static constexpr int MENU_VISIBLE    = 4;    // rows on screen at once
static constexpr int MENU_HEADER_Y   = 15;   // y of the rule under the title
static constexpr float MENU_VOLUME_STEP = 0.1f;   // gain change per encoder detent

// ============================================================================
//  TROLLS -- WHEN THEY FIRE
//  Once a minute, while the plain clock screen is showing and nothing else is
//  going on, the clock rolls twice: first for a major, then for a minor. The
//  odds are per CATEGORY, so with N trolls of a kind enabled, each individual
//  one is 1 in (odds * N).
//
//  Dailies get their own roll, once each morning at WAKE_UP_HOUR, and run until
//  NIGHT_MODE_HOUR. Weather scenes are never rolled at all -- they mirror the
//  real forecast, or you launch them by hand from the dev menu.
// ============================================================================
static constexpr unsigned long TROLL_ROLL_INTERVAL_MS = 60000;   // how often the dice come out

static constexpr int TROLL_MAJOR_ODDS = 400;  // 1-in-N per minute: ~1 every 6.7 hours
static constexpr int TROLL_MINOR_ODDS = 40;   // 1-in-N per minute: ~1 every 40 minutes
static constexpr int TROLL_DAILY_ODDS = 20;   // 1-in-N per day: most days are just days

// ============================================================================
//  TROLLS -- HOW LONG THEY LAST
//  Three shared backstops, then the individual ones. A backstop is not the
//  intended lifetime: a button ends these instantly. It only stops a troll that
//  fired while you were out from still running when you get back.
// ============================================================================

// For minors that leave the time UNREADABLE -- clock carried off, digits
// scrambled, or the screen given over entirely to an animation.
static constexpr unsigned long TROLL_BLIND_MINOR_MS = 30UL * 60 * 1000;        // 30 minutes

// For minors where the real time stays legible. These can happily run for
// hours; this just stops them running for days.
static constexpr unsigned long TROLL_PATIENT_MINOR_MS = 6UL * 60 * 60 * 1000;  // 6 hours

// The "sit there and take it" lockout the icon trolls and Eye stare+ share.
// Every button is dead for a randomly chosen length in this range.
static constexpr int TROLL_LOCKOUT_MIN_MINUTES = 2;
static constexpr int TROLL_LOCKOUT_MAX_MINUTES = 60;

// Individual durations, for the rows that use a fixed one.
static constexpr unsigned long TR_FAKE_UPDATE_MS = 60UL * 60 * 1000;        // 1 hour of progress bar
static constexpr unsigned long TR_WRONG_TIME_MS  = 120UL * 60 * 1000;       // 2 hours of lying
static constexpr unsigned long TR_MAD_TEMP_MS    = 120UL * 60 * 1000;       // 2 hours of nonsense weather
static constexpr unsigned long TR_CAT_CLOCK_MS   = 20UL * 60 * 1000;        // 20 minutes of cat
static constexpr unsigned long TR_AQUARIUM_MS    = 24UL * 60 * 60 * 1000;   // capped by the daily end-of-day rule
static constexpr unsigned long TR_DAILY_MS       = 12UL * 60 * 60 * 1000;   // ditto, for the simple dailies
static constexpr unsigned long TR_LOSE_SLIDE_MS  = 3600;                    // an element sliding off for good

// ============================================================================
//  TROLLS -- INDIVIDUAL DIALS
//  One block per troll, in the order they appear in trollEvents[]. These are
//  the numbers that give each one its character.
// ============================================================================

// ---- Fish swim: a school crosses the clock and leaves ----------------------
static constexpr int FISH_MAX       = 20;      // pool size, and the largest school
static constexpr int FISH_MIN_COUNT = 1;       // smallest school
static constexpr int FISH_FAST_MS   = 3000;    // quickest crossing
static constexpr int FISH_SLOW_MS   = 14000;   // slowest crossing
static constexpr int FISH_STAGGER   = 7000;    // largest entry delay, so they trickle in
static constexpr int FISH_MARGIN    = 300;     // grace period before the event ends
static constexpr int FISH_Y_MIN     = 14;      // highest a fish swims (keeps its bob on screen)
static constexpr int FISH_Y_MAX     = 51;      // lowest

// ---- Fish swarm: a shoal floods past and carries the clock off -------------
static constexpr int SWARM_MAX       = 36;   // pool size, and the densest shoal
static constexpr int SWARM_MIN_COUNT = 28;   // sparsest shoal; below this it leaves gaps
static constexpr int SWARM_ROWS      = 6;    // horizontal bands, so coverage has no holes
static constexpr int SWARM_FAST_MS   = 480;  // quickest crossing -- it is a rush, not a stroll
static constexpr int SWARM_SLOW_MS   = 850;  // slowest
static constexpr int SWARM_STAGGER   = 900;  // largest entry delay
static constexpr int SWARM_HIDE_PCT  = 40;   // % through act 1 to snap the clock away, when cover is thickest

// ---- Lone-fish patrol: shared by Fish swarm's last act and the Aquarium ----
// One fish at a time, alternating direction, with irregular gaps between runs.
// The aquarium's pace is slower and the gaps longer, because it has all day.
static constexpr uint32_t PATROL_SWARM_FAST_MS = 2200;   // quickest crossing, after the shoal
static constexpr uint32_t PATROL_SWARM_SLOW_MS = 4200;   // slowest
static constexpr uint32_t PATROL_SWARM_GAP_MIN = 700;    // shortest wait between fish
static constexpr uint32_t PATROL_SWARM_GAP_MAX = 2700;   // longest

static constexpr uint32_t PATROL_AQ_FAST_MS = 5000;      // the aquarium ambles
static constexpr uint32_t PATROL_AQ_SLOW_MS = 8500;
static constexpr uint32_t PATROL_AQ_GAP_MIN = 2000;
static constexpr uint32_t PATROL_AQ_GAP_MAX = 7000;

// 1-in-N frames a swimming fish blows a burst of bubbles. At 30fps, 900 is
// roughly one burst per 30 seconds of swimming -- rare enough to be a surprise
// rather than a feature.
static constexpr int PATROL_BUBBLE_ODDS = 900;
static constexpr int SWARM_BUBBLE_ODDS  = 1400;   // rarer: thirty fish would make a fog

// ---- Bubbles: shared by Fish swarm and Aquarium ----------------------------
static constexpr int BUBBLE_MAX      = 20;    // particle pool
static constexpr int BUBBLE_RISE_PX  = 78;    // distance travelled over a bubble's life
static constexpr int BUBBLE_LIFE_MIN = 1800;  // shortest life
static constexpr int BUBBLE_LIFE_MAX = 3600;  // longest

// ---- Aquarium: an all-day fish tank over the working clock -----------------
// The sand surface rolls. AQ_FLOOR_LOW is the LOWEST the surface ever sits --
// one pixel above the date/temp/alarm row, so those readouts are always buried
// (and redrawn in black on the sand) no matter where the hills fall.
static constexpr int AQ_FLOOR_LOW      = CLOCK_ROW_Y - 1;   // lowest point of the sand
static constexpr int AQ_FLOOR_RANGE    = 4;                 // total hill height
static constexpr int AQ_BUBBLE_GAP_MIN = 20000;             // quietest gap between bubble bursts
static constexpr int AQ_BUBBLE_GAP_MAX = 60000;             // longest

// ---- Eye stare: a giant eye watches you and whispers -----------------------
// A real eye blinks every few seconds, so the eye is OPEN nearly all the time
// and the blink itself is a fast one-off.
static constexpr uint32_t EYE_GAP_MIN     = 4000;   // shortest hold between blinks
static constexpr uint32_t EYE_GAP_MAX     = 10000;   // longest
static constexpr uint32_t EYE_DOUBLE_GAP  = 320;    // pause inside a double blink
static constexpr int      EYE_DOUBLE_ODDS = 6;      // 1-in-N blinks come in pairs
static constexpr uint32_t EYE_WHISPER_MIN = 4000;   // shortest gap between whispers
static constexpr uint32_t EYE_WHISPER_MAX = 20000;  // longest -- sparse on purpose

// ---- Walker: a figure paces across the screen ------------------------------
// Ground covered per stride, in pixels. Set to the sprite's widest leg span, so
// the planted foot stays planted -- get this wrong and the figure moonwalks.
static constexpr int WALK_STRIDE_PX = 29;

// ---- Cat bounce and DVD mode: ricochet around the screen -------------------
// Speeds are deliberately not neat ratios of each other, so corner hits stay
// rare. Making them nearly equal would send the sprite diagonally into a corner
// almost immediately.
static constexpr float CAT_VX = 23.0f;   // px/sec horizontally
static constexpr float CAT_VY = 15.0f;   // px/sec vertically
static constexpr float DVD_VX = 27.0f;
static constexpr float DVD_VY = 17.0f;

// ---- Cat clock and Frog: an animal sits on part of the clock ---------------
// 1-in-N frames a noise plays. At 30fps, 900 is roughly every 30 seconds.
static constexpr int ANIMAL_NOISE_ODDS = 900;

// ---- Error popup and Self destruct: countdowns -----------------------------
static constexpr int      CD_MIN_SECONDS   = 20;    // shortest countdown
static constexpr int      CD_MAX_SECONDS   = 600;   // longest
static constexpr uint32_t ERR_REBOOT_MS    = 5000;  // fake boot screen after the countdown
static constexpr uint32_t ERR_BLACK_MS     = 700;   // dead screen first, like a real reset
static constexpr int      ERR_BEEP_EVERY_S = 10;    // marker beep interval during the countdown

static constexpr uint32_t SD_FLASH_MS    = 2200;   // strobe and bang
static constexpr uint32_t SD_STROBE_MS   = 70;     // half-period of the strobe
static constexpr uint32_t SD_DEAD_MS     = 100000;  // the long nothing afterwards -- this is the joke
static constexpr uint32_t SD_BACK_MS     = 1600;   // clock elements sliding home
static constexpr int      SD_BEEP_EVERY_S = 5;     // marker beep interval, faster than the error popup

// ---- Font cycle: rips through the clock fonts on its own -------------------
static constexpr uint32_t FONT_STEP_MS = 220;   // ms per font; lower is faster

// ---- Slide off: the time laps the screen until you catch it ----------------
static constexpr uint32_t SLIDE_PERIOD_MS = 650;   // one full lap; lower is faster
static constexpr int      SLIDE_BELOW     = 70;    // offset that puts it fully below the screen
static constexpr int      SLIDE_ABOVE     = -70;   // and fully above it

// ---- Digit scroll: the digits race too fast to read ------------------------
static constexpr uint32_t DIGIT_STEP_MS = 35;   // ms per fake minute; lower is faster

// ---- Clock flees: the time hides until you ask it back ---------------------
static constexpr uint32_t FLEE_MS = 900;   // slide time, each direction
static constexpr int      FLEE_Y  = -72;   // far enough up to be fully off screen

// ---- All adrift: every element wanders on its own sine wave ----------------
// Ranges are rolled per element per launch, so nothing ever moves in lockstep.
// That unison is what would make it read as one sliding image rather than four
// things coming loose independently.
static constexpr int ADRIFT_AMP_X_MIN = 18;   // horizontal swing, px
static constexpr int ADRIFT_AMP_X_MAX = 46;
static constexpr int ADRIFT_AMP_Y_MIN = 8;    // vertical swing, px
static constexpr int ADRIFT_AMP_Y_MAX = 23;
static constexpr int ADRIFT_RATE_MIN  = 30;   // rad/sec * 100
static constexpr int ADRIFT_RATE_MAX  = 140;

// ---- Alarm drift (fundamental): the alarm goes off at the wrong time -------
static constexpr int ALARM_DRIFT_MAX_MIN = 10;   // +/- this many minutes, re-rolled on each arming

// ---- Alarm volume (fundamental): each ring is too quiet or far too loud ----
// Weighted, never uniform: half the time you sleep through it, half the time it
// takes the roof off. The useful middle is deliberately never chosen.
static constexpr int ALARM_GAIN_QUIET_MIN = 5;    // gain * 100
static constexpr int ALARM_GAIN_QUIET_MAX = 40;
static constexpr int ALARM_GAIN_LOUD_MIN  = 250;
static constexpr int ALARM_GAIN_LOUD_MAX  = 391;

// ---- Snooze wheel (fundamental): a slot machine decides your snooze --------
static constexpr int           SNOOZE_WHEEL_MAX     = 20;     // wheel holds 1..N minutes
static constexpr unsigned long SNOOZE_WHEEL_SPIN_MS = 2200;   // spin length
static constexpr unsigned long SNOOZE_WHEEL_HOLD_MS = 900;    // pause on the result so you can read it
static constexpr int           SNOOZE_WHEEL_TURNS   = 4;      // full revolutions before it lands

// ---- Snooze games (fundamental): you have to earn the snooze ---------------
static constexpr unsigned long SNOOZE_GAME_TIMEOUT_MS = 30000;  // never pressing at all means no snooze
static constexpr unsigned long SNOOZE_GAME_VERDICT_MS = 1200;   // how long the result is held on screen

// Catch the snooze: the word bounces and you press while it is inside the box.
// The period is the difficulty dial -- shorter is faster and harder.
static constexpr uint32_t CATCH_BOUNCE_MS = 3000;

// Slot reels: stop each of three reels on SNZ. Milliseconds per item scrolled
// past, which is also your reaction window, since it is how long an item stays
// centred. Lower is harder, and each reel is quicker than the last.
static constexpr float REEL_RATE_MS[3] = { 340.0f, 300.0f, 240.0f };

#endif
