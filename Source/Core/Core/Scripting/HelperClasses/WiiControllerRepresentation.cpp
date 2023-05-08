#include "Core/Scripting/HelperClasses/WiiControllerRepresentation.h"
namespace WiiControllerEnums
{

static bool IsEqualIgnoringCase(const char* str1, const char* str2)
{
  int index = 0;
  while (str1[index] != '\0')
  {
    if (str1[index] != str2[index])
    {
      char first_char = str1[index];
      if (first_char >= 65 && first_char <= 90)
        first_char += 32;
      char second_char = str2[index];
      if (second_char >= 65 && second_char <= 90)
        second_char += 32;
      if (first_char != second_char)
        return false;
    }
    ++index;
  }

  return str2[index] == '\0';
}
WiimoteButtonsEnum ParseWiimoteButton(const char* button_name)
{
  if (button_name == nullptr || button_name[0] == '\0')
    return WiimoteButtonsEnum::Unknown;

  if (button_name[1] == '\0')  // Case where String is 1 character long
  {
    switch (button_name[0])
    {
    case 'a':
    case 'A':
      return WiimoteButtonsEnum::A;
    case 'b':
    case 'B':
      return WiimoteButtonsEnum::B;
    case '+':
      return WiimoteButtonsEnum::Plus;
    case '-':
      return WiimoteButtonsEnum::Minus;
    case '1':
      return WiimoteButtonsEnum::One;
    case '2':
      return WiimoteButtonsEnum::Two;
    default:
      return WiimoteButtonsEnum::Unknown;
    }
  }
  else if (IsEqualIgnoringCase(button_name, "dPadUp"))
    return WiimoteButtonsEnum::DPadUp;
  else if (IsEqualIgnoringCase(button_name, "dPadDown"))
    return WiimoteButtonsEnum::DPadDown;
  else if (IsEqualIgnoringCase(button_name, "dPadLeft"))
    return WiimoteButtonsEnum::DPadLeft;
  else if (IsEqualIgnoringCase(button_name, "dPadRight"))
    return WiimoteButtonsEnum::DPadRight;

  else if (IsEqualIgnoringCase(button_name, "HOME"))
    return WiimoteButtonsEnum::Home;
  else if (IsEqualIgnoringCase(button_name, "ACCELERATION"))
    return WiimoteButtonsEnum::Acceleration;

  else
    return WiimoteButtonsEnum::Unknown;
}

const char* ConvertWiimoteButtonToString(WiimoteButtonsEnum button)
{
  switch (button)
  {
  case WiimoteButtonsEnum::DPadUp:
    return "dPadUp";
  case WiimoteButtonsEnum::DPadDown:
    return "dPadDown";
  case WiimoteButtonsEnum::DPadLeft:
    return "dPadLeft";
  case WiimoteButtonsEnum::DPadRight:
    return "dPadRight";
  case WiimoteButtonsEnum::A:
    return "A";
  case WiimoteButtonsEnum::B:
    return "B";
  case WiimoteButtonsEnum::Plus:
    return "+";
  case WiimoteButtonsEnum::Minus:
    return "-";
  case WiimoteButtonsEnum::One:
    return "1";
  case WiimoteButtonsEnum::Two:
    return "2";
  case WiimoteButtonsEnum::Home:
    return "Home";
  case WiimoteButtonsEnum::Acceleration:
    return "Acceleration";
  default:
    return "";
  }
}

AccelerationEnum ParseAccelerationInput(const char* input_name)
{
  if (input_name == nullptr || input_name[0] == '\0' || input_name[1] != '\0')
    return AccelerationEnum::Unknown;

  switch (input_name[0])
  {
  case 'x':
  case 'X':
    return AccelerationEnum::X;

  case 'y':
  case 'Y':
    return AccelerationEnum::Y;

  case 'z':
  case 'Z':
    return AccelerationEnum::Z;

  default:
    return AccelerationEnum::Unknown;
  }
}

const char* ConvertAccelerationInputToString(AccelerationEnum input)
{
  switch (input)
  {
  case AccelerationEnum::X:
    return "X";
  case AccelerationEnum::Y:
    return "Y";
  case AccelerationEnum::Z:
    return "Z";
  default:
    return "";
  }
}

IREnum ParseIRInput(const char* input_name)
{
  if (input_name == nullptr || input_name[0] == '\0' || input_name[1] != '\0')
    return IREnum::Unknown;

  switch (input_name[0])
  {
  case 'x':
  case 'X':
    return IREnum::X;

  case 'y':
  case 'Y':
    return IREnum::Y;

  default:
    return IREnum::Unknown;
  }
}

const char* ConvertIRInputToString(IREnum input)
{
  switch (input)
  {
  case IREnum::X:
    return "X";
  case IREnum::Y:
    return "Y";
  default:
    return "";
  }
}

NunchuckButtonsEnum ParseNunchuckButton(const char* button_name)
{
  if (button_name == nullptr || button_name[0] == '\0')
    return NunchuckButtonsEnum::Unknown;

  if (IsEqualIgnoringCase(button_name, "C"))
    return NunchuckButtonsEnum::C;
  else if (IsEqualIgnoringCase(button_name, "Z"))
    return NunchuckButtonsEnum::Z;
  else if (IsEqualIgnoringCase(button_name, "controlStickX"))
    return NunchuckButtonsEnum::ControlStickX;
  else if (IsEqualIgnoringCase(button_name, "controlStickY"))
    return NunchuckButtonsEnum::ControlStickY;
  else if (IsEqualIgnoringCase(button_name, "Acceleration"))
    return NunchuckButtonsEnum::Acceleration;
  else
    return NunchuckButtonsEnum::Unknown;
}

const char* ConvertNunchuckButtonToString(NunchuckButtonsEnum button)
{
  switch (button)
  {
  case NunchuckButtonsEnum::C:
    return "C";
  case NunchuckButtonsEnum::Z:
    return "Z";
  case NunchuckButtonsEnum::ControlStickX:
    return "controlStickX";
  case NunchuckButtonsEnum::ControlStickY:
    return "controlStickY";
  case NunchuckButtonsEnum::Acceleration:
    return "Acceleration";
  default:
    return "";
  }
}

ClassicControllerButtonsEnum ParseClassicControllerButton(const char* button_name)
{
  if (button_name == nullptr || button_name[0] == '\0')
    return ClassicControllerButtonsEnum::Unknown;

  if (button_name[1] == '\0')  // case where string is 1 char
  {
    switch (button_name[0])
    {
    case 'a':
    case 'A':
      return ClassicControllerButtonsEnum::A;
    case 'b':
    case 'B':
      return ClassicControllerButtonsEnum::B;
    case 'x':
    case 'X':
      return ClassicControllerButtonsEnum::X;
    case 'y':
    case 'Y':
      return ClassicControllerButtonsEnum::Y;
    default:
      return ClassicControllerButtonsEnum::Unknown;
    }
  }

  else if (IsEqualIgnoringCase(button_name, "YL"))
    return ClassicControllerButtonsEnum::YL;
  else if (IsEqualIgnoringCase(button_name, "ZR"))
    return ClassicControllerButtonsEnum::ZR;

  else if (IsEqualIgnoringCase(button_name, "dPadUp"))
    return ClassicControllerButtonsEnum::DPadUp;
  else if (IsEqualIgnoringCase(button_name, "dPadDown"))
    return ClassicControllerButtonsEnum::DPadDown;
  else if (IsEqualIgnoringCase(button_name, "dPadLeft"))
    return ClassicControllerButtonsEnum::DPadLeft;
  else if (IsEqualIgnoringCase(button_name, "dPadRight"))
    return ClassicControllerButtonsEnum::DPadRight;

  else if (IsEqualIgnoringCase(button_name, "Home"))
    return ClassicControllerButtonsEnum::Home;

  else if (IsEqualIgnoringCase(button_name, "+"))
    return ClassicControllerButtonsEnum::Plus;
  else if (IsEqualIgnoringCase(button_name, "-"))
    return ClassicControllerButtonsEnum::Minus;

  else if (IsEqualIgnoringCase(button_name, "triggerL"))
    return ClassicControllerButtonsEnum::TriggerL;
  else if (IsEqualIgnoringCase(button_name, "triggerR"))
    return ClassicControllerButtonsEnum::TriggerR;

  else if (IsEqualIgnoringCase(button_name, "leftStickX"))
    return ClassicControllerButtonsEnum::LeftStickX;
  else if (IsEqualIgnoringCase(button_name, "leftStickY"))
    return ClassicControllerButtonsEnum::LeftStickY;

  else if (IsEqualIgnoringCase(button_name, "rightStickX"))
    return ClassicControllerButtonsEnum::RightStickX;
  else if (IsEqualIgnoringCase(button_name, "rightStickY"))
    return ClassicControllerButtonsEnum::RightStickY;

  else
    return ClassicControllerButtonsEnum::Unknown;
}

const char* ConvertClassicControllerButtonToString(ClassicControllerButtonsEnum button)
{
  switch (button)
  {
  case ClassicControllerButtonsEnum::A:
    return "A";
  case ClassicControllerButtonsEnum::B:
    return "B";
  case ClassicControllerButtonsEnum::X:
    return "X";
  case ClassicControllerButtonsEnum::Y:
    return "Y";
  case ClassicControllerButtonsEnum::YL:
    return "YL";
  case ClassicControllerButtonsEnum::ZR:
    return "ZR";
  case ClassicControllerButtonsEnum::DPadUp:
    return "dPadUp";
  case ClassicControllerButtonsEnum::DPadDown:
    return "dPadDown";
  case ClassicControllerButtonsEnum::DPadLeft:
    return "dPadLeft";
  case ClassicControllerButtonsEnum::DPadRight:
    return "dPadRight";
  case ClassicControllerButtonsEnum::Home:
    return "Home";
  case ClassicControllerButtonsEnum::Plus:
    return "+";
  case ClassicControllerButtonsEnum::Minus:
    return "-";
  case ClassicControllerButtonsEnum::TriggerL:
    return "triggerL";
  case ClassicControllerButtonsEnum::TriggerR:
    return "triggerR";
  case ClassicControllerButtonsEnum::LeftStickX:
    return "leftStickX";
  case ClassicControllerButtonsEnum::LeftStickY:
    return "leftStickY";
  case ClassicControllerButtonsEnum::RightStickX:
    return "rightStickX";
  case ClassicControllerButtonsEnum::RightStickY:
    return "rightStickY";
  default:
    return "";
  }
}

}  // namespace WiiControllerEnums
