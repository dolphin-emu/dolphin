#pragma once
#include <vector>
#include "string.h"

enum class GcButtonName
{
  A,
  B,
  X,
  Y,
  Z,
  L,
  R,
  DPadUp,
  DPadDown,
  DPadLeft,
  DPadRight,
  AnalogStickX,
  AnalogStickY,
  CStickX,
  CStickY,
  TriggerL,
  TriggerR,
  Start,
  Reset,
  Unknown
};

extern GcButtonName ParseGCButton(const char* button_name);
extern const char* ConvertButtonEnumToString(GcButtonName button);
extern std::vector<GcButtonName> GetListOfAllButtons();
