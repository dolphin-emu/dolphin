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

#include "Common/Logging/Log.h"

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

  // Encrypt data read from extension register.
  if (ENCRYPTION_ENABLED == m_reg.encryption)
  {
    if (m_is_key_dirty)
    {
      UpdateEncryptionKey();
      m_is_key_dirty = false;
    }

    ext_key.Encrypt(data_out, addr, count);
  }

  return result;
}

int EncryptedExtension::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  auto const result = RawWrite(&m_reg, addr, count, data_in);

  constexpr u8 ENCRYPTION_KEY_DATA_BEGIN = offsetof(Register, encryption_key_data);
  constexpr u8 ENCRYPTION_KEY_DATA_END = ENCRYPTION_KEY_DATA_BEGIN + 0x10;

  if (addr + count > ENCRYPTION_KEY_DATA_BEGIN && addr < ENCRYPTION_KEY_DATA_END)
  {
    // FYI: Real extensions seem to require the key data written in specifically sized chunks.
    // We just run the key generation on all writes to the key area.
    m_is_key_dirty = true;
  }

  return result;
}

void EncryptedExtension::Reset()
{
  // Clear register state.
  m_reg = {};

  // Clear encryption key state.
  ext_key = {};
  m_is_key_dirty = true;
}

void EncryptedExtension::DoState(PointerWrap& p)
{
  p.Do(m_reg);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // No need to sync the key when we can just regenerate it.
    m_is_key_dirty = true;
  }
}

void Extension1stParty::UpdateEncryptionKey()
{
  ext_key = KeyGen1stParty().GenerateFromExtensionKeyData(m_reg.encryption_key_data);
}

void Extension3rdParty::UpdateEncryptionKey()
{
  ext_key = KeyGen3rdParty().GenerateFromExtensionKeyData(m_reg.encryption_key_data);
}

}  // namespace WiimoteEmu
