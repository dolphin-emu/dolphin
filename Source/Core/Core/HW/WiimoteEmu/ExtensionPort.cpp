// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/ExtensionPort.h"

#include "Common/ChunkFile.h"

namespace WiimoteEmu
{
ExtensionPort::ExtensionPort(I2CBus* i2c_bus) : m_i2c_bus(*i2c_bus)
{
}

bool ExtensionPort::IsDeviceConnected() const
{
  return m_extension->ReadDeviceDetectPin();
}

void ExtensionPort::AttachExtension(Extension* ext)
{
  m_i2c_bus.RemoveSlave(m_extension);

  m_extension = ext;
  m_i2c_bus.AddSlave(m_extension);
}

}  // namespace WiimoteEmu
