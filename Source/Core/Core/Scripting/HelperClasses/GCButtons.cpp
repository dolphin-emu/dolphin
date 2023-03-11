#include "Core/Scripting/HelperClasses/GCButtons.h"

static bool IsEqualIgnoreCase(const char* string_1, const char* string_2)
{
  int index = 0;
  while (string_1[index] != '\0')
  {
    if (string_1[index] != string_2[index])
    {
      char first_char = string_1[index];
      if (first_char >= 65 && first_char <= 90)
        first_char += 32;
      char second_char = string_2[index];
      if (second_char >= 65 && second_char <= 90)
        second_char += 32;
      if (first_char != second_char)
        return false;
    }
    ++index;
  }

  return string_2[index] == '\0';
}

GcButtonName ParseGCButton(const char* button_name)
{
  if (strlen(button_name) == 1)
  {
    switch (button_name[0])
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

  switch (button_name[0])
  {
  case 'd':
  case 'D':
    if (IsEqualIgnoreCase(button_name, "dPadUp"))
      return GcButtonName::DPadUp;
    else if (IsEqualIgnoreCase(button_name, "dPadDown"))
      return GcButtonName::DPadDown;
    else if (IsEqualIgnoreCase(button_name, "dPadLeft"))
      return GcButtonName::DPadLeft;
    else if (IsEqualIgnoreCase(button_name, "dPadRight"))
      return GcButtonName::DPadRight;
    else
      return GcButtonName::Unknown;

  case 'a':
  case 'A':
    if (IsEqualIgnoreCase(button_name, "analogStickX"))
      return GcButtonName::AnalogStickX;
    else if (IsEqualIgnoreCase(button_name, "analogStickY"))
      return GcButtonName::AnalogStickY;
    else
      return GcButtonName::Unknown;

  case 'c':
  case 'C':
    if (IsEqualIgnoreCase(button_name, "cStickX"))
      return GcButtonName::CStickX;
    else if (IsEqualIgnoreCase(button_name, "cStickY"))
      return GcButtonName::CStickY;
    else
      return GcButtonName::Unknown;

  case 't':
  case 'T':
    if (IsEqualIgnoreCase(button_name, "triggerL"))
      return GcButtonName::TriggerL;
    else if (IsEqualIgnoreCase(button_name, "triggerR"))
      return GcButtonName::TriggerR;
    else
      return GcButtonName::Unknown;

  case 'r':
  case 'R':
    if (IsEqualIgnoreCase(button_name, "reset"))
      return GcButtonName::Reset;
    else
      return GcButtonName::Unknown;

  case 's':
  case 'S':
    if (IsEqualIgnoreCase(button_name, "start"))
      return GcButtonName::Start;
    else
      return GcButtonName::Unknown;

  default:
    return GcButtonName::Unknown;
  }
}

const char* ConvertButtonEnumToString(GcButtonName button)
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
    return "Start";
  case GcButtonName::Reset:
    return "Reset";
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
