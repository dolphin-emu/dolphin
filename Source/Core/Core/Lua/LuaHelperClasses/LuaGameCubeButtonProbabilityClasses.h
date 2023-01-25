#pragma once

#include "Core/Movie.h"
#include "LuaGCButtons.h"
#include <vector>

class LuaGameCubeButtonProbabilityEvent
{
public:
  virtual void applyProbability(Movie::ControllerState& controllerState) = 0;
  virtual ~LuaGameCubeButtonProbabilityEvent() {}
  void checkIfEventHappened(double probability) {
    if (probability >= 100.0)
      eventHappened = true;
    else if (probability <= 0.0)
      eventHappened = false;
    else
    eventHappened = ((probability/100.0) >= (static_cast<double>(rand()) / (static_cast<double>(RAND_MAX))));
  }

  bool eventHappened;
  GC_BUTTON_NAME buttonEffected;
};

class LuaGCButtonFlipProbabilityEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonFlipProbabilityEvent(double probability, GC_BUTTON_NAME newButtonEffected) {
    buttonEffected = newButtonEffected;
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case A:
      controllerState.A = !controllerState.A;
      return;
    case B:
      controllerState.B = !controllerState.B;
      return;
    case X:
      controllerState.X = !controllerState.X;
      return;
    case Y:
      controllerState.Y = !controllerState.Y;
      return;
    case Z:
      controllerState.Z = !controllerState.Z;
      return;
    case L:
      controllerState.L = !controllerState.L;
      return;
    case R:
      controllerState.R = !controllerState.R;
      return;
    case dPadUp:
      controllerState.DPadUp = !controllerState.DPadUp;
      return;
    case dPadDown:
      controllerState.DPadDown = !controllerState.DPadDown;
      return;
    case dPadLeft:
      controllerState.DPadLeft = !controllerState.DPadLeft;
      return;
    case dPadRight:
      controllerState.DPadRight = !controllerState.DPadRight;
      return;
    case START:
      controllerState.Start = !controllerState.Start;
      return;
    case RESET:
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
  LuaGCButtonReleaseEvent(double probability, GC_BUTTON_NAME newButtonName) {
    buttonEffected = newButtonName;
    checkIfEventHappened(probability);
  }

  void applyProbability(Movie::ControllerState& controllerState)
  {
    if (!eventHappened)
      return;

    switch (buttonEffected)
    {
    case A:
      controllerState.A = false;
      return;
    case B:
      controllerState.B = false;
      return;
    case X:
      controllerState.X = false;
      return;
    case Y:
      controllerState.Y = false;
      return;
    case Z:
      controllerState.Z = false;
      return;
    case L:
      controllerState.L = false;
      return;
    case R:
      controllerState.R = false;
      return;
    case dPadUp:
      controllerState.DPadUp = false;
      return;
    case dPadDown:
      controllerState.DPadDown = false;
      return;
    case dPadLeft:
      controllerState.DPadLeft = false;
      return;
    case dPadRight:
      controllerState.DPadRight = false;
      return;
    case START:
      controllerState.Start = false;
      return;
    case RESET:
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
  LuaGCButtonPressEvent(double probability, GC_BUTTON_NAME newButton)
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
    case A:
      controllerState.A = true;
      return;
    case B:
      controllerState.B = true;
      return;
    case X:
      controllerState.X = true;
      return;
    case Y:
      controllerState.Y = true;
      return;
    case Z:
      controllerState.Z = true;
      return;
    case L:
      controllerState.L = true;
      return;
    case R:
      controllerState.R = true;
      return;
    case dPadUp:
      controllerState.DPadUp = true;
      return;
    case dPadDown:
      controllerState.DPadDown = true;
      return;
    case dPadLeft:
      controllerState.DPadLeft = true;
      return;
    case dPadRight:
      controllerState.DPadRight = true;
      return;
    case START:
      controllerState.Start = true;
      return;
    case RESET:
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

  LuaGCAlterAnalogInputFromCurrentValue(double probability, GC_BUTTON_NAME newButtonName, u32 newMaxLowerDifference, u32 newMaxUpperDifference)
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
    case analogStickX:
      controllerState.AnalogStickX = getNewValue(controllerState.AnalogStickX);
      return;
    case analogStickY:
      controllerState.AnalogStickY = getNewValue(controllerState.AnalogStickY);
      return;
    case cStickX:
      controllerState.CStickX = getNewValue(controllerState.CStickX);
      return;
    case cStickY:
      controllerState.CStickY = getNewValue(controllerState.CStickY);
      return;
    case triggerL:
      controllerState.TriggerL = getNewValue(controllerState.TriggerL);
      return;
    case triggerR:
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
  LuaGCAlterAnalogInputFromFixedValue(double probability, GC_BUTTON_NAME newButtonName, u32 value, u32 maxLowerDifference, u32 maxUpperDifference)
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
    case analogStickX:
      controllerState.AnalogStickX = alteredButtonValue;
      return;
    case analogStickY:
      controllerState.AnalogStickY = alteredButtonValue;
      return;
    case cStickX:
      controllerState.CStickX = alteredButtonValue;
      return;
    case cStickY:
      controllerState.CStickY = alteredButtonValue;
      return;
    case triggerL:
      controllerState.TriggerL = alteredButtonValue;
      return;
    case triggerR:
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
  LuaGCButtonComboEvent(double probability,  Movie::ControllerState newButtonPresses, std::vector<GC_BUTTON_NAME> newButtonsToLookAt, bool newSetOtherButtonsToBlank)
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
      case A:
        controllerState.A = buttonPresses.A;
        break;
      case B:
        controllerState.B = buttonPresses.B;
        break;
      case X:
        controllerState.X = buttonPresses.X;
        break;
      case Y:
        controllerState.Y = buttonPresses.Y;
        break;
      case Z:
        controllerState.Z = buttonPresses.Z;
        break;
      case L:
        controllerState.L = buttonPresses.L;
        break;
      case R:
        controllerState.R = buttonPresses.R;
        break;
      case dPadUp:
        controllerState.DPadUp = buttonPresses.DPadUp;
        break;
      case dPadDown:
        controllerState.DPadDown = buttonPresses.DPadDown;
        break;
      case dPadLeft:
        controllerState.DPadLeft = buttonPresses.DPadLeft;
        break;
      case dPadRight:
        controllerState.DPadRight = buttonPresses.DPadRight;
        break;
      case START:
        controllerState.Start = buttonPresses.Start;
        break;
      case RESET:
        controllerState.reset = buttonPresses.reset;
        break;
      case triggerL:
        controllerState.TriggerL = buttonPresses.TriggerL;
        break;
      case triggerR:
        controllerState.TriggerR = buttonPresses.TriggerR;
        break;
      case analogStickX:
        controllerState.AnalogStickX = buttonPresses.AnalogStickX;
        break;
      case analogStickY:
        controllerState.AnalogStickY = buttonPresses.AnalogStickY;
        break;
      case cStickX:
        controllerState.CStickX = buttonPresses.CStickX;
        break;
      case cStickY:
        controllerState.CStickY = buttonPresses.CStickY;
        break;
      default:
        break;
      }
    }


  }

  private:
    Movie::ControllerState buttonPresses;
    std::vector<GC_BUTTON_NAME> buttonsToLookAt;
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
