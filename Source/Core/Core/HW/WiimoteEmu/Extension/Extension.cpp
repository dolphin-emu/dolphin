// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Compiler.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

namespace WiimoteEmu
{
Extension::Extension(const char* name) : m_name(name)
{
}

std::string Extension::GetName() const
{
  return m_name;
}

None::None() : Extension("None")
{
}

bool None::ReadDeviceDetectPin() const
{
  return false;
}

void None::Update()
{
  // Nothing needed.
}

bool None::IsButtonPressed() const
{
  return false;
}

void None::Reset()
{
  // Nothing needed.
}

void None::DoState(PointerWrap& p)
{
  // Nothing needed.
}

int None::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  return 0;
}

int None::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  return 0;
}

EncryptedExtension::EncryptedExtension(const char* name) : Extension(name)
{
}

bool EncryptedExtension::ReadDeviceDetectPin() const
{
  return true;
}

int EncryptedExtension::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (0x00 == addr)
  {
    // This is where real hardware would update controller data
    // We do it in Update() for TAS determinism
    // TAS code fails to sync data reads and such..
  }

  auto const result = RawRead(&m_reg, addr, count, data_out);

  // Encrypt data read from extension register
  if (ENCRYPTION_ENABLED == m_reg.encryption)
  {
    // INFO_LOG(WIIMOTE, "Encrypted read.");
    ext_key.Encrypt(data_out, addr, (u8)count);
  }
  else
  {
    // INFO_LOG(WIIMOTE, "Unencrypted read.");
  }

  return result;
}

int EncryptedExtension::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  auto const result = RawWrite(&m_reg, addr, count, data_in);

  // TODO: make this check less ugly:
  if (addr + count > 0x40 && addr < 0x50)
  {
    // Run the key generation on all writes in the key area, it doesn't matter
    //  that we send it parts of a key, only the last full key will have an effect
    ext_key.Generate(m_reg.encryption_key_data);
  }

  return result;
}

void EncryptedExtension::DoState(PointerWrap& p)
{
  p.Do(m_reg);

  // No need to sync this when we can regenerate it:
  if (p.GetMode() == PointerWrap::MODE_READ)
    ext_key.Generate(m_reg.encryption_key_data);
}

}  // namespace WiimoteEmu
