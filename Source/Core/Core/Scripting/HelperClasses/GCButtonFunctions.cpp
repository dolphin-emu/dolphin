#include "Core/Scripting/HelperClasses/GCButtonFunctions.h"

#include "Core/Scripting/CoreScriptContextFiles/Enums/GCButtonNameEnum.h"

#include "string.h"

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

int ParseGCButton_impl(const char* button_name)
{
  if (strlen(button_name) == 1)
  {
    switch (button_name[0])
    {
    case 'a':
    case 'A':
      return (int)ScriptingEnums::GcButtonNameEnum::A;

    case 'b':
    case 'B':
      return (int)ScriptingEnums::GcButtonNameEnum::B;

    case 'x':
    case 'X':
      return (int)ScriptingEnums::GcButtonNameEnum::X;

    case 'y':
    case 'Y':
      return (int)ScriptingEnums::GcButtonNameEnum::Y;

    case 'z':
    case 'Z':
      return (int)ScriptingEnums::GcButtonNameEnum::Z;

    case 'l':
    case 'L':
      return (int)ScriptingEnums::GcButtonNameEnum::L;

    case 'r':
    case 'R':
      return (int)ScriptingEnums::GcButtonNameEnum::R;

    default:
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;
    }
  }

  switch (button_name[0])
  {
  case 'd':
  case 'D':
    if (IsEqualIgnoreCase(button_name, "dPadUp"))
      return (int)ScriptingEnums::GcButtonNameEnum::DPadUp;

    else if (IsEqualIgnoreCase(button_name, "dPadDown"))
      return (int)ScriptingEnums::GcButtonNameEnum::DPadDown;

    else if (IsEqualIgnoreCase(button_name, "dPadLeft"))
      return (int)ScriptingEnums::GcButtonNameEnum::DPadLeft;

    else if (IsEqualIgnoreCase(button_name, "dPadRight"))
      return (int)ScriptingEnums::GcButtonNameEnum::DPadRight;

    else if (IsEqualIgnoreCase(button_name, "disc"))
      return (int)ScriptingEnums::GcButtonNameEnum::Disc;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 'a':
  case 'A':
    if (IsEqualIgnoreCase(button_name, "analogStickX"))
      return (int)ScriptingEnums::GcButtonNameEnum::AnalogStickX;

    else if (IsEqualIgnoreCase(button_name, "analogStickY"))
      return (int)ScriptingEnums::GcButtonNameEnum::AnalogStickY;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 'c':
  case 'C':
    if (IsEqualIgnoreCase(button_name, "cStickX"))
      return (int)ScriptingEnums::GcButtonNameEnum::CStickX;

    else if (IsEqualIgnoreCase(button_name, "cStickY"))
      return (int)ScriptingEnums::GcButtonNameEnum::CStickY;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 't':
  case 'T':
    if (IsEqualIgnoreCase(button_name, "triggerL"))
      return (int)ScriptingEnums::GcButtonNameEnum::TriggerL;

    else if (IsEqualIgnoreCase(button_name, "triggerR"))
      return (int)ScriptingEnums::GcButtonNameEnum::TriggerR;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 'r':
  case 'R':
    if (IsEqualIgnoreCase(button_name, "reset"))
      return (int)ScriptingEnums::GcButtonNameEnum::Reset;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 's':
  case 'S':
    if (IsEqualIgnoreCase(button_name, "start"))
      return (int)ScriptingEnums::GcButtonNameEnum::Start;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 'g':
  case 'G':
    if (IsEqualIgnoreCase(button_name, "getOrigin"))
      return (int)ScriptingEnums::GcButtonNameEnum::GetOrigin;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  case 'i':
  case 'I':
    if (IsEqualIgnoreCase(button_name, "isConnected"))
      return (int)ScriptingEnums::GcButtonNameEnum::IsConnected;

    else
      return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;

  default:
    return (int)ScriptingEnums::GcButtonNameEnum::UnknownButton;
  }
}

