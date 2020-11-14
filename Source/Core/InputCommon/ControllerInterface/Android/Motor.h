// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

namespace ciface::Android
{
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
}  // namespace ciface::Android
