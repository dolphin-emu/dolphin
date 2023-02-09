#include "LuaGCButtons.h"


bool isBinaryButton(GcButtonName button)
{
  return (button == GcButtonName::A || button == GcButtonName::B || button == GcButtonName::X || button == GcButtonName::Y ||
    button == GcButtonName::Z || button == GcButtonName::L || button == GcButtonName::R || button == GcButtonName::Start ||
    button == GcButtonName::Reset || button == GcButtonName::DPadUp || button == GcButtonName::DPadDown || button == GcButtonName::DPadLeft || button == GcButtonName::DPadRight);
}

bool isAnalogButton(GcButtonName button)
{
  return (button == GcButtonName::AnalogStickX || button == GcButtonName::AnalogStickY ||
          button == GcButtonName::CStickX || button == GcButtonName::CStickY ||
          button == GcButtonName::TriggerL || button == GcButtonName::TriggerR);
}
bool isEqualIgnoreCase(const char* string1, const char* string2)
{
  int index = 0;
  while (string1[index] != '\0')
  {
    if (string1[index] != string2[index])
    {
      char firstChar = string1[index];
      if (firstChar >= 65 && firstChar <= 90)
        firstChar += 32;
      char secondChar = string2[index];
      if (secondChar >= 65 && secondChar <= 90)
        secondChar += 32;
      if (firstChar != secondChar)
        return false;
    }
    ++index;
  }

  return string2[index] == '\0';
}

GcButtonName parseGCButton(const char* buttonName)
{
  if (strlen(buttonName) == 1)
  {
    switch (buttonName[0])
    {
    case 'a':
    case 'A':
      return GcButtonName::A;

    case 'b':
    case 'B':
      return GcButtonName::B;

    case 'x':
    case 'X':
      return GcButtonName::X;

    case 'y':
    case 'Y':
      return GcButtonName::Y;

    case 'z':
    case 'Z':
      return GcButtonName::Z;

    case 'l':
    case 'L':
      return GcButtonName::L;

    case 'r':
    case 'R':
      return GcButtonName::R;

    default:
      return GcButtonName::Unknown;
    }
  }

  switch (buttonName[0])
  {
  case 'd':
  case 'D':
    if (isEqualIgnoreCase(buttonName, "dPadUp"))
      return GcButtonName::DPadUp;
    else if (isEqualIgnoreCase(buttonName, "dPadDown"))
      return GcButtonName::DPadDown;
    else if (isEqualIgnoreCase(buttonName, "dPadLeft"))
      return GcButtonName::DPadLeft;
    else if (isEqualIgnoreCase(buttonName, "dPadRight"))
      return GcButtonName::DPadRight;
    else
      return GcButtonName::Unknown;

  case 'a':
  case 'A':
    if (isEqualIgnoreCase(buttonName, "analogStickX"))
      return GcButtonName::AnalogStickX;
    else if (isEqualIgnoreCase(buttonName, "analogStickY"))
      return GcButtonName::AnalogStickY;
    else
      return GcButtonName::Unknown;

  case 'c':
  case 'C':
    if (isEqualIgnoreCase(buttonName, "cStickX"))
      return GcButtonName::CStickX;
    else if (isEqualIgnoreCase(buttonName, "cStickY"))
      return GcButtonName::CStickY;
    else
      return GcButtonName::Unknown;

  case 't':
  case 'T':
    if (isEqualIgnoreCase(buttonName, "triggerL"))
      return GcButtonName::TriggerL;
    else if (isEqualIgnoreCase(buttonName, "triggerR"))
      return GcButtonName::TriggerR;
    else
      return GcButtonName::Unknown;

  case 'r':
  case 'R':
    if (isEqualIgnoreCase(buttonName, "reset"))
      return GcButtonName::Reset;
    else
      return GcButtonName::Unknown;

  case 's':
  case 'S':
    if (isEqualIgnoreCase(buttonName, "start"))
      return GcButtonName::Start;
    else
      return GcButtonName::Unknown;

  default:
    return GcButtonName::Unknown;
  }
}

  const char* convertButtonEnumToString(GcButtonName button)
  {
    switch (button)
    {
    case GcButtonName::A:
      return "A";
    case GcButtonName::B:
      return "B";
    case GcButtonName::X:
      return "X";
    case GcButtonName::Y:
      return "Y";
    case GcButtonName::Z:
      return "Z";
    case GcButtonName::L:
      return "L";
    case GcButtonName::R:
      return "R";
    case GcButtonName::Start:
      return "START";
    case GcButtonName::Reset:
      return "RESET";
    case GcButtonName::DPadUp:
      return "dPadUp";
    case GcButtonName::DPadDown:
      return "dPadDown";
    case GcButtonName::DPadLeft:
      return "dPadLeft";
    case GcButtonName::DPadRight:
      return "dPadRight";
    case GcButtonName::TriggerL:
      return "triggerL";
    case GcButtonName::TriggerR:
      return "triggerR";
    case GcButtonName::AnalogStickX:
      return "analogStickX";
    case GcButtonName::AnalogStickY:
      return "analogStickY";
    case GcButtonName::CStickX:
      return "cStickX";
    case GcButtonName::CStickY:
      return "cStickY";
    default:
      return "";
    }
  }
