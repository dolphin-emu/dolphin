#pragma once
#include "string.h"

enum GC_BUTTON_NAME
{
  A,
  B,
  X,
  Y,
  Z,
  L,
  R,
  dPadUp,
  dPadDown,
  dPadLeft,
  dPadRight,
  analogStickX,
  analogStickY,
  cStickX,
  cStickY,
  triggerL,
  triggerR,
  START,
  RESET,
  UNKNOWN
};

extern bool isBinaryButton(GC_BUTTON_NAME button);
extern bool isAnalogButton(GC_BUTTON_NAME button);
extern bool isEqualIgnoreCase(const char* string1, const char* string2);
extern GC_BUTTON_NAME parseGCButton(const char* buttonName);
extern const char* convertButtonEnumToString(GC_BUTTON_NAME button);
