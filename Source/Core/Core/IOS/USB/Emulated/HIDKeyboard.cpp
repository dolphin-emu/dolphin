// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/HIDKeyboard.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"
#include "InputCommon/ControlReference/ControlReference.h"

namespace IOS::HLE::USB
{
HIDKeyboard::HIDKeyboard()
{
  m_id = u64(m_vid) << 32 | u64(m_pid) << 16 | u64(8) << 8 | u64(1);
}

HIDKeyboard::~HIDKeyboard()
{
  if (!m_device_attached)
    return;
  CancelPendingTransfers();
}

DeviceDescriptor HIDKeyboard::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> HIDKeyboard::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> HIDKeyboard::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> HIDKeyboard::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  if (const auto it{m_endpoint_descriptor.find(interface)}; it != m_endpoint_descriptor.end())
  {
    return it->second;
  }
  return {};
}

bool HIDKeyboard::Attach()
{
  if (m_device_attached)
    return true;

  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening emulated keyboard", m_vid, m_pid);
  m_keyboard_context = Common::KeyboardContext::GetInstance();
  m_worker.Reset("HID Keyboard", [this](auto transfer) { HandlePendingTransfer(transfer); });
  m_device_attached = true;
  return true;
}

bool HIDKeyboard::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int HIDKeyboard::CancelTransfer(const u8 endpoint)
{
  if (endpoint != KEYBOARD_ENDPOINT)
  {
    ERROR_LOG_FMT(
        IOS_USB,
        "[{:04x}:{:04x} {}] Cancelling transfers for invalid endpoint {:#x} (expected: {:#x})",
        m_vid, m_pid, m_active_interface, endpoint, KEYBOARD_ENDPOINT);
    return IPC_SUCCESS;
  }

  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);
  CancelPendingTransfers();

  return IPC_SUCCESS;
}

int HIDKeyboard::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int HIDKeyboard::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int HIDKeyboard::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int HIDKeyboard::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  auto& ios = cmd->GetEmulationKernel();

  switch (cmd->request_type << 8 | cmd->request)
  {
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_INTERFACE, REQUEST_GET_INTERFACE):
  {
    constexpr u8 data{1};
    cmd->FillBuffer(&data, sizeof(data));
    cmd->ScheduleTransferCompletion(1, 100);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] REQUEST_SET_INTERFACE index={:04x} value={:04x}",
                 m_vid, m_pid, m_active_interface, cmd->index, cmd->value);
    if (static_cast<u8>(cmd->index) != m_active_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->index));
      if (ret < 0)
      {
        ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Failed to change interface to {}", m_vid, m_pid,
                      m_active_interface, cmd->index);
        return ret;
      }
    }
    const int ret = SetAltSetting(static_cast<u8>(cmd->value));
    if (ret == 0)
      ios.EnqueueIPCReply(cmd->ios_request, cmd->length);
    return ret;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_REPORT):
  {
    // According to the HID specification:
    //  - A device might choose to ignore input Set_Report requests as meaningless.
    //  - Alternatively these reports could be used to reset the origin of a control
    // (that is, current position should report zero).
    //  - The effect of sent reports will also depend on whether the recipient controls
    // are absolute or relative.
    const u8 report_type = cmd->value >> 8;
    const u8 report_id = cmd->value & 0xFF;
    auto& memory = ios.GetSystem().GetMemory();

    // The data seems to report LED status for keys such as:
    //  - NUM LOCK, CAPS LOCK
    const u8* data = memory.GetPointerForRange(cmd->data_address, cmd->length);
    INFO_LOG_FMT(IOS_USB, "SET_REPORT ignored (report_type={:02x}, report_id={:02x}, index={})\n{}",
                 report_type, report_id, cmd->index, HexDump(data, cmd->length));
    ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_IDLE):
  {
    WARN_LOG_FMT(IOS_USB, "SET_IDLE not implemented (value={:04x}, index={})", cmd->value,
                 cmd->index);
    // TODO: Handle idle duration and implement NAK
    ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, HIDRequestCodes::SET_PROTOCOL):
  {
    INFO_LOG_FMT(IOS_USB, "SET_PROTOCOL: value={}, index={}", cmd->value, cmd->index);
    const HIDProtocol protocol = static_cast<HIDProtocol>(cmd->value);
    switch (protocol)
    {
    case HIDProtocol::Boot:
    case HIDProtocol::Report:
      m_current_protocol = protocol;
      break;
    default:
      WARN_LOG_FMT(IOS_USB, "SET_PROTOCOL: Unknown protocol {} for interface {}", cmd->value,
                   cmd->index);
    }
    ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  default:
    WARN_LOG_FMT(IOS_USB, "Unknown command, req={:02x}, type={:02x}", cmd->request,
                 cmd->request_type);
    ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  }

  return IPC_SUCCESS;
}

