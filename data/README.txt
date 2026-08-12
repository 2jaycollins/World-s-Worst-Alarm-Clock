data/  -> LittleFS filesystem image on the ESP32
================================================

Drop your sound files in THIS folder, then upload them to the board with:

    PlatformIO: Project Tasks -> esp32dev -> Platform -> Build Filesystem Image
    PlatformIO: Project Tasks -> esp32dev -> Platform -> Upload Filesystem Image

(or from the CLI:  pio run -t buildfs   then   pio run -t uploadfs )

This is a SEPARATE upload from your normal code upload. Re-run "Upload
Filesystem Image" whenever you add or change a sound.

Play a file from code with a leading slash. The file names all live in
include/values.h -- add yours there rather than hardcoding a path:

    playSound(SND_BEEP);


WAV or MP3?
-----------
Both work. The decoder is picked by the file extension: ".mp3" decodes as MP3,
anything else is treated as WAV.

- WAV is instant, with no decode latency. Use it for short effects: beeps,
  boings, clicks.
- MP3 is far smaller. Use it for anything long: speech, ambience, music.

WAV format requirements (ESP8266Audio AudioGeneratorWAV)
--------------------------------------------------------
- Container : WAV (RIFF), PCM uncompressed (NOT adpcm/float)
- Bit depth : 16-bit (or 8-bit) PCM
- Channels  : mono preferred (output is mono anyway)
- Sample rate: 16000-22050 Hz recommended. 44100 works but is heavier on the
  CPU and on flash space.

Convert anything to a compatible WAV with ffmpeg:

    ffmpeg -i input.mp3 -ac 1 -ar 22050 -sample_fmt s16 beep.wav

...or to a small mono MP3:

    ffmpeg -i input.wav -ac 1 -ar 22050 -b:a 64k output.mp3

Space
-----
The LittleFS partition is 1.875 MB (see partitions_bt.csv). That is a lot of
22 kHz mono audio, but it is not unlimited -- keep an eye on it if you start
adding music.

Output path / hardware
----------------------
Audio leaves the ESP32 over I2S on the pins set in include/values.h
(PIN_AUDIO_DOUT / BCLK / WCLK) into an I2S amplifier. See README.md for wiring.
