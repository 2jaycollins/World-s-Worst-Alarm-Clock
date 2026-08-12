// ============================================================================
//  globals.h -- debug output, included by everything.
//
//  DEBUG_MODE is defined in platformio.ini's build_flags. Remove it there and
//  every print below compiles away to nothing, which is what you want for a
//  clock that has to live on a bedside table rather than a USB cable.
// ============================================================================

#ifndef GLOBALS_H
#define GLOBALS_H

#include "values.h"

#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x)         Serial.print(x)
  #define DEBUG_PRINTLN(x)       Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#endif

#endif
