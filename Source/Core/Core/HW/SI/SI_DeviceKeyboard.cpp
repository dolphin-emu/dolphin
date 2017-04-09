// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceKeyboard.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GCKeyboard.h"
#include "InputCommon/KeyboardStatus.h"

namespace SerialInterface
{
// --- GameCube keyboard ---
CSIDevice_Keyboard::CSIDevice_Keyboard(SIDevices device, int device_number)
    : ISIDevice(device, device_number)
{
}

int CSIDevice_Keyboard::RunBuffer(u8* buffer, int length)
{
  // For debug logging only
  ISIDevice::RunBuffer(buffer, length);

  // Read the command
  EBufferCommands command = static_cast<EBufferCommands>(buffer[3]);

  // Handle it
  switch (command)
  {
  case CMD_RESET:
  case CMD_ID:
  {
    constexpr u32 id = SI_GC_KEYBOARD;
    std::memcpy(buffer, &id, sizeof(id));
    break;
  }

  case CMD_DIRECT:
  {
    INFO_LOG(SERIALINTERFACE, "Keyboard - Direct (Length: %d)", length);
    u32 high, low;
    GetData(high, low);
    for (int i = 0; i < (length - 1) / 2; i++)
    {
      buffer[i + 0] = (high >> (i * 8)) & 0xff;
      buffer[i + 4] = (low >> (i * 8)) & 0xff;
    }
  }
  break;

  default:
  {
    ERROR_LOG(SERIALINTERFACE, "Unknown SI command     (0x%x)", command);
  }
  break;
  }

  return length;
}

KeyboardStatus CSIDevice_Keyboard::GetKeyboardStatus() const
{
  return Keyboard::GetStatus(m_device_number);
}

bool CSIDevice_Keyboard::GetData(u32& hi, u32& low)
{
  KeyboardStatus key_status = GetKeyboardStatus();
  u8 key[3] = {0x00, 0x00, 0x00};
  MapKeys(key_status, key);
  u8 checksum = key[0] ^ key[1] ^ key[2] ^ m_counter;

  hi = m_counter << 24;
  low = key[0] << 24 | key[1] << 16 | key[2] << 8 | checksum;

  return true;
}

void CSIDevice_Keyboard::SendCommand(u32 command, u8 poll)
{
  UCommand keyboard_command(command);

  switch (keyboard_command.command)
  {
  case 0x00:
    break;

  case CMD_POLL:
  {
    m_counter++;
    m_counter &= 15;
  }
  break;
  default:
  {
    ERROR_LOG(SERIALINTERFACE, "Unknown direct command     (0x%x)", command);
  }
  break;
  }
}

void CSIDevice_Keyboard::DoState(PointerWrap& p)
{
  p.Do(m_counter);
}

void CSIDevice_Keyboard::MapKeys(const KeyboardStatus& key_status, u8* key)
{
  u8 keys_held = 0;
  const u8 MAX_KEYS_HELD = 3;

  if (key_status.key0x & KEYMASK_HOME)
  {
    key[keys_held++] = KEY_HOME;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_END)
  {
    key[keys_held++] = KEY_END;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_PGUP)
  {
    key[keys_held++] = KEY_PGUP;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_PGDN)
  {
    key[keys_held++] = KEY_PGDN;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_SCROLLLOCK)
  {
    key[keys_held++] = KEY_SCROLLLOCK;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_A)
  {
    key[keys_held++] = KEY_A;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_B)
  {
    key[keys_held++] = KEY_B;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_C)
  {
    key[keys_held++] = KEY_C;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_D)
  {
    key[keys_held++] = KEY_D;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_E)
  {
    key[keys_held++] = KEY_E;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_F)
  {
    key[keys_held++] = KEY_F;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_G)
  {
    key[keys_held++] = KEY_G;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_H)
  {
    key[keys_held++] = KEY_H;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_I)
  {
    key[keys_held++] = KEY_I;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_J)
  {
    key[keys_held++] = KEY_J;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key0x & KEYMASK_K)
  {
    key[keys_held++] = KEY_K;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_L)
  {
    key[keys_held++] = KEY_L;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_M)
  {
    key[keys_held++] = KEY_M;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_N)
  {
    key[keys_held++] = KEY_N;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_O)
  {
    key[keys_held++] = KEY_O;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_P)
  {
    key[keys_held++] = KEY_P;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_Q)
  {
    key[keys_held++] = KEY_Q;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_R)
  {
    key[keys_held++] = KEY_R;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_S)
  {
    key[keys_held++] = KEY_S;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_T)
  {
    key[keys_held++] = KEY_T;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_U)
  {
    key[keys_held++] = KEY_U;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_V)
  {
    key[keys_held++] = KEY_V;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_W)
  {
    key[keys_held++] = KEY_W;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_X)
  {
    key[keys_held++] = KEY_X;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_Y)
  {
    key[keys_held++] = KEY_Y;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_Z)
  {
    key[keys_held++] = KEY_Z;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key1x & KEYMASK_1)
  {
    key[keys_held++] = KEY_1;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_2)
  {
    key[keys_held++] = KEY_2;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_3)
  {
    key[keys_held++] = KEY_3;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_4)
  {
    key[keys_held++] = KEY_4;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_5)
  {
    key[keys_held++] = KEY_5;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_6)
  {
    key[keys_held++] = KEY_6;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_7)
  {
    key[keys_held++] = KEY_7;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_8)
  {
    key[keys_held++] = KEY_8;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_9)
  {
    key[keys_held++] = KEY_9;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_0)
  {
    key[keys_held++] = KEY_0;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_MINUS)
  {
    key[keys_held++] = KEY_MINUS;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_PLUS)
  {
    key[keys_held++] = KEY_PLUS;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_PRINTSCR)
  {
    key[keys_held++] = KEY_PRINTSCR;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_BRACE_OPEN)
  {
    key[keys_held++] = KEY_BRACE_OPEN;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_BRACE_CLOSE)
  {
    key[keys_held++] = KEY_BRACE_CLOSE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_COLON)
  {
    key[keys_held++] = KEY_COLON;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key2x & KEYMASK_QUOTE)
  {
    key[keys_held++] = KEY_QUOTE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_HASH)
  {
    key[keys_held++] = KEY_HASH;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_COMMA)
  {
    key[keys_held++] = KEY_COMMA;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_PERIOD)
  {
    key[keys_held++] = KEY_PERIOD;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_QUESTIONMARK)
  {
    key[keys_held++] = KEY_QUESTIONMARK;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_INTERNATIONAL1)
  {
    key[keys_held++] = KEY_INTERNATIONAL1;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F1)
  {
    key[keys_held++] = KEY_F1;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F2)
  {
    key[keys_held++] = KEY_F2;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F3)
  {
    key[keys_held++] = KEY_F3;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F4)
  {
    key[keys_held++] = KEY_F4;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F5)
  {
    key[keys_held++] = KEY_F5;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F6)
  {
    key[keys_held++] = KEY_F6;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F7)
  {
    key[keys_held++] = KEY_F7;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F8)
  {
    key[keys_held++] = KEY_F8;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F9)
  {
    key[keys_held++] = KEY_F9;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F10)
  {
    key[keys_held++] = KEY_F10;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key3x & KEYMASK_F11)
  {
    key[keys_held++] = KEY_F11;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_F12)
  {
    key[keys_held++] = KEY_F12;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_ESC)
  {
    key[keys_held++] = KEY_ESC;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_INSERT)
  {
    key[keys_held++] = KEY_INSERT;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_DELETE)
  {
    key[keys_held++] = KEY_DELETE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_TILDE)
  {
    key[keys_held++] = KEY_TILDE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_BACKSPACE)
  {
    key[keys_held++] = KEY_BACKSPACE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_TAB)
  {
    key[keys_held++] = KEY_TAB;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_CAPSLOCK)
  {
    key[keys_held++] = KEY_CAPSLOCK;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_LEFTSHIFT)
  {
    key[keys_held++] = KEY_LEFTSHIFT;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_RIGHTSHIFT)
  {
    key[keys_held++] = KEY_RIGHTSHIFT;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_LEFTCONTROL)
  {
    key[keys_held++] = KEY_LEFTCONTROL;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_RIGHTALT)
  {
    key[keys_held++] = KEY_RIGHTALT;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_LEFTWINDOWS)
  {
    key[keys_held++] = KEY_LEFTWINDOWS;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_SPACE)
  {
    key[keys_held++] = KEY_SPACE;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_RIGHTWINDOWS)
  {
    key[keys_held++] = KEY_RIGHTWINDOWS;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key4x & KEYMASK_MENU)
  {
    key[keys_held++] = KEY_MENU;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key5x & KEYMASK_LEFTARROW)
  {
    key[keys_held++] = KEY_LEFTARROW;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key5x & KEYMASK_DOWNARROW)
  {
    key[keys_held++] = KEY_DOWNARROW;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key5x & KEYMASK_UPARROW)
  {
    key[keys_held++] = KEY_UPARROW;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key5x & KEYMASK_RIGHTARROW)
  {
    key[keys_held++] = KEY_RIGHTARROW;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
  if (key_status.key5x & KEYMASK_ENTER)
  {
    key[keys_held++] = KEY_ENTER;
    if (keys_held >= MAX_KEYS_HELD)
      return;
  }
}
}  // namespace SerialInterface