const char* ConvertButtonEnumToString_impl(int button)
{
  switch (button)
  {
  case ScriptingEnums::GcButtonNameEnum::A:
    return "A";

  case ScriptingEnums::GcButtonNameEnum::B:
    return "B";

  case ScriptingEnums::GcButtonNameEnum::X:
    return "X";

  case ScriptingEnums::GcButtonNameEnum::Y:
    return "Y";

  case ScriptingEnums::GcButtonNameEnum::Z:
    return "Z";

  case ScriptingEnums::GcButtonNameEnum::L:
    return "L";

  case ScriptingEnums::GcButtonNameEnum::R:
    return "R";

  case ScriptingEnums::GcButtonNameEnum::Start:
    return "Start";

  case ScriptingEnums::GcButtonNameEnum::Reset:
    return "Reset";

  case ScriptingEnums::GcButtonNameEnum::DPadUp:
    return "dPadUp";

  case ScriptingEnums::GcButtonNameEnum::DPadDown:
    return "dPadDown";

  case ScriptingEnums::GcButtonNameEnum::DPadLeft:
    return "dPadLeft";

  case ScriptingEnums::GcButtonNameEnum::DPadRight:
    return "dPadRight";

  case ScriptingEnums::GcButtonNameEnum::TriggerL:
    return "triggerL";

  case ScriptingEnums::GcButtonNameEnum::TriggerR:
    return "triggerR";

  case ScriptingEnums::GcButtonNameEnum::AnalogStickX:
    return "analogStickX";

  case ScriptingEnums::GcButtonNameEnum::AnalogStickY:
    return "analogStickY";

  case ScriptingEnums::GcButtonNameEnum::CStickX:
    return "cStickX";

  case ScriptingEnums::GcButtonNameEnum::CStickY:
    return "cStickY";

  case ScriptingEnums::GcButtonNameEnum::Disc:
    return "disc";

  case ScriptingEnums::GcButtonNameEnum::GetOrigin:
    return "getOrigin";

  case ScriptingEnums::GcButtonNameEnum::IsConnected:
    return "isConnected";

  default:
    return "";
  }
}

int IsValidButtonEnum_impl(int button)
{
  if (button >= 0 && button < ScriptingEnums::GcButtonNameEnum::UnknownButton)
    return 1;
  else
    return 0;
}

int IsDigitalButton_impl(int raw_button_val)
{
  ScriptingEnums::GcButtonNameEnum button_name = (ScriptingEnums::GcButtonNameEnum)raw_button_val;

  switch (button_name)
  {
  case ScriptingEnums::GcButtonNameEnum::A:
  case ScriptingEnums::GcButtonNameEnum::B:
  case ScriptingEnums::GcButtonNameEnum::Disc:
  case ScriptingEnums::GcButtonNameEnum::DPadDown:
  case ScriptingEnums::GcButtonNameEnum::DPadLeft:
  case ScriptingEnums::GcButtonNameEnum::DPadRight:
  case ScriptingEnums::GcButtonNameEnum::DPadUp:
  case ScriptingEnums::GcButtonNameEnum::GetOrigin:
  case ScriptingEnums::GcButtonNameEnum::IsConnected:
  case ScriptingEnums::GcButtonNameEnum::L:
  case ScriptingEnums::GcButtonNameEnum::R:
  case ScriptingEnums::GcButtonNameEnum::Reset:
  case ScriptingEnums::GcButtonNameEnum::Start:
  case ScriptingEnums::GcButtonNameEnum::X:
  case ScriptingEnums::GcButtonNameEnum::Y:
  case ScriptingEnums::GcButtonNameEnum::Z:
    return 1;

  default:
    return 0;
  }
}
int IsAnalogButton_impl(int raw_button_val)
{
  ScriptingEnums::GcButtonNameEnum button_name = (ScriptingEnums::GcButtonNameEnum)raw_button_val;

  switch (button_name)
  {
  case ScriptingEnums::GcButtonNameEnum::AnalogStickX:
  case ScriptingEnums::GcButtonNameEnum::AnalogStickY:
  case ScriptingEnums::GcButtonNameEnum::CStickX:
  case ScriptingEnums::GcButtonNameEnum::CStickY:
  case ScriptingEnums::GcButtonNameEnum::TriggerL:
  case ScriptingEnums::GcButtonNameEnum::TriggerR:
    return 1;

  default:
    return 0;
  }
}
