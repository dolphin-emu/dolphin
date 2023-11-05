#include "Core/Scripting/HelperClasses/GCButton_API_Implementations.h"

#include <string_view>
#include "Common/StringUtil.h"
#include "Core/Scripting/CoreScriptInterface/Enums/GCButtonNameEnum.h"
#include "string.h"

namespace Scripting
{

int ParseGCButton_impl(const char* button_name)
{
  if (button_name == nullptr)
    return static_cast<int>(GCButtonNameEnum::UnknownButton);

  size_t button_string_length = strlen(button_name);

  if (button_string_length == 0)
    return static_cast<int>(GCButtonNameEnum::UnknownButton);

  if (button_string_length == 1)
  {
    switch (button_name[0])
    {
    case 'a':
    case 'A':
      return static_cast<int>(GCButtonNameEnum::A);

    case 'b':
    case 'B':
      return static_cast<int>(GCButtonNameEnum::B);

    case 'x':
    case 'X':
      return static_cast<int>(GCButtonNameEnum::X);

    case 'y':
    case 'Y':
      return static_cast<int>(GCButtonNameEnum::Y);

    case 'z':
    case 'Z':
      return static_cast<int>(GCButtonNameEnum::Z);

    case 'l':
    case 'L':
      return static_cast<int>(GCButtonNameEnum::L);

    case 'r':
    case 'R':
      return static_cast<int>(GCButtonNameEnum::R);

    default:
      return static_cast<int>(GCButtonNameEnum::UnknownButton);
    }
  }

  switch (button_name[0])
  {
  case 'd':
  case 'D':
    if (Common::CaseInsensitiveEquals(button_name, "dPadUp"))
      return static_cast<int>(GCButtonNameEnum::DPadUp);

    else if (Common::CaseInsensitiveEquals(button_name, "dPadDown"))
      return static_cast<int>(GCButtonNameEnum::DPadDown);

    else if (Common::CaseInsensitiveEquals(button_name, "dPadLeft"))
      return static_cast<int>(GCButtonNameEnum::DPadLeft);

    else if (Common::CaseInsensitiveEquals(button_name, "dPadRight"))
      return static_cast<int>(GCButtonNameEnum::DPadRight);

    else if (Common::CaseInsensitiveEquals(button_name, "disc"))
      return static_cast<int>(GCButtonNameEnum::Disc);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 'a':
  case 'A':
    if (Common::CaseInsensitiveEquals(button_name, "analogStickX"))
      return static_cast<int>(GCButtonNameEnum::AnalogStickX);

    else if (Common::CaseInsensitiveEquals(button_name, "analogStickY"))
      return static_cast<int>(GCButtonNameEnum::AnalogStickY);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 'c':
  case 'C':
    if (Common::CaseInsensitiveEquals(button_name, "cStickX"))
      return static_cast<int>(GCButtonNameEnum::CStickX);

    else if (Common::CaseInsensitiveEquals(button_name, "cStickY"))
      return static_cast<int>(GCButtonNameEnum::CStickY);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 't':
  case 'T':
    if (Common::CaseInsensitiveEquals(button_name, "triggerL"))
      return static_cast<int>(GCButtonNameEnum::TriggerL);

    else if (Common::CaseInsensitiveEquals(button_name, "triggerR"))
      return static_cast<int>(GCButtonNameEnum::TriggerR);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 's':
  case 'S':
    if (Common::CaseInsensitiveEquals(button_name, "start"))
      return static_cast<int>(GCButtonNameEnum::Start);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 'r':
  case 'R':
    if (Common::CaseInsensitiveEquals(button_name, "reset"))
      return static_cast<int>(GCButtonNameEnum::Reset);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 'g':
  case 'G':
    if (Common::CaseInsensitiveEquals(button_name, "getOrigin"))
      return static_cast<int>(GCButtonNameEnum::GetOrigin);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  case 'i':
  case 'I':
    if (Common::CaseInsensitiveEquals(button_name, "isConnected"))
      return static_cast<int>(GCButtonNameEnum::IsConnected);

    else
      return static_cast<int>(GCButtonNameEnum::UnknownButton);

  default:
    return static_cast<int>(GCButtonNameEnum::UnknownButton);
  }
}

const char* ConvertButtonEnumToString_impl(int button)
{
  switch (static_cast<GCButtonNameEnum>(button))
  {
  case GCButtonNameEnum::A:
    return "A";

  case GCButtonNameEnum::B:
    return "B";

  case GCButtonNameEnum::X:
    return "X";

  case GCButtonNameEnum::Y:
    return "Y";

  case GCButtonNameEnum::Z:
    return "Z";

  case GCButtonNameEnum::L:
    return "L";

  case GCButtonNameEnum::R:
    return "R";

  case GCButtonNameEnum::Start:
    return "Start";

  case GCButtonNameEnum::Reset:
    return "Reset";

  case GCButtonNameEnum::DPadUp:
    return "dPadUp";

  case GCButtonNameEnum::DPadDown:
    return "dPadDown";

  case GCButtonNameEnum::DPadLeft:
    return "dPadLeft";

  case GCButtonNameEnum::DPadRight:
    return "dPadRight";

  case GCButtonNameEnum::TriggerL:
    return "triggerL";

  case GCButtonNameEnum::TriggerR:
    return "triggerR";

  case GCButtonNameEnum::AnalogStickX:
    return "analogStickX";

  case GCButtonNameEnum::AnalogStickY:
    return "analogStickY";

  case GCButtonNameEnum::CStickX:
    return "cStickX";

  case GCButtonNameEnum::CStickY:
    return "cStickY";

  case GCButtonNameEnum::Disc:
    return "disc";

  case GCButtonNameEnum::GetOrigin:
    return "getOrigin";

  case GCButtonNameEnum::IsConnected:
    return "isConnected";

  default:
    return "";
  }
}

int IsValidButtonEnum_impl(int button)
{
  if (button >= 0 && button < static_cast<int>(GCButtonNameEnum::UnknownButton))
    return 1;
  else
    return 0;
}

int IsDigitalButton_impl(int raw_button_val)
{
  GCButtonNameEnum button_name = static_cast<GCButtonNameEnum>(raw_button_val);

  switch (button_name)
  {
  case GCButtonNameEnum::A:
  case GCButtonNameEnum::B:
  case GCButtonNameEnum::Disc:
  case GCButtonNameEnum::DPadDown:
  case GCButtonNameEnum::DPadLeft:
  case GCButtonNameEnum::DPadRight:
  case GCButtonNameEnum::DPadUp:
  case GCButtonNameEnum::GetOrigin:
  case GCButtonNameEnum::IsConnected:
  case GCButtonNameEnum::L:
  case GCButtonNameEnum::R:
  case GCButtonNameEnum::Reset:
  case GCButtonNameEnum::Start:
  case GCButtonNameEnum::X:
  case GCButtonNameEnum::Y:
  case GCButtonNameEnum::Z:
    return 1;

  default:
    return 0;
  }
}
int IsAnalogButton_impl(int raw_button_val)
{
  GCButtonNameEnum button_name = static_cast<GCButtonNameEnum>(raw_button_val);

  switch (button_name)
  {
  case GCButtonNameEnum::AnalogStickX:
  case GCButtonNameEnum::AnalogStickY:
  case GCButtonNameEnum::CStickX:
  case GCButtonNameEnum::CStickY:
  case GCButtonNameEnum::TriggerL:
  case GCButtonNameEnum::TriggerR:
    return 1;

  default:
    return 0;
  }
}
}  // namespace Scripting
