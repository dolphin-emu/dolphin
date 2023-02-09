#pragma once

#include "Core/Movie.h"
#include "LuaGCButtons.h"
#include <vector>
#include <random>


class LuaGameCubeButtonProbabilityEvent
{
public:
  static std::mt19937_64 numGenerator_64Bits;
  static void setRngSeeding();

  virtual void applyProbability(Movie::ControllerState& controllerState) = 0;
  virtual ~LuaGameCubeButtonProbabilityEvent() {}
  void checkIfEventHappened(double probability) {
    if (probability >= 100.0)
      eventHappened = true;
    else if (probability <= 0.0)
      eventHappened = false;
    else
    eventHappened = ((probability/100.0) >= (static_cast<double>(numGenerator_64Bits()) / static_cast<double>(numGenerator_64Bits.max())));
  }

  bool eventHappened;
  GcButtonName buttonEffected;
};

class LuaGCButtonFlipProbabilityEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonFlipProbabilityEvent(double probability, GcButtonName newButtonEffected)
  {
    buttonEffected = newButtonEffected;
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case GcButtonName::A:
      controllerState.A = !controllerState.A;
      return;
    case GcButtonName::B:
      controllerState.B = !controllerState.B;
      return;
    case GcButtonName::X:
      controllerState.X = !controllerState.X;
      return;
    case GcButtonName::Y:
      controllerState.Y = !controllerState.Y;
      return;
    case GcButtonName::Z:
      controllerState.Z = !controllerState.Z;
      return;
    case GcButtonName::L:
      controllerState.L = !controllerState.L;
      return;
    case GcButtonName::R:
      controllerState.R = !controllerState.R;
      return;
    case GcButtonName::DPadUp:
      controllerState.DPadUp = !controllerState.DPadUp;
      return;
    case GcButtonName::DPadDown:
      controllerState.DPadDown = !controllerState.DPadDown;
      return;
    case GcButtonName::DPadLeft:
      controllerState.DPadLeft = !controllerState.DPadLeft;
      return;
    case GcButtonName::DPadRight:
      controllerState.DPadRight = !controllerState.DPadRight;
      return;
    case GcButtonName::Start:
      controllerState.Start = !controllerState.Start;
      return;
    case GcButtonName::Reset:
      controllerState.reset = !controllerState.reset;
      return;
    default:
      return;
    }
  }
};

class LuaGCButtonReleaseEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonReleaseEvent(double probability, GcButtonName newButtonName)
  {
    buttonEffected = newButtonName;
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case GcButtonName::A:
      controllerState.A = false;
      return;
    case GcButtonName::B:
      controllerState.B = false;
      return;
    case GcButtonName::X:
      controllerState.X = false;
      return;
    case GcButtonName::Y:
      controllerState.Y = false;
      return;
    case GcButtonName::Z:
      controllerState.Z = false;
      return;
    case GcButtonName::L:
      controllerState.L = false;
      return;
    case GcButtonName::R:
      controllerState.R = false;
      return;
    case GcButtonName::DPadUp:
      controllerState.DPadUp = false;
      return;
    case GcButtonName::DPadDown:
      controllerState.DPadDown = false;
      return;
    case GcButtonName::DPadLeft:
      controllerState.DPadLeft = false;
      return;
    case GcButtonName::DPadRight:
      controllerState.DPadRight = false;
      return;
    case GcButtonName::Start:
      controllerState.Start = false;
      return;
    case GcButtonName::Reset:
      controllerState.reset = false;
      return;
    default:
      return;

    }
  }
};

class LuaGCButtonPressEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonPressEvent(double probability, GcButtonName newButton)
  {
    buttonEffected = newButton;
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case GcButtonName::A:
      controllerState.A = true;
      return;
    case GcButtonName::B:
      controllerState.B = true;
      return;
    case GcButtonName::X:
      controllerState.X = true;
      return;
    case GcButtonName::Y:
      controllerState.Y = true;
      return;
    case GcButtonName::Z:
      controllerState.Z = true;
      return;
    case GcButtonName::L:
      controllerState.L = true;
      return;
    case GcButtonName::R:
      controllerState.R = true;
      return;
    case GcButtonName::DPadUp:
      controllerState.DPadUp = true;
      return;
    case GcButtonName::DPadDown:
      controllerState.DPadDown = true;
      return;
    case GcButtonName::DPadLeft:
      controllerState.DPadLeft = true;
      return;
    case GcButtonName::DPadRight:
      controllerState.DPadRight = true;
      return;
    case GcButtonName::Start:
      controllerState.Start = true;
      return;
    case GcButtonName::Reset:
      controllerState.reset = true;
      return;
    default:
      return;
    }
  }
};

class LuaGCAlterAnalogInputFromCurrentValue : public LuaGameCubeButtonProbabilityEvent
{

public:

  LuaGCAlterAnalogInputFromCurrentValue(double probability, GcButtonName newButtonName,
                                        u32 newMaxLowerDifference, u32 newMaxUpperDifference)
  {
    buttonEffected = newButtonName;
    maxLowerDifference = newMaxLowerDifference;
    checkIfEventHappened(probability);
    calculateOffsetFromLowerBound(newMaxLowerDifference, newMaxUpperDifference);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case GcButtonName::AnalogStickX:
      controllerState.AnalogStickX = getNewValue(controllerState.AnalogStickX);
      return;
    case GcButtonName::AnalogStickY:
      controllerState.AnalogStickY = getNewValue(controllerState.AnalogStickY);
      return;
    case GcButtonName::CStickX:
      controllerState.CStickX = getNewValue(controllerState.CStickX);
      return;
    case GcButtonName::CStickY:
      controllerState.CStickY = getNewValue(controllerState.CStickY);
      return;
    case GcButtonName::TriggerL:
      controllerState.TriggerL = getNewValue(controllerState.TriggerL);
      return;
    case GcButtonName::TriggerR:
      controllerState.TriggerR = getNewValue(controllerState.TriggerR);
      return;
    default:
      return;

    }
  }

  private:
    u32 offsetFromLowerBound;
    u8 maxLowerDifference;

    void calculateOffsetFromLowerBound(u32 lowerDifference, u32 upperDifference)
    {
      if (upperDifference == 0 && lowerDifference == 0)
      {
        offsetFromLowerBound = 0;
        return;
      }

      offsetFromLowerBound = (rand() % (lowerDifference + upperDifference + 1));
    }

    u8 getNewValue(u8 currentValue)
    {
      int lowerBound = static_cast<int>(currentValue) - static_cast<int>(maxLowerDifference);
      int returnValue = lowerBound + static_cast<int>(offsetFromLowerBound);
      if (returnValue < 0)
        returnValue = 0;
      else if (returnValue > 255)
        returnValue = 255;
      return static_cast<u8>(returnValue);
    }
};

class LuaGCAlterAnalogInputFromFixedValue : public LuaGameCubeButtonProbabilityEvent
{
public:
    LuaGCAlterAnalogInputFromFixedValue(double probability, GcButtonName newButtonName, u32 value,
                                        u32 maxLowerDifference, u32 maxUpperDifference)
  {
    checkIfEventHappened(probability);
    buttonEffected = newButtonName;
    if (maxLowerDifference == 0 && maxUpperDifference == 0)
      alteredButtonValue = static_cast<u8>(value);
    else
    {
      int lowerBound = static_cast<int>(value) - static_cast<int>(maxLowerDifference);
      int tempResult = lowerBound + static_cast<int>((rand() % (maxLowerDifference + maxUpperDifference + 1)));
      if (tempResult < 0)
        tempResult = 0;
      if (tempResult > 255)
        tempResult = 255;
      alteredButtonValue = static_cast<u8>(tempResult);
    }
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case GcButtonName::AnalogStickX:
      controllerState.AnalogStickX = alteredButtonValue;
      return;
    case GcButtonName::AnalogStickY:
      controllerState.AnalogStickY = alteredButtonValue;
      return;
    case GcButtonName::CStickX:
      controllerState.CStickX = alteredButtonValue;
      return;
    case GcButtonName::CStickY:
      controllerState.CStickY = alteredButtonValue;
      return;
    case GcButtonName::TriggerL:
      controllerState.TriggerL = alteredButtonValue;
      return;
    case GcButtonName::TriggerR:
      controllerState.TriggerR = alteredButtonValue;
      return;
    default:
      return; 
    }
  }

  private:
    u8 alteredButtonValue;
};

class LuaGCButtonComboEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
    LuaGCButtonComboEvent(double probability, Movie::ControllerState newButtonPresses,
                          std::vector<GcButtonName> newButtonsToLookAt,
                          bool newSetOtherButtonsToBlank)
  {
    checkIfEventHappened(probability);
    buttonPresses = newButtonPresses;
    buttonsToLookAt = newButtonsToLookAt;
    setOtherButtonsToBlank = newSetOtherButtonsToBlank;
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    if (setOtherButtonsToBlank)
    {
      memset(&controllerState, 0, sizeof(Movie::ControllerState));
      controllerState.is_connected = true;
      controllerState.CStickX = 128;
      controllerState.CStickY = 128;
      controllerState.AnalogStickX = 128;
      controllerState.AnalogStickY = 128;
    }

    for (size_t i = 0; i < buttonsToLookAt.size(); ++i)
    {
      switch (buttonsToLookAt[i])
      {
      case GcButtonName::A:
        controllerState.A = buttonPresses.A;
        break;
      case GcButtonName::B:
        controllerState.B = buttonPresses.B;
        break;
      case GcButtonName::X:
        controllerState.X = buttonPresses.X;
        break;
      case GcButtonName::Y:
        controllerState.Y = buttonPresses.Y;
        break;
      case GcButtonName::Z:
        controllerState.Z = buttonPresses.Z;
        break;
      case GcButtonName::L:
        controllerState.L = buttonPresses.L;
        break;
      case GcButtonName::R:
        controllerState.R = buttonPresses.R;
        break;
      case GcButtonName::DPadUp:
        controllerState.DPadUp = buttonPresses.DPadUp;
        break;
      case GcButtonName::DPadDown:
        controllerState.DPadDown = buttonPresses.DPadDown;
        break;
      case GcButtonName::DPadLeft:
        controllerState.DPadLeft = buttonPresses.DPadLeft;
        break;
      case GcButtonName::DPadRight:
        controllerState.DPadRight = buttonPresses.DPadRight;
        break;
      case GcButtonName::Start:
        controllerState.Start = buttonPresses.Start;
        break;
      case GcButtonName::Reset:
        controllerState.reset = buttonPresses.reset;
        break;
      case GcButtonName::TriggerL:
        controllerState.TriggerL = buttonPresses.TriggerL;
        break;
      case GcButtonName::TriggerR:
        controllerState.TriggerR = buttonPresses.TriggerR;
        break;
      case GcButtonName::AnalogStickX:
        controllerState.AnalogStickX = buttonPresses.AnalogStickX;
        break;
      case GcButtonName::AnalogStickY:
        controllerState.AnalogStickY = buttonPresses.AnalogStickY;
        break;
      case GcButtonName::CStickX:
        controllerState.CStickX = buttonPresses.CStickX;
        break;
      case GcButtonName::CStickY:
        controllerState.CStickY = buttonPresses.CStickY;
        break;
      default:
        break;
      }
    }
  }

  private:
    Movie::ControllerState buttonPresses;
    std::vector<GcButtonName> buttonsToLookAt;
    bool setOtherButtonsToBlank;
    
};

class LuaGCClearControllerEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCClearControllerEvent(double probability) {
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    memset(&controllerState, 0, sizeof(Movie::ControllerState));
    controllerState.is_connected = true;
    controllerState.CStickX = 128;
    controllerState.CStickY = 128;
    controllerState.AnalogStickX = 128;
    controllerState.AnalogStickY = 128;
    return;
  }
};
