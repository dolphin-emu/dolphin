// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceKeyboard.h"

#include <array>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/HW/GCKeyboard.h"
#include "InputCommon/KeyboardStatus.h"

namespace SerialInterface
{
// --- GameCube keyboard ---
CSIDevice_Keyboard::CSIDevice_Keyboard(Core::System& system, SIDevices device, int device_number)
    : ISIDevice(system, device, device_number)
{
}

int CSIDevice_Keyboard::RunBuffer(u8* buffer, int request_length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, request_length);

  // Read the command
  const auto command = static_cast<EBufferCommands>(buffer[0]);

  // Handle it
  switch (command)
  {
  case EBufferCommands::CMD_STATUS:
  case EBufferCommands::CMD_RESET:
  {
    u32 id = Common::swap32(SI_GC_KEYBOARD);
    std::memcpy(buffer, &id, sizeof(id));
    return sizeof(id);
  }

  case EBufferCommands::CMD_DIRECT_KB:
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Keyboard - Direct (Request Length: {})", request_length);
    u32 high, low;
    GetData(high, low);
    for (int i = 0; i < 4; i++)
    {
      buffer[i + 0] = (high >> (24 - (i * 8))) & 0xff;
      buffer[i + 4] = (low >> (24 - (i * 8))) & 0xff;
    }
    return sizeof(high) + sizeof(low);
  }

  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command     ({:#x})", static_cast<u8>(command));
  }
  break;
  }

  return 0;
}

KeyboardStatus CSIDevice_Keyboard::GetKeyboardStatus() const
{
  return Keyboard::GetStatus(m_device_number);
}

bool CSIDevice_Keyboard::GetData(u32& hi, u32& low)
{
  const KeyboardStatus key_status = GetKeyboardStatus();
  const KeyArray key = MapKeys(key_status);
  const u8 checksum = key[0] ^ key[1] ^ key[2] ^ m_counter;

  hi = m_counter << 24;
  low = key[0] << 24 | key[1] << 16 | key[2] << 8 | checksum;

  return true;
}

