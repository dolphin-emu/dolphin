// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Inline.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "Common/Logging/Log.h"

namespace WiimoteEmu
{
Extension::Extension(const char* name) : Extension(name, name)
{
}

Extension::Extension(const char* config_name, const char* display_name)
    : m_config_name(config_name), m_display_name(display_name)
{
}

std::string Extension::GetName() const
{
  return m_config_name;
}

std::string Extension::GetDisplayName() const
{
  return m_display_name;
}

InputConfig* Extension::GetConfig() const
{
  return ::Wiimote::GetConfig();
}

None::None() : Extension("None")
{
}

bool None::ReadDeviceDetectPin() const
{
  return false;
}

void None::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  target_state->data.emplace<std::monostate>();
}

void None::Update(const DesiredExtensionState& target_state)
{
  // Nothing needed.
}

void None::Reset()
{
  // Nothing needed.
}

void None::DoState(PointerWrap& p)
{
  // Nothing needed.
}

bool None::StartWrite(u8 slave_addr)
{
  return false;
}

bool None::StartRead(u8 slave_addr)
{
  return false;
}

void None::Stop()
{
  // Nothing needed.
}

std::optional<u8> None::ReadByte()
{
  return std::nullopt;
}

bool None::WriteByte(u8 value)
{
  return false;
}

bool EncryptedExtension::ReadDeviceDetectPin() const
{
  return true;
}

u8 EncryptedExtension::ReadByte(u8 addr)
{
  if (0x00 == addr)
  {
    // This is where real hardware would update controller data
    // We do it in Update() for TAS determinism
    // TAS code fails to sync data reads and such..
  }

  u8 result = RawRead(&m_reg, addr);

  // Encrypt data read from extension register.
  if (ENCRYPTION_ENABLED == m_reg.encryption)
  {
    if (m_is_key_dirty)
    {
      UpdateEncryptionKey();
      m_is_key_dirty = false;
    }

    ext_key.Encrypt(&result, addr, 1);
  }

  return result;
}

void EncryptedExtension::WriteByte(u8 addr, u8 value)
{
  RawWrite(&m_reg, addr, value);

  constexpr u8 ENCRYPTION_KEY_DATA_BEGIN = offsetof(Register, encryption_key_data);
  constexpr u8 ENCRYPTION_KEY_DATA_END = ENCRYPTION_KEY_DATA_BEGIN + 0x10;

  if (addr >= ENCRYPTION_KEY_DATA_BEGIN && addr < ENCRYPTION_KEY_DATA_END)
  {
    // FYI: Real extensions seem to require the key data written in specifically sized chunks.
    // We just run the key generation on all writes to the key area.
    m_is_key_dirty = true;
  }
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
  I2CSlaveAutoIncrementing::DoState(p);

  p.Do(m_reg);

  if (p.IsReadMode())
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
