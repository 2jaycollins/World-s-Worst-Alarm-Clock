# The World's Worst Alarm Clock

An ESP32 alarm clock that keeps perfect time, pulls the weather, works as a Bluetooth speaker, and most importantly, it is very unpredictable.

It is a genuinely good alarm clock. It syncs from NTP, it has a battery-backed RTC so a power cut doesn't touch it, it dims itself at night, it plays whatever sound you want. It also has forty different ways of ruining your morning, and you get to choose which ones are armed.

![The clock on a bedside table](docs/hero.jpg)

**Demo video:** _(coming soon)_

---

## Why

I wanted an alarm clock I couldn't get complacent about. The problem with a normal alarm is that you learn it — exactly how long the snooze is, exactly what the beep sounds like, exactly how many times you can hit it. So this one doesn't let you. The snooze might be three minutes or it might be twenty, and you don't find out until a slot machine tells you. The alarm might go off nine minutes early. Some mornings the clock is just quietly wrong, by a few minutes, all day.

Every one of those behaviours is individually switchable from a hidden menu, so it runs as a completely normal clock until you start turning the knives on one at a time.

---

## What it does

**Clock** — Battery-backed DS3231 corrected from NTP at boot. 12/24 hour, date, live temperature. 45 selectable clock fonts, cycled straight from the knob. Automatic night mode dims the panel at a set hour and strips the screen back to just the time. Four brightness levels driven by the SSD1309's real contrast, pre-charge and VCOMH registers, so it goes genuinely dark rather than just dim.

**Weather** — Current temperature and today's high from open-meteo, switching to tomorrow's high in the evening. Multiple saved locations with full POSIX timezone and DST rules; changing location re-syncs both the clock and the forecast. WiFi powers down between refreshes, and a reading that has failed to update several times running is marked stale rather than silently shown as current.

**Alarm** — Set from the menu or armed with one button on the clock face. Plays any WAV or MP3 from the on-board filesystem, and several can be chained into a sequence. The panel flashes while it rings, and a ring pulls the display out of night mode — then puts it back afterwards.

**Bluetooth speaker** — The clock is an A2DP sink. Track title and artist scroll along the bottom and freeze when you pause; the three front buttons become transport controls and the knob is volume. An alarm going off will drop the Bluetooth link, take the audio hardware back, and ring.

![Bluetooth mode](docs/bluetooth.jpg)

---

## The trolls

Forty of them, all off by default, all individually switchable, sorted by how much damage they're allowed to do.

![A troll in action](docs/troll-demo.gif)

**Minor** gags are dismissed by any button — a school of fish swims across the clock, a figure paces the screen with footstep audio, the digits scroll too fast to read, the whole clock comes loose and drifts.

**Major** ones freeze the buttons. A firmware update that takes an hour and never finishes. A system error counting down in milliseconds to a reset that never comes. A self-destruct that strobes, detonates, and then leaves you looking at ten full seconds of dead screen wondering whether you've killed it — before the clock crawls back in from off-screen, one element at a time.

**Daily** moods are rolled each morning and last all day: a fish tank in front of the still-working clock, the entire display rotated 180°, or the clock shrunk to a tiny readout with the font control disabled.

**Weather scenes** track the real forecast — drizzle, rain, showers, snow and thunderstorms, each with its own particle density, fall speed, wind slant and animation cadence. When it's raining outside, it rains on your clock.

![Storm scene](docs/weather.gif)

**Fundamentals** aren't events at all, they're permanent changes in behaviour. The snooze wheel replaces your snooze length with a slot machine. Snooze games make you win a minigame to earn one — lose, and the alarm switches off entirely and has to be re-armed by hand. Alarm drift moves the alarm up to ten minutes either way, re-rolled every time you arm it so the offset can't be learned.

There's a safety switch, on by default, that guarantees no troll can suppress or steal the alarm. That's the difference between something you can sleep next to and a science experiment.

---

