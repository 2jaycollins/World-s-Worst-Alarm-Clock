#include "clock_logic.h"
#include "clock.h"     // tempMode / night mode / u8g2 for the offline splash
#include "encoder.h"   // the startup WiFi picker reads the encoder directly

RTC_DS3231 rtc;
Preferences prefs;

bool twelveHourFormat  = true;
bool clockDriftEnabled = true;
bool isPM              = false;
int  fakeDriftMinutes  = 0;

float todayHighTemp    = 0.0;
float tomorrowHighTemp = 0.0;
float currentTemp      = 0.0;
int   weatherCode      = 0;
bool  weatherUnavailable = false;

int currentLocationIndex = 0;
int currentWifiIndex     = 0;
bool offlineMode         = false;

static unsigned long lastWeatherFetch = 0;
static int  weatherFailCount = 0;   // consecutive failed refreshes
static int  lastCheckedDay   = -1;  // day the drift was last re-rolled

extern bool bluetoothActive;   // defined in bluetooth.cpp; gates the weather refresh

// ============================================================================
//  SETUP
// ============================================================================

// Bring up the RTC, get onto WiFi, and set the clock. Falls back to a "dumb"
// offline clock on the RTC's battery-backed time if no network is reachable.
void setupClock() {
    Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
    if (!rtc.begin()) {
        DEBUG_PRINTLN("RTC not found!");
        while (1);   // nothing works without it
    }

    loadLocationPreference();
    loadWifiPreference();

    // Try the saved network first. On a routine power-cut reboot this just
    // reconnects with nobody present and the picker never appears. Only if the
    // saved network is gone -- the clock has physically moved -- do we fall
    // through to the picker, which also auto-tries everything if left alone.
    bool connected = connectWifi();
    if (!connected) connected = selectWifiAtStartup();

    if (connected) {
        saveWifiPreference(currentWifiIndex);   // remember whatever actually worked
        syncTimeFromNTP();
        fetchWeather();
        disconnectWifi();
    } else {
        offlineMode = true;
        disconnectWifi();
        u8g2.clearBuffer();
        centerText("Offline Mode");
        u8g2.sendBuffer();
        delay(2000);

        // No forecast to show, so stand in a fixed mild reading rather than
        // blanking the readout or leaving whatever floats in memory. Only set
        // in memory: a later online boot fetches the real thing.
        currentTemp        = OFFLINE_TEMP;
        todayHighTemp      = OFFLINE_TEMP;
        tomorrowHighTemp   = OFFLINE_TEMP;
        weatherCode        = 0;      // clear sky
        weatherUnavailable = false;  // deliberate stand-in, not a stale reading
        DEBUG_PRINTLN("No WiFi at boot -- offline clock, placeholder date and temp");
    }

    // Sync night mode to the wall clock now the RTC has the real time. The
    // transitions below are edge-triggered, so a reboot at 02:00 would
    // otherwise come up at daytime brightness and stay there until the evening.
    DateTime now = getCurrentTime();
    initDayTriggers(now.day());
    if (autoNightMode && isNightHour(now.hour())) enterNightMode();

    DEBUG_PRINTLN("Clock setup complete");
}

// ============================================================================
//  THE PER-FRAME UPDATE
// ============================================================================

bool triggerWakeUp   = false;
bool triggerNightMode = false;

static int lastWakeDay  = -1;   // day each transition last fired, so they fire once each
static int lastNightDay = -1;

// True inside the night window, which wraps midnight.
bool isNightHour(int hour) {
    return hour >= NIGHT_MODE_HOUR || hour < WAKE_UP_HOUR;
}

// Seed the day latches from the current time, so the boot-time state sync in
// setupClock() does not immediately re-fire as an edge.
//
// A latch is claimed ONLY IF that transition's moment has already gone by
// today. Claiming both unconditionally is the obvious version, and it silently
// eats a transition: boot at any time before NIGHT_MODE_HOUR and lastNightDay
// already equals today, so 23:00 comes and goes without dimming. The wake-up
// still fires, because the next one falls on a DIFFERENT day and the latch
// compares days -- which is exactly why the morning looked fine while the
// evening never happened. On a clock that gets rebooted often, night mode then
// only ever appears via the boot-time sync below.
void initDayTriggers(int day) {
    int hour = getCurrentTime().hour();

    // The wake-up has happened if we are past it and not yet into the night.
    lastWakeDay  = isNightHour(hour) ? -1 : day;

    // Tonight's dimming has happened only inside the 23:00-23:59 tail of today.
    // Between midnight and the wake-up hour we are in the night that began
    // YESTERDAY, so today's is still to come and the latch stays unclaimed.
    lastNightDay = (hour >= NIGHT_MODE_HOUR) ? day : -1;
}

