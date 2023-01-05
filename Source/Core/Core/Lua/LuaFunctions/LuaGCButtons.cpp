#include "LuaGCButtons.h"


bool isBinaryButton(GC_BUTTON_NAME button)
{
  return (button == A || button == B || button == X || button == Y || button == Z || button == L ||
          button == R || button == START || button == RESET || button == dPadUp ||
          button == dPadDown || button == dPadLeft || button == dPadRight);
}

bool isAnalogButton(GC_BUTTON_NAME button)
{
  return (button == analogStickX || button == analogStickY || button == cStickX ||
          button == cStickY || button == triggerL || button == triggerR);
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

GC_BUTTON_NAME parseGCButton(const char* buttonName)
{
  if (strlen(buttonName) == 1)
  {
    switch (buttonName[0])
    {
    case 'a':
    case 'A':
      return A;

    case 'b':
    case 'B':
      return B;

    case 'x':
    case 'X':
      return X;

    case 'y':
    case 'Y':
      return Y;

    case 'z':
    case 'Z':
      return Z;

    case 'l':
    case 'L':
      return L;

    case 'r':
    case 'R':
      return R;

    default:
      return UNKNOWN;
    }
  }

  switch (buttonName[0])
  {
  case 'd':
  case 'D':
    if (isEqualIgnoreCase(buttonName, "dPadUp"))
      return dPadUp;
    else if (isEqualIgnoreCase(buttonName, "dPadDown"))
      return dPadDown;
    else if (isEqualIgnoreCase(buttonName, "dPadLeft"))
      return dPadLeft;
    else if (isEqualIgnoreCase(buttonName, "dPadRight"))
      return dPadRight;
    else
      return UNKNOWN;

  case 'a':
  case 'A':
    if (isEqualIgnoreCase(buttonName, "analogStickX"))
      return analogStickX;
    else if (isEqualIgnoreCase(buttonName, "analogStickY"))
      return analogStickY;
    else
      return UNKNOWN;

  case 'c':
  case 'C':
    if (isEqualIgnoreCase(buttonName, "cStickX"))
      return cStickX;
    else if (isEqualIgnoreCase(buttonName, "cStickY"))
      return cStickY;
    else
      return UNKNOWN;

  case 't':
  case 'T':
    if (isEqualIgnoreCase(buttonName, "triggerL"))
      return triggerL;
    else if (isEqualIgnoreCase(buttonName, "triggerR"))
      return triggerR;
    else
      return UNKNOWN;

  case 'r':
  case 'R':
    if (isEqualIgnoreCase(buttonName, "reset"))
      return RESET;
    else
      return UNKNOWN;

  case 's':
  case 'S':
    if (isEqualIgnoreCase(buttonName, "start"))
      return START;
    else
      return UNKNOWN;

  default:
    return UNKNOWN;
  }
}

  const char* convertButtonEnumToString(GC_BUTTON_NAME button)
  {
    switch (button)
    {
    case A:
      return "A";
    case B:
      return "B";
    case X:
      return "X";
    case Y:
      return "Y";
    case Z:
      return "Z";
    case L:
      return "L";
    case R:
      return "R";
    case START:
      return "START";
    case RESET:
      return "RESET";
    case dPadUp:
      return "dPadUp";
    case dPadDown:
      return "dPadDown";
    case dPadLeft:
      return "dPadLeft";
    case dPadRight:
      return "dPadRight";
    case triggerL:
      return "triggerL";
    case triggerR:
      return "triggerR";
    case analogStickX:
      return "analogStickX";
    case analogStickY:
      return "analogStickY";
    case cStickX:
      return "cStickX";
    case cStickY:
      return "cStickY";
    default:
      return "";
    }
  }