## Engineering notes

The interesting problem here isn't any individual gag — it's getting forty of them to coexist without the codebase becoming a pile of special cases.

**Trolls are data, not code.** Each one is a row in a single table: kind, duration, whether it draws over the clock, and function pointers for its lifecycle hooks. Adding one is an enum entry, a table row and a draw function — the menu entry, saved on/off state, random scheduling and input handling all follow automatically. A `static_assert` fails the build if the enum and the table ever drift apart.

**Trolls never touch the clock directly.** They write to a set of hooks — fake times, per-element pixel offsets, layout flags — and every hook is reset centrally when an event ends, so a troll can't permanently break the clock even if it's cancelled mid-animation.

**Animation is a pure function of time.** Nothing counts frames. Every draw asks "how far along am I, 0 to 1?" and derives the whole picture from that, so there's no state to reset, it's correct at any frame rate, and frame 900 can be reasoned about without simulating the 899 before it.

**The audio engine took the most debugging.** It plays WAV and MP3 off LittleFS with a ring-buffered play queue, per-clip gain overrides, and a decoder chosen by file extension so the MP3 decoder's heap cost is only paid when a clip needs it. Two bugs were worth the trouble: the library's `stop()` wipes the DMA buffer at end-of-file, cutting the last ~170ms off every clip, and starting the next queued clip immediately then collides with that same draining tail. The fix was a persistent I2S subclass that keeps the peripheral installed and can preserve the buffer, plus a short drain window between clips — effectively appending a silent tail in code instead of editing every audio file.

**Bluetooth and the sound engine share one I2S peripheral,** and only one driver can own it. Entering speaker mode tears down the sound engine's driver and hands the pins to the A2DP sink; leaving reinstalls it, and an alarm mid-playback performs that handoff on the fly. Fitting the Bluetooth stack also meant a custom partition table trading the second OTA slot for application space.

**Robustness, because it's a clock.** It boots offline on the RTC's last known time if no network is reachable. Buttons are interrupt-driven with debounce in the ISR. Every state transition is owned by one file rather than scattered across the modules that request them. An idle mode notices nobody has touched it in days, dims down and suspends every troll until someone comes back.

**One file holds every tunable value** — pins, timeouts, night-mode hours, sound files and volumes, the odds each troll fires at, and per-troll dials for how fast the fish swim or how often the eye blinks. Behaviour can be retuned without reading any logic.

**Stack:** C++ on ESP32 / Arduino via PlatformIO · U8g2 · ESP8266Audio · ESP32-A2DP · ArduinoJson · LittleFS · NVS · open-meteo

---

## Hardware

ESP32, SSD1309 128×64 SPI OLED, DS3231 RTC, rotary encoder, three buttons, and a MAX98357A I2S amplifier driving a small speaker.

- **Parts list:** _(link coming)_
- **Printable enclosure (STL):** _(link coming)_

![Wiring](docs/wiring.jpg)

---

## Code layout

```
include/values.h          every tunable number, in one place
src/application_code.cpp  main loop and every state transition
src/clock_logic.cpp       timekeeping, WiFi, weather
src/clock.cpp             clock face, brightness, night mode, fonts
src/alarm.cpp             setting, checking and ringing the alarm
src/audio.cpp             WAV/MP3 playback with a play queue
src/menu.cpp              settings, and the hidden troll menu
src/bluetooth.cpp         A2DP speaker mode
src/troll_events.cpp      all forty trolls
src/display.cpp           the panel, plus a time-based animation toolkit
```

Built with [PlatformIO](https://platformio.org/). Copy `include/secretsFILLER.h` to `include/secrets.h` and add your WiFi details before building.

---

Fonts from [U8g2](https://github.com/olikraus/u8g2). Weather from [open-meteo](https://open-meteo.com/). Bluetooth via [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP). Animations built with the [Wokwi animator](https://wokwi.com/animator), icons by [icons8](https://icons8.com/).