// Once per frame: re-roll the drift at midnight, run the night-mode
// transitions, and refresh the weather on its timer.
void updateClock() {
    DateTime now = getCurrentTime();

    if (now.day() != lastCheckedDay && clockDriftEnabled) {
        lastCheckedDay = now.day();
        rollDailyDrift();
        DEBUG_PRINTLN("New day -- drift rerolled");
    }

    // Fire once per day on ENTERING the window rather than matching an exact
    // second: updateClock() can itself block for a minute or more (a connect
    // attempt plus three HTTP tries), and a refresh straddling the boundary
    // would otherwise skip night mode for the whole night.
    if (lastWakeDay != now.day() && !isNightHour(now.hour())) {
        lastWakeDay = now.day();
        if (autoNightMode) triggerWakeUp = true;
    }
    if (lastNightDay != now.day() && now.hour() >= NIGHT_MODE_HOUR) {
        lastNightDay = now.day();
        if (autoNightMode) triggerNightMode = true;
    }

    // Consumed here rather than in the clock's render path, which only runs in
    // the clock and alarm states -- sitting in the menu at bedtime used to
    // defer night mode until you next looked at the clock.
    if (triggerNightMode) {
        triggerNightMode = false;
        enterNightMode();
        DEBUG_PRINTLN("Night mode on");
    }
    if (triggerWakeUp) {
        triggerWakeUp = false;
        exitNightMode();
        DEBUG_PRINTLN("Night mode off");
    }

    // Never while Bluetooth is active: connectWifi() blocks and powers the
    // radio back on, which would freeze the UI and fight the BT stack. This
    // also runs in offline mode -- the stall is accepted in exchange for the
    // clock recovering on its own when the network comes back.
    if (!bluetoothActive && shouldRefreshWeather()) {
        lastWeatherFetch = millis();   // mark the attempt, so a failure does not retry every frame

        bool updated = false;
        if (connectWifi()) {
            // Coming back from an offline boot: the RTC has never been
            // corrected and the date on screen is still the placeholder, so
            // fix both before the readings land.
            if (offlineMode) {
                syncTimeFromNTP();
                offlineMode = false;
                initDayTriggers(getCurrentTime().day());
                DEBUG_PRINTLN("Network is back -- leaving offline mode");
            }
            updated = fetchWeather();
        }
        disconnectWifi();

        if (updated) {
            weatherFailCount   = 0;
            weatherUnavailable = false;
        } else {
            // Keep the last known readings on screen; only mark them stale once
            // several refreshes in a row have missed, since a blip is normal.
            weatherFailCount++;
            if (weatherFailCount >= WEATHER_FAIL_THRESHOLD) weatherUnavailable = true;
            DEBUG_PRINTF("Weather refresh failed (%d in a row)\n", weatherFailCount);
        }
    }
}

// ============================================================================
//  TIME
// ============================================================================

DateTime getCurrentTime() {
    return rtc.now();
}

// Minutes since midnight as the DISPLAY sees them, drift included and wrapped.
static int displayMinutesOfDay() {
    DateTime now = getCurrentTime();
    int total = now.hour() * 60 + now.minute() + fakeDriftMinutes;
    return ((total % 1440) + 1440) % 1440;
}

// The hour to show, converted to 12-hour form if that is the setting. Sets isPM
// as a side effect, which drawAMPM() reads.
int getDisplayHour() {
    int hh = displayMinutesOfDay() / 60;

    if (!twelveHourFormat) {
        isPM = false;
        return hh;
    }

    isPM = (hh >= 12);
    if (hh == 0 || hh == 12) return 12;
    return hh > 12 ? hh - 12 : hh;
}

int getDisplayMinute() {
    return displayMinutesOfDay() % 60;
}

String getFormattedTime() {
    char buf[6];
    sprintf(buf, "%02d:%02d", getDisplayHour(), getDisplayMinute());
    return String(buf);
}

String getFormattedDate() {
    char buf[11];
    if (offlineMode) {
        sprintf(buf, "%02d/%02d", OFFLINE_MONTH, OFFLINE_DAY);
        return String(buf);
    }
    DateTime now = getCurrentTime();
    sprintf(buf, "%02d/%02d", now.month(), now.day());
    return String(buf);
}

// Pick today's silent error, re-rolled at midnight by updateClock().
void rollDailyDrift() {
    fakeDriftMinutes = random(-CLOCK_DRIFT_MAX_MIN, CLOCK_DRIFT_MAX_MIN + 1);
    DEBUG_PRINTF("Today's drift: %d minutes\n", fakeDriftMinutes);
}