int HIDKeyboard::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={:04x} endpoint={:02x}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int HIDKeyboard::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  static auto start_time = std::chrono::steady_clock::now();
  const auto current_time = std::chrono::steady_clock::now();

  const bool should_poll =
      (current_time - start_time) >= POLLING_RATE && ControlReference::GetInputGate();
  const Common::HIDPressedState state =
      should_poll ? m_keyboard_context->GetPressedState() : m_last_state;

  // We can't use cmd->ScheduleTransferCompletion here as it might provoke
  // invalid memory access with scheduled transfers when CancelTransfer is called.
  EnqueueTransfer(std::move(cmd), state);

  if (should_poll)
  {
    m_last_state = std::move(state);
    start_time = std::chrono::steady_clock::now();
  }
  return IPC_SUCCESS;
}

int HIDKeyboard::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Isochronous: length={:04x} endpoint={:02x} num_packets={:02x}",
                m_vid, m_pid, m_active_interface, cmd->length, cmd->endpoint, cmd->num_packets);
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, 2000);
  return IPC_SUCCESS;
}

void HIDKeyboard::EnqueueTransfer(std::unique_ptr<IntrMessage> msg,
                                  const Common::HIDPressedState& state)
{
  msg->FillBuffer(reinterpret_cast<const u8*>(&state), sizeof(state));
  auto transfer = std::make_shared<PendingTransfer>(std::move(msg));
  m_worker.EmplaceItem(transfer);
  {
    std::lock_guard lock(m_pending_lock);
    m_pending_tranfers.insert(transfer);
  }
}

void HIDKeyboard::HandlePendingTransfer(std::shared_ptr<PendingTransfer> transfer)
{
  std::unique_lock lock(m_pending_lock);
  if (transfer->IsCanceled())
    return;

  while (!transfer->IsReady())
  {
    lock.unlock();
    std::this_thread::sleep_for(POLLING_RATE / 2);
    lock.lock();

    if (transfer->IsCanceled())
      return;
  }

  transfer->Do();
  m_pending_tranfers.erase(transfer);
}

void HIDKeyboard::CancelPendingTransfers()
{
  m_worker.Cancel();
  {
    std::lock_guard lock(m_pending_lock);
    for (auto& transfer : m_pending_tranfers)
      transfer->Cancel();
    m_pending_tranfers.clear();
  }
}

HIDKeyboard::PendingTransfer::PendingTransfer(std::unique_ptr<IntrMessage> msg)
{
  m_time = std::chrono::steady_clock::now();
  m_msg = std::move(msg);
}

HIDKeyboard::PendingTransfer::~PendingTransfer()
{
  if (!m_pending)
    return;
  // Value based on LibusbDevice's HandleTransfer implementation
  m_msg->ScheduleTransferCompletion(-5, 0);
}

bool HIDKeyboard::PendingTransfer::IsReady() const
{
  return (std::chrono::steady_clock::now() - m_time) >= POLLING_RATE;
}

bool HIDKeyboard::PendingTransfer::IsCanceled() const
{
  return m_is_canceled;
}

void HIDKeyboard::PendingTransfer::Do()
{
  m_msg->ScheduleTransferCompletion(IPC_SUCCESS, 0);
  m_pending = false;
}

void HIDKeyboard::PendingTransfer::Cancel()
{
  m_is_canceled = true;
}
}  // namespace IOS::HLE::USB
