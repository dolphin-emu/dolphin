// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace WiimoteEmu
{
struct ExtensionReg;

class Attachment : public ControllerEmu::EmulatedController
{
public:
  Attachment(const char* const name, ExtensionReg& reg);

  virtual void GetState(u8* const data);
  virtual bool IsButtonPressed() const;

  void Reset();
  std::string GetName() const override;

protected:
  // Default radius for attachment analog sticks.
  static constexpr ControlState DEFAULT_ATTACHMENT_STICK_RADIUS = 1.0;

  std::array<u8, 6> m_id{};
  std::array<u8, 0x10> m_calibration{};

private:
  const char* const m_name;
  ExtensionReg& m_reg;
};

class None : public Attachment
{
public:
  explicit None(ExtensionReg& reg);
};
}  // namespace WiimoteEmu
