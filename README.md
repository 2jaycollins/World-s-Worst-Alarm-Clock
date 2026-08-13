# The World's Worst Alarm Clock Ever

An ESP32 alarm clock that keeps perfect time, pulls the weather, works as a Bluetooth speaker, and most importantly, it is very unpredictable.

It is a genuinely good alarm clock. It syncs from NTP, it has a battery-backed RTC so a power cut doesn't touch it, it dims itself at night, and also has forty different ways of ruining your morning.

![The clock on a bedside table](docs/hero.jpg)

**Demo video:** _(coming soon)_

---

## Why

My roommate needed a new alarm clock, so I took it upon myself to make his life as slightly inconvenient as I could. The problem with a normal alarm is that it works exactly how it is supposed to. Where is the fun in that? This alarm clock is packed with a variety of "features" that do anything from canceling your alarm for no reason to whispering at you in your sleep. 

---

## What it does

**Clock** — Battery-backed DS3231 corrected from NTP at boot. 12/24 hour, date, live temperature. 45 selectable clock fonts, cycled straight from the knob. Automatic night mode dims the panel at a set hour and strips the screen back to just the time. Four brightness levels driven by the SSD1309's real contrast, pre-charge and VCOMH registers, so it goes genuinely dark rather than just dim.

**Weather** — Current temperature and today's high from open-meteo, switching to tomorrow's high in the evening. Multiple saved locations with full POSIX timezone and DST rules; changing location re-syncs both the clock and the forecast. WiFi powers down between refreshes, and a reading that has failed to update several times running is marked stale rather than silently shown as current.

**Alarm** — Set from the menu or armed with one button on the clock face. Plays any WAV or MP3 from the on-board filesystem, and several can be chained into a sequence. The panel flashes while it rings, and a ring pulls the display out of night mode — then puts it back afterwards.

**Bluetooth speaker** — The clock is an A2DP sink. Track title and artist scroll along the bottom and freeze when you pause; the three front buttons become transport controls and the knob is volume. An alarm going off will drop the Bluetooth link, take the audio hardware back, and ring.

![Bluetooth mode](docs/bluetooth.jpg)

---

## The trolls

Forty randomly-triggered events that range from quick visual gags to multi-hour long interruptions.

![A troll in action](docs/troll-demo.gif)

**Minor** gags are dismissed by any button — a school of fish swims across the clock, a figure paces the screen with footstep audio, the digits scroll too fast to read, the whole clock comes loose and drifts.

**Major** ones freeze the buttons. A firmware update that takes an hour and never finishes. A system error counting down in milliseconds to a reset that never comes. A self-destruct that strobes, detonates, and then leaves you looking at ten full seconds of dead screen wondering whether you've killed it — before the clock crawls back in from off-screen, one element at a time.

**Daily** moods are rolled each morning and last all day: a fish tank in front of the still-working clock, the entire display rotated 180°, or the clock shrunk to a tiny readout with the font control disabled.

**Weather scenes** track the real forecast — drizzle, rain, showers, snow and thunderstorms, each with its own particle density, fall speed, wind slant and animation cadence. When it's raining outside, it rains on your clock.

![Storm scene](docs/weather.gif)

**Fundamentals** aren't events at all, they're permanent changes in behaviour. The snooze wheel replaces your snooze length with a slot machine. Snooze games make you win a minigame to earn one — lose, and the alarm switches off entirely and has to be re-armed by hand. Alarm drift moves the alarm up to ten minutes either way, re-rolled every time you arm it so the offset can't be learned.

There's a safety switch, accessible by a secret code, that allows the customization of the events and what they are capable of. That's the difference between something you can sleep next to and a science experiment.

---

## Engineering notes

Forty gags share one codebase without turning into forty special cases, mostly because trolls are treated as data rather than code. Each one is a row in a table — kind, duration, whether it draws over the clock, function pointers for its lifecycle hooks. Adding a new troll means an enum entry, a table row, and a draw function; the menu entry, saved on/off state, random scheduling, and input handling come for free. A static_assert catches it if the enum and table ever drift out of sync.

Trolls don't touch the clock state directly. They write to a set of hooks — fake times, per-element pixel offsets, layout flags — that get reset centrally whenever an event ends. That's what stops a cancelled or interrupted troll from leaving the clock in a broken state.

Animation is stateless. Every draw call works out where it is in the sequence as a fraction from 0 to 1 and renders from that alone — nothing increments a frame counter. That means frame rate doesn't matter, there's no state to clean up, and frame 900 renders correctly without stepping through the 899 before it.

Bluetooth and the sound engine fight over the same I2S peripheral, since only one driver can hold it at a time. Switching to speaker mode tears down the sound engine's driver and hands the pins to the A2DP sink; switching back reinstalls it, and an alarm that fires mid-Bluetooth-playback does that handoff live. Getting the Bluetooth stack to fit at all meant giving up the second OTA slot for a custom partition table.

It's a clock, so it has to behave like one even when things go wrong. It comes up on the RTC's last known time if there's no network at boot. Buttons are interrupt-driven with debounce handled in the ISR itself. State transitions are owned by a single file instead of being triggered piecemeal from whatever module wants one. If nobody touches it for a few days, it dims the display and suspends every troll until someone interacts with it again.

Every tunable value — pins, timeouts, night-mode hours, sound files and volumes, per-troll trigger odds, fish swim speed, blink frequency — lives in one config file, so behavior can be adjusted without touching the logic anywhere else.

Designing and printing the case was one of the most time consuming parts of the whole project. The measurements are extremely precise and it took multiple renditions to perfectly narrow down the design. The STL file is shown below.

Stack: C++ on ESP32 / Arduino via PlatformIO · U8g2 · ESP8266Audio · ESP32-A2DP · ArduinoJson · LittleFS · NVS · open-meteo

---

## Hardware

ESP32, SSD1309 128×64 SPI OLED, DS3231 RTC, rotary encoder, three buttons, and a MAX98357A I2S amplifier driving a small speaker.

- **Parts list:** _(link coming)_
- **Printable enclosure (STL):**
[![Enclosure preview](https://cdn.thingiverse.com/assets/.../preview.jpg)](https://www.thingiverse.com/thing:7395218)

Alternate download:
[Download the case STL](docs/WorstAlarmClockEver.stl)


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
