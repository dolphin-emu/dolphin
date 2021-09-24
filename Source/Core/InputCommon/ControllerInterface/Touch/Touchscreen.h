// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

namespace ciface::Touch
{
class Touchscreen : public Core::Device
{
private:
  class Button : public Input
  {
  public:
    std::string GetName() const override;
    Button(int pad_id, ButtonManager::ButtonType index) : m_pad_id(pad_id), m_index(index) {}
    ControlState GetState() const override;

  private:
    const int m_pad_id;
    const ButtonManager::ButtonType m_index;
  };
  class Axis : public Input
  {
  public:
    std::string GetName() const override;
    bool IsDetectable() const override { return false; }
    Axis(int pad_id, ButtonManager::ButtonType index, float neg = 1.0f)
        : m_pad_id(pad_id), m_index(index), m_neg(neg)
    {
    }
    ControlState GetState() const override;

  private:
    const int m_pad_id;
    const ButtonManager::ButtonType m_index;
    const float m_neg;
  };
  class Motor : public Core::Device::Output
  {
  public:
    Motor(int pad_id, ButtonManager::ButtonType index) : m_pad_id(pad_id), m_index(index) {}
    ~Motor();
    std::string GetName() const override;
    void SetState(ControlState state) override;

  private:
    const int m_pad_id;
    const ButtonManager::ButtonType m_index;
    static void Rumble(int pad_id, double state);
  };

public:
  Touchscreen(int pad_id, bool accelerometer_enabled, bool gyroscope_enabled);
  ~Touchscreen() {}
  std::string GetName() const override;
  std::string GetSource() const override;

private:
  const int m_pad_id;
};
}  // namespace ciface::Touch
