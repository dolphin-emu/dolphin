#pragma once
#include "string.h"

enum class GcButtonName
{
  A, B, X, Y, Z, L, R, DPadUp, DPadDown, DPadLeft, DPadRight, AnalogStickX, AnalogStickY, CStickX, CStickY, TriggerL, TriggerR, Start, Reset, Unknown
};

extern bool IsBinaryButton(GcButtonName button);
extern bool IsAnalogButton(GcButtonName button);
extern bool IsEqualIgnoreCase(const char* string_1, const char* string_2);
extern GcButtonName ParseGCButton(const char* button_name);
extern const char* ConvertButtonEnumToString(GcButtonName button);
