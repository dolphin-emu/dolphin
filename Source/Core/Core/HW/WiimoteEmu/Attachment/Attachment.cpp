// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"

namespace WiimoteEmu
{
// Extension device IDs to be written to the last bytes of the extension reg
// The id for nothing inserted
constexpr std::array<u8, 6> nothing_id{{0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e}};
// The id for a partially inserted extension (currently unused)
UNUSED constexpr std::array<u8, 6> partially_id{{0x00, 0x00, 0x00, 0x00, 0xff, 0xff}};

Attachment::Attachment(const char* const name, ExtensionReg& reg) : m_name(name), m_reg(reg)
{
}

None::None(ExtensionReg& reg) : Attachment("None", reg)
{
  // set up register
  m_id = nothing_id;
}

void Attachment::GetState(u8* const data)
{
}

bool Attachment::IsButtonPressed() const
{
  return false;
}

std::string Attachment::GetName() const
{
  return m_name;
}

void Attachment::Reset()
{
  // set up register
  m_reg = {};
  std::copy(m_id.cbegin(), m_id.cend(), m_reg.constant_id);
  std::copy(m_calibration.cbegin(), m_calibration.cend(), m_reg.calibration);
}
}  // namespace WiimoteEmu

namespace ControllerEmu
{
void Extension::GetState(u8* const data)
{
  static_cast<WiimoteEmu::Attachment*>(attachments[active_extension].get())->GetState(data);
}

bool Extension::IsButtonPressed() const
{
  // Extension == 0 means no Extension, > 0 means one is connected
  // Since we want to use this to know if disconnected Wiimotes want to be connected, and
  // disconnected
  // Wiimotes (can? always?) have their active_extension set to -1, we also have to check the
  // switch_extension
  if (active_extension > 0)
    return static_cast<WiimoteEmu::Attachment*>(attachments[active_extension].get())
        ->IsButtonPressed();
  if (switch_extension > 0)
    return static_cast<WiimoteEmu::Attachment*>(attachments[switch_extension].get())
        ->IsButtonPressed();
  return false;
}
}  // namespace ControllerEmu