void CSIDevice_Keyboard::SendCommand(u32 command, u8 poll)
{
  UCommand keyboard_command(command);

  if (static_cast<EDirectCommands>(keyboard_command.command) == EDirectCommands::CMD_POLL)
  {
    m_counter++;
    m_counter &= 15;
  }
  else if (keyboard_command.command != 0x00)
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown direct command     ({:#x})", command);
  }
}

void CSIDevice_Keyboard::DoState(PointerWrap& p)
{
  p.Do(m_counter);
}

template <size_t N>
using MaskArray = std::array<KeyMasks, N>;

template <size_t N>
using KeyScanCodeArray = std::array<KeyScanCode, N>;

CSIDevice_Keyboard::KeyArray CSIDevice_Keyboard::MapKeys(const KeyboardStatus& key_status) const
{
  static constexpr MaskArray<16> key0_masks{
      KEYMASK_HOME, KEYMASK_END, KEYMASK_PGUP, KEYMASK_PGDN, KEYMASK_SCROLLLOCK, KEYMASK_A,
      KEYMASK_B,    KEYMASK_C,   KEYMASK_D,    KEYMASK_E,    KEYMASK_F,          KEYMASK_G,
      KEYMASK_H,    KEYMASK_I,   KEYMASK_J,    KEYMASK_K,
  };
  static constexpr KeyScanCodeArray<16> key0_keys{
      KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_SCROLLLOCK, KEY_A, KEY_B, KEY_C,
      KEY_D,    KEY_E,   KEY_F,    KEY_G,    KEY_H,          KEY_I, KEY_J, KEY_K,
  };

  static constexpr MaskArray<16> key1_masks{
      KEYMASK_L, KEYMASK_M, KEYMASK_N, KEYMASK_O, KEYMASK_P, KEYMASK_Q, KEYMASK_R, KEYMASK_S,
      KEYMASK_T, KEYMASK_U, KEYMASK_V, KEYMASK_W, KEYMASK_X, KEYMASK_Y, KEYMASK_Z, KEYMASK_1,
  };
  static constexpr KeyScanCodeArray<16> key1_keys{
      KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S,
      KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_1,
  };

  static constexpr MaskArray<16> key2_masks{
      KEYMASK_2,          KEYMASK_3,           KEYMASK_4,     KEYMASK_5,
      KEYMASK_6,          KEYMASK_7,           KEYMASK_8,     KEYMASK_9,
      KEYMASK_0,          KEYMASK_MINUS,       KEYMASK_PLUS,  KEYMASK_PRINTSCR,
      KEYMASK_BRACE_OPEN, KEYMASK_BRACE_CLOSE, KEYMASK_COLON, KEYMASK_QUOTE,
  };
  static constexpr KeyScanCodeArray<16> key2_keys{
      KEY_2,          KEY_3,           KEY_4,     KEY_5,     KEY_6,    KEY_7,
      KEY_8,          KEY_9,           KEY_0,     KEY_MINUS, KEY_PLUS, KEY_PRINTSCR,
      KEY_BRACE_OPEN, KEY_BRACE_CLOSE, KEY_COLON, KEY_QUOTE,
  };

  static constexpr MaskArray<16> key3_masks{
      KEYMASK_HASH, KEYMASK_COMMA, KEYMASK_PERIOD, KEYMASK_QUESTIONMARK, KEYMASK_INTERNATIONAL1,
      KEYMASK_F1,   KEYMASK_F2,    KEYMASK_F3,     KEYMASK_F4,           KEYMASK_F5,
      KEYMASK_F6,   KEYMASK_F7,    KEYMASK_F8,     KEYMASK_F9,           KEYMASK_F10,
      KEYMASK_F11,
  };
  static constexpr KeyScanCodeArray<16> key3_keys{
      KEY_HASH, KEY_COMMA, KEY_PERIOD, KEY_QUESTIONMARK, KEY_INTERNATIONAL1,
      KEY_F1,   KEY_F2,    KEY_F3,     KEY_F4,           KEY_F5,
      KEY_F6,   KEY_F7,    KEY_F8,     KEY_F9,           KEY_F10,
      KEY_F11,
  };

  static constexpr MaskArray<16> key4_masks{
      KEYMASK_F12,         KEYMASK_ESC,        KEYMASK_INSERT,       KEYMASK_DELETE,
      KEYMASK_TILDE,       KEYMASK_BACKSPACE,  KEYMASK_TAB,          KEYMASK_CAPSLOCK,
      KEYMASK_LEFTSHIFT,   KEYMASK_RIGHTSHIFT, KEYMASK_LEFTCONTROL,  KEYMASK_RIGHTALT,
      KEYMASK_LEFTWINDOWS, KEYMASK_SPACE,      KEYMASK_RIGHTWINDOWS, KEYMASK_MENU,
  };
  static constexpr KeyScanCodeArray<16> key4_keys{
      KEY_F12,         KEY_ESC,        KEY_INSERT,       KEY_DELETE,
      KEY_TILDE,       KEY_BACKSPACE,  KEY_TAB,          KEY_CAPSLOCK,
      KEY_LEFTSHIFT,   KEY_RIGHTSHIFT, KEY_LEFTCONTROL,  KEY_RIGHTALT,
      KEY_LEFTWINDOWS, KEY_SPACE,      KEY_RIGHTWINDOWS, KEY_MENU,
  };

  static constexpr MaskArray<5> key5_masks{
      KEYMASK_LEFTARROW, KEYMASK_DOWNARROW, KEYMASK_UPARROW, KEYMASK_RIGHTARROW, KEYMASK_ENTER,
  };
  static constexpr KeyScanCodeArray<5> key5_keys{
      KEY_LEFTARROW, KEY_DOWNARROW, KEY_UPARROW, KEY_RIGHTARROW, KEY_ENTER,
  };

  u32 keys_held = 0;
  constexpr u32 MAX_KEYS_HELD = 3;

  KeyArray key{};

  const auto check_masks = [&](const auto& masks, const auto& keys, u32 field) {
    for (size_t i = 0; i < masks.size(); i++)
    {
      if ((field & masks[i]) == 0)
        continue;

      key[keys_held++] = keys[i];
      if (keys_held >= MAX_KEYS_HELD)
        return true;
    }

    return false;
  };

  if (check_masks(key0_masks, key0_keys, key_status.key0x))
    return key;

  if (check_masks(key1_masks, key1_keys, key_status.key1x))
    return key;

  if (check_masks(key2_masks, key2_keys, key_status.key2x))
    return key;

  if (check_masks(key3_masks, key3_keys, key_status.key3x))
    return key;

  if (check_masks(key4_masks, key4_keys, key_status.key4x))
    return key;

  if (check_masks(key5_masks, key5_keys, key_status.key5x))
    return key;

  return key;
}
}  // namespace SerialInterface
