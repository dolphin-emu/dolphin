// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "jni/ButtonManager.h"

namespace ciface
{
namespace Android
{
void PopulateDevices();
class Touchscreen : public Core::Device
{
private:
  class Button : public Input
  {
  public:
    std::string GetName() const;
    Button(int padID, ButtonManager::ButtonType index) : _padID(padID), _index(index) {}
    ControlState GetState() const;

  private:
    const int _padID;
    const ButtonManager::ButtonType _index;
  };
  class Axis : public Input
  {
  public:
    std::string GetName() const;
    Axis(int padID, ButtonManager::ButtonType index, float neg = 1.0f)
        : _padID(padID), _index(index), _neg(neg)
    {
    }
    ControlState GetState() const;

  private:
    const int _padID;
    const ButtonManager::ButtonType _index;
    const float _neg;
  };

public:
  Touchscreen(int padID);
  ~Touchscreen() {}
  std::string GetName() const;
  std::string GetSource() const;

private:
  const int _padID;
};
}
}
