#pragma once
#include "string.h"

enum class GcButtonName
{
  A, B, X, Y, Z, L, R, DPadUp, DPadDown, DPadLeft, DPadRight, AnalogStickX, AnalogStickY, CStickX, CStickY, TriggerL, TriggerR, Start, Reset, Unknown
};

extern bool isBinaryButton(GcButtonName button);
extern bool isAnalogButton(GcButtonName button);
extern bool isEqualIgnoreCase(const char* string1, const char* string2);
extern GcButtonName parseGCButton(const char* buttonName);
extern const char* convertButtonEnumToString(GcButtonName button);