int getFakeDrift() {
    return fakeDriftMinutes;
}

// ============================================================================
//  NETWORK
// ============================================================================

// Connect to the currently selected network, blocking until it is up or the
// timeout expires. The caller decides what to do without a connection.
bool connectWifi() {
    WifiNetwork net = WIFI_NETWORKS[currentWifiIndex];
    DEBUG_PRINTF("Connecting to WiFi: %s (%s)\n", net.name, net.ssid);

    WiFi.begin(net.ssid, net.password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= WIFI_CONNECT_TIMEOUT) {
            DEBUG_PRINTLN("\nWiFi connect timed out");
            return false;
        }
        delay(500);
        DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN("\nWiFi connected");
    return true;
}

// Drop the link and power the radio down.
void disconnectWifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    DEBUG_PRINTLN("WiFi disconnected");
}

// Power the radio back on and start associating, WITHOUT waiting for the link.
// Used when leaving Bluetooth mode; the next weather refresh picks up once the
// association completes.
void reconnectWifiAsync() {
    WifiNetwork net = WIFI_NETWORKS[currentWifiIndex];
    WiFi.mode(WIFI_STA);
    WiFi.begin(net.ssid, net.password);
    DEBUG_PRINTLN("WiFi reconnecting (async, post-Bluetooth)");
}

// Set the RTC from NTP. Syncs UTC first, then applies the location's timezone
// rules and reads the time back, so DST is handled by the C library.
void syncTimeFromNTP() {
    Location loc = LOCATIONS[currentLocationIndex];
    struct tm timeinfo;

    configTime(0, 0, "pool.ntp.org");
    while (!getLocalTime(&timeinfo)) delay(500);

    setenv("TZ", loc.posixTz, 1);
    tzset();
    getLocalTime(&timeinfo);

    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));

    DEBUG_PRINTF("Time synced: %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
}

// ============================================================================
//  WEATHER
// ============================================================================

bool shouldRefreshWeather() {
    return millis() - lastWeatherFetch > WEATHER_UPDATE_INTERVAL;
}

// Fetch current temperature, condition code, and today's and tomorrow's highs.
// open-meteo's combined query is intermittently slow and sometimes drops the
// connection outright, so this retries a few times before giving up.
bool fetchWeather() {
    Location loc = LOCATIONS[currentLocationIndex];

    String url = "http://api.open-meteo.com/v1/forecast?latitude=";
    url += loc.lat;
    url += "&longitude=";
    url += loc.lon;
    url += "&current=temperature_2m,weathercode";
    url += "&daily=temperature_2m_max&temperature_unit=fahrenheit&timezone=";
    url += loc.timezone;
    url += "&forecast_days=2";

    for (int attempt = 1; attempt <= WEATHER_FETCH_ATTEMPTS; attempt++) {
        WiFiClient client;
        HTTPClient http;
        http.setConnectTimeout(WEATHER_HTTP_TIMEOUT);
        http.setTimeout(WEATHER_HTTP_TIMEOUT);
        http.begin(client, url);

        int code = http.GET();
        DEBUG_PRINTF("Weather HTTP %d (attempt %d/%d)\n", code, attempt, WEATHER_FETCH_ATTEMPTS);

        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (err) {
                DEBUG_PRINT("Weather JSON parse failed: ");
                DEBUG_PRINTLN(err.c_str());
                http.end();
                return false;   // a 200 with bad JSON will not fix itself on retry
            }

            todayHighTemp    = doc["daily"]["temperature_2m_max"][0];
            tomorrowHighTemp = doc["daily"]["temperature_2m_max"][1];
            currentTemp      = doc["current"]["temperature_2m"];
            weatherCode      = doc["current"]["weathercode"];

            http.end();
            return true;
        }

        http.end();
        if (attempt < WEATHER_FETCH_ATTEMPTS) delay(WEATHER_RETRY_DELAY);
    }

    return false;   // every attempt failed: the caller keeps the last known values
}

// ============================================================================
//  SAVED PREFERENCES
// ============================================================================

// Switch location, persist it, then resync the time and forecast for the new one.
void onLocationChanged(int newIndex) {
    currentLocationIndex = newIndex;
    saveLocationPreference(newIndex);
    connectWifi();
    syncTimeFromNTP();
    fetchWeather();
    disconnectWifi();
    DEBUG_PRINTF("Location changed to: %s\n", LOCATIONS[newIndex].name);
}

void loadLocationPreference() {
    prefs.begin("clock", false);
    currentLocationIndex = prefs.getInt("location", 0);
    prefs.end();
    if (currentLocationIndex < 0 || currentLocationIndex >= LOCATION_COUNT)
        currentLocationIndex = 0;
}

