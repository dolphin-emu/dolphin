// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USB_KBD.h"

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"  // Local core functions
#include "Core/HW/Memmap.h"
#include "Core/System.h"
#include "InputCommon/ControlReference/ControlReference.h"  // For background input check

namespace IOS::HLE
{
USB_KBD::MessageData::MessageData(MessageType type, const Common::HIDPressedState& state)
    : msg_type{Common::swap32(static_cast<u32>(type))}, modifiers{state.modifiers},
      pressed_keys{state.pressed_keys}
{
}

// TODO: support in netplay/movies.

USB_KBD::USB_KBD(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

std::optional<IPCReply> USB_KBD::Open(const OpenRequest& request)
{
  INFO_LOG_FMT(IOS, "USB_KBD: Open");

  m_message_queue = {};
  m_previous_state = {};
  m_keyboard_context = Common::KeyboardContext::GetInstance();

  // m_message_queue.emplace(MessageType::KeyboardConnect, {});
  return Device::Open(request);
}

std::optional<IPCReply> USB_KBD::Close(u32 fd)
{
  INFO_LOG_FMT(IOS, "USB_KBD: Close");

  m_keyboard_context.reset();

  return Device::Close(fd);
}

std::optional<IPCReply> USB_KBD::Write(const ReadWriteRequest& request)
{
  // Stubbed.
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> USB_KBD::IOCtl(const IOCtlRequest& request)
{
  if (Config::Get(Config::MAIN_WII_KEYBOARD) && !Core::WantsDeterminism() &&
      ControlReference::GetInputGate() && !m_message_queue.empty())
  {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    memory.CopyToEmu(request.buffer_out, &m_message_queue.front(), sizeof(MessageData));
    m_message_queue.pop();
  }
  return IPCReply(IPC_SUCCESS);
}

void USB_KBD::Update()
{
  if (!Config::Get(Config::MAIN_WII_KEYBOARD) || Core::WantsDeterminism() || !m_is_active ||
      !m_keyboard_context)
  {
    return;
  }

  const auto current_state = m_keyboard_context->GetPressedState();

  if (current_state == m_previous_state)
    return;

  m_message_queue.emplace(MessageType::Event, current_state);
  m_previous_state = std::move(current_state);
}
}  // namespace IOS::HLE
