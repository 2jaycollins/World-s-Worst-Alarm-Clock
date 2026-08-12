// ============================================================================
//  bluetooth.h -- Bluetooth speaker mode.
//
//  Turns the clock into an A2DP audio sink: pair a phone with it and it plays
//  through the same speaker the alarm uses. Entered from the main menu.
//
//  While it is active the clock still shows the time and the alarm indicator,
//  the three front buttons become media controls, and the encoder adjusts the
//  volume or opens the disconnect prompt.
//
//  Two things have to be handed over on the way in, and back on the way out:
//  the I2S peripheral (only one driver can own it) and the 2.4GHz radio (WiFi
//  is switched off, so the weather refresh is suspended for the duration).
//
//  The device name, volume behaviour and marquee speed are in values.h.
// ============================================================================

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>

// True while Bluetooth mode is active. Read by clock_logic, to skip the
// blocking weather refresh, and by the app loop for the alarm override.
extern bool bluetoothActive;

// Enter: WiFi off, hand the I2S to the A2DP sink, start advertising.
void enterBluetoothMode();

// Exit: stop A2DP, take the I2S back, and optionally re-enable WiFi. The alarm
// path passes false -- when a ring interrupts Bluetooth mode, getting the sound
// engine back matters and the weather does not.
void exitBluetoothMode(bool reconnectWifi = true);

// Called by the app loop while Bluetooth mode has focus.
void drawBluetoothScreen();
void handleBluetoothButtons(bool snoozeClick, bool alarmClick,
                            bool brightClick, bool encoderClick);
void bluetoothEncoderRotate(int dir);   // +1 clockwise (louder), -1 anticlockwise

#endif