void saveLocationPreference(int index) {
    prefs.begin("clock", false);
    prefs.putInt("location", index);
    prefs.end();
}

void loadWifiPreference() {
    prefs.begin("clock", false);
    currentWifiIndex = prefs.getInt("wifi", 0);
    prefs.end();
    if (currentWifiIndex < 0 || currentWifiIndex >= WIFI_COUNT) currentWifiIndex = 0;
}

void saveWifiPreference(int index) {
    prefs.begin("clock", false);
    prefs.putInt("wifi", index);
    prefs.end();
}

// ============================================================================
//  STARTUP WIFI PICKER
//  Only shown when the saved network failed to connect. Blocking on purpose --
//  nothing else can usefully run before the clock knows the time.
// ============================================================================

// The list carries one extra row past the known networks: "Offline Mode", which
// skips connecting altogether and boots straight to the dumb clock.
static const int WIFI_OFFLINE_INDEX = WIFI_COUNT;
static const int WIFI_ITEM_COUNT    = WIFI_COUNT + 1;

static const char* wifiItemName(int idx) {
    return idx == WIFI_OFFLINE_INDEX ? "Offline Mode" : WIFI_NETWORKS[idx].name;
}

// A scrolling list of the known networks. `connecting` swaps the footer for a
// status line while an attempt is in flight.
static void drawWifiSelectScreen(int sel, bool connecting) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 2, "Select WiFi");
    u8g2.drawHLine(0, MENU_HEADER_Y, SCREEN_W);

    const int visible = 3;
    int first = 0;
    if (sel >= visible) first = sel - visible + 1;

    for (int row = 0; row < visible; row++) {
        int idx = first + row;
        if (idx >= WIFI_ITEM_COUNT) break;
        int y = MENU_START_Y + row * MENU_ROW_H;
        if (idx == sel) {
            u8g2.drawBox(0, y, SCREEN_W, MENU_ROW_H);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }
        u8g2.drawStr(3, y + 1, wifiItemName(idx));
        u8g2.setDrawColor(1);
    }

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(2, 55, connecting ? "Connecting..." : "press to connect");
    u8g2.sendBuffer();
}

// Let the user pick a network, and connect to it. Picking "Offline Mode" gives
// up immediately and boots the dumb clock. If nobody interacts for
// WIFI_AUTO_CONNECT_DELAY, walks every known network itself (never the offline
// row), so an unattended power-cut reboot still recovers. Returns false when
// offline was chosen, or when none of the networks connect.
bool selectWifiAtStartup() {
    int sel = currentWifiIndex;
    if (sel < 0 || sel >= WIFI_COUNT) sel = 0;

    // Drop any encoder events queued during boot, so we do not auto-scroll.
    getEncoderDelta();
    wasEncoderPressed();

    int accum = 0;
    unsigned long lastInteraction = millis();
    drawWifiSelectScreen(sel, false);

    while (true) {
        if (wasEncoderPressed()) {
            // Offline Mode: no attempt at all, and currentWifiIndex is left
            // alone so the saved network survives for the next boot.
            if (sel == WIFI_OFFLINE_INDEX) {
                DEBUG_PRINTLN("Offline Mode selected at startup");
                return false;
            }

            drawWifiSelectScreen(sel, true);
            currentWifiIndex = sel;
            if (connectWifi()) return true;

            // Failed. Let them try another, and reset the unattended timer.
            getEncoderDelta();
            accum = 0;
            lastInteraction = millis();
            drawWifiSelectScreen(sel, false);
        }

        accum += getEncoderDelta();
        bool moved = false;
        while (accum >= ENCODER_STEPS_PER_DETENT) {
            accum -= ENCODER_STEPS_PER_DETENT;
            sel = (sel + 1) % WIFI_ITEM_COUNT;
            moved = true;
        }
        while (accum <= -ENCODER_STEPS_PER_DETENT) {
            accum += ENCODER_STEPS_PER_DETENT;
            sel = (sel - 1 + WIFI_ITEM_COUNT) % WIFI_ITEM_COUNT;
            moved = true;
        }
        if (moved) {
            lastInteraction = millis();
            drawWifiSelectScreen(sel, false);
        }

        if (millis() - lastInteraction >= WIFI_AUTO_CONNECT_DELAY) {
            for (int i = 0; i < WIFI_COUNT; i++) {
                drawWifiSelectScreen(i, true);
                currentWifiIndex = i;
                if (connectWifi()) return true;
            }
            return false;   // nothing connected: the caller boots offline
        }

        delay(10);
    }
}
