// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Bluetooth/LibUSBBluetoothAdapter.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ranges>
#include <thread>
#include <utility>

#include <fmt/format.h>
#include <libusb.h>

#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Bluetooth/hci.h"

namespace
{
constexpr std::size_t BUFFER_SIZE = 1024;

constexpr auto HCI_COMMAND_TIMEOUT = std::chrono::milliseconds{1000};

constexpr u8 REQUEST_TYPE =
    u8(u8(LIBUSB_ENDPOINT_OUT) | u8(LIBUSB_REQUEST_TYPE_CLASS)) | u8(LIBUSB_RECIPIENT_DEVICE);

constexpr u8 HCI_EVENT = 0x81;
constexpr u8 ACL_DATA_IN = 0x82;

template <auto MemFun>
constexpr libusb_transfer_cb_fn LibUSBMemFunCallback()
{
  return [](libusb_transfer* tr) {
    std::invoke(MemFun, static_cast<Common::ObjectType<MemFun>*>(tr->user_data), tr);
  };
}

}  // namespace

bool LibUSBBluetoothAdapter::IsBluetoothDevice(const libusb_device_descriptor& descriptor)
{
  constexpr u8 SUBCLASS = 0x01;
  constexpr u8 PROTOCOL_BLUETOOTH = 0x01;

  const bool is_bluetooth_protocol = descriptor.bDeviceClass == LIBUSB_CLASS_WIRELESS &&
                                     descriptor.bDeviceSubClass == SUBCLASS &&
                                     descriptor.bDeviceProtocol == PROTOCOL_BLUETOOTH;

  // Some devices misreport their class, so we avoid relying solely on descriptor checks and allow
  // users to specify their own VID/PID.
  return is_bluetooth_protocol || LibUSBBluetoothAdapter::IsConfiguredBluetoothDevice(
                                      descriptor.idVendor, descriptor.idProduct);
}

bool LibUSBBluetoothAdapter::IsWiiBTModule() const
{
  return m_is_wii_bt_module;
}

bool LibUSBBluetoothAdapter::HasConfiguredBluetoothDevice()
{
  const int configured_vid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
  const int configured_pid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
  return configured_vid != -1 && configured_pid != -1;
}

bool LibUSBBluetoothAdapter::IsConfiguredBluetoothDevice(u16 vid, u16 pid)
{
  const int configured_vid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
  const int configured_pid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
  return configured_vid == vid && configured_pid == pid;
}

LibUSBBluetoothAdapter::LibUSBBluetoothAdapter()
{
  if (!m_context.IsValid())
    return;

  m_last_open_error.clear();

  const bool has_configured_bt = HasConfiguredBluetoothDevice();

  const int ret = m_context.GetDeviceList([&](libusb_device* device) {
    libusb_device_descriptor device_descriptor;
    libusb_get_device_descriptor(device, &device_descriptor);
    auto [make_config_descriptor_ret, config_descriptor] =
        LibusbUtils::MakeConfigDescriptor(device);
    if (make_config_descriptor_ret != LIBUSB_SUCCESS || !config_descriptor)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Failed to get config descriptor for device {:04x}:{:04x}: {}",
                    device_descriptor.idVendor, device_descriptor.idProduct,
                    LibusbUtils::ErrorWrap(make_config_descriptor_ret));
      return true;
    }

    if (has_configured_bt &&
        !IsConfiguredBluetoothDevice(device_descriptor.idVendor, device_descriptor.idProduct))
    {
      return true;
    }

    if (IsBluetoothDevice(device_descriptor) && OpenDevice(device_descriptor, device))
    {
      const auto manufacturer =
          LibusbUtils::GetStringDescriptor(m_handle, device_descriptor.iManufacturer).value_or("?");
      const auto product =
          LibusbUtils::GetStringDescriptor(m_handle, device_descriptor.iProduct).value_or("?");
      const auto serial_number =
          LibusbUtils::GetStringDescriptor(m_handle, device_descriptor.iSerialNumber).value_or("?");
      NOTICE_LOG_FMT(IOS_WIIMOTE, "Using device {:04x}:{:04x} (rev {:x}) for Bluetooth: {} {} {}",
                     device_descriptor.idVendor, device_descriptor.idProduct,
                     device_descriptor.bcdDevice, manufacturer, product, serial_number);
      m_is_wii_bt_module =
          device_descriptor.idVendor == 0x57e && device_descriptor.idProduct == 0x305;
      return false;
    }
    return true;
  });
  if (ret != LIBUSB_SUCCESS)
  {
    m_last_open_error =
        Common::FmtFormatT("GetDeviceList failed: {0}", LibusbUtils::ErrorWrap(ret));
  }

  if (m_handle == nullptr)
  {
    if (m_last_open_error.empty())
    {
      CriticalAlertFmtT(
          "Could not find any usable Bluetooth USB adapter for Bluetooth Passthrough.\n\n"
          "The emulated console will now stop.");
    }
    else
    {
      CriticalAlertFmtT(
          "Could not find any usable Bluetooth USB adapter for Bluetooth Passthrough.\n"
          "The following error occurred when Dolphin tried to use an adapter:\n{0}\n\n"
          "The emulated console will now stop.",
          m_last_open_error);
    }
    Core::QueueHostJob(&Core::Stop);
    return;
  }

  StartInputTransfers();

  m_output_worker.Reset("Bluetooth Output",
                        std::bind_front(&LibUSBBluetoothAdapter::SubmitTimedTransfer, this));
}

LibUSBBluetoothAdapter::~LibUSBBluetoothAdapter()
{
  if (m_handle == nullptr)
    return;

  // Wait for completion (or time out) of all HCI commands.
  while (!m_pending_hci_transfers.empty() && !m_unacknowledged_commands.empty())
  {
    (void)ReceiveHCIEvent();
    Common::YieldCPU();
  }

  m_output_worker.Shutdown();

  // Stop the repeating input transfers.
  if (std::lock_guard lg{m_transfers_mutex}; true)
    m_run_input_transfers = false;

  // Cancel all LibUSB transfers.
  std::ranges::for_each(m_transfer_buffers | std::ranges::views::keys, libusb_cancel_transfer);

  // Wait for transfer callbacks and clean up all the buffers.
  while (!m_transfer_buffers.empty())
  {
    m_transfers_to_free.WaitForData();
    CleanCompletedTransfers();
  }

  const int ret = libusb_release_interface(m_handle, 0);
  if (ret != LIBUSB_SUCCESS)
    WARN_LOG_FMT(IOS_WIIMOTE, "libusb_release_interface failed: {}", LibusbUtils::ErrorWrap(ret));

  libusb_close(std::exchange(m_handle, nullptr));
}

void LibUSBBluetoothAdapter::Update()
{
  // Remove timed out commands.
  const auto expired_time = Clock::now() - HCI_COMMAND_TIMEOUT;
  while (!m_unacknowledged_commands.empty() &&
         m_unacknowledged_commands.front().submit_time < expired_time)
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "HCI command 0x{:04x} timed out.",
                 m_unacknowledged_commands.front().opcode);
    m_unacknowledged_commands.pop_front();
  }

  // Allow sending commands if none are pending acknowledgement.
  if (m_unacknowledged_commands.empty())
    m_num_hci_command_packets = 1;

  // Push queued commands when the controller is ready for them.
  if (!m_pending_hci_transfers.empty() && IsControllerReadyForCommand())
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "Submitting queued HCI command.");

    PushHCICommand(m_pending_hci_transfers.front());
    m_pending_hci_transfers.pop();
  }

  CleanCompletedTransfers();
}

auto LibUSBBluetoothAdapter::ReceiveHCIEvent() -> BufferType
{
  if (m_hci_event_queue.Empty())
  {
    Update();
    return {};
  }

  auto buffer = std::move(m_hci_event_queue.Front());
  m_hci_event_queue.Pop();

  // We only care about "COMMAND_COMPL" and "COMMAND_STATUS" events here.
  // The controller will reply with one of those for every command.
  // We track which commands are still "in flight" to properly obey Num_HCI_Command_Packets.
  const auto event = buffer[0];
  if (event == HCI_EVENT_COMMAND_COMPL)
  {
    AcknowledgeCommand<hci_command_compl_ep>(buffer);
  }
  else if (event == HCI_EVENT_COMMAND_STATUS)
  {
    AcknowledgeCommand<hci_command_status_ep>(buffer);
  }

  Update();
  return buffer;
}

auto LibUSBBluetoothAdapter::ReceiveACLData() -> BufferType
{
  BufferType buffer;
  m_acl_data_queue.Pop(buffer);
  return buffer;
}

bool LibUSBBluetoothAdapter::IsControllerReadyForCommand() const
{
  return m_num_hci_command_packets > m_unacknowledged_commands.size();
}

template <typename EventType>
void LibUSBBluetoothAdapter::AcknowledgeCommand(std::span<const u8> buffer)
{
  if (buffer.size() < sizeof(hci_event_hdr_t) + sizeof(EventType)) [[unlikely]]
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "Undersized HCI event");
    return;
  }

  EventType ev;
  std::memcpy(&ev, buffer.data() + sizeof(hci_event_hdr_t), sizeof(ev));

  const auto it =
      std::ranges::find(m_unacknowledged_commands, ev.opcode, &OutstandingCommand::opcode);
  if (it != m_unacknowledged_commands.end())
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI command acknowledged: 0x{:04x}", ev.opcode);
    m_unacknowledged_commands.erase(it);
  }
  else if (ev.opcode != 0x0000)
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "Unexpected opcode acknowledgement: 0x{:04x}", ev.opcode);
  }

  m_num_hci_command_packets = ev.num_cmd_pkts;
}

libusb_transfer* LibUSBBluetoothAdapter::AllocateTransfer(std::size_t buffer_size)
{
  auto& item =
      *m_transfer_buffers.emplace(libusb_alloc_transfer(0), Common::UniqueBuffer<u8>{buffer_size})
           .first;

  auto* const transfer = item.first;
  transfer->buffer = item.second.get();

  return transfer;
}

void LibUSBBluetoothAdapter::FreeTransfer(libusb_transfer* transfer)
{
  libusb_free_transfer(transfer);
  m_transfer_buffers.erase(transfer);
}

void LibUSBBluetoothAdapter::CleanCompletedTransfers()
{
  libusb_transfer* transfer{};
  while (m_transfers_to_free.Pop(transfer))
    FreeTransfer(transfer);
}

void LibUSBBluetoothAdapter::PushHCICommand(TimedTransfer transfer)
{
  hci_cmd_hdr_t cmd;
  std::memcpy(&cmd, libusb_control_transfer_get_data(transfer.transfer), sizeof(cmd));

  // Push the time forward so our timeouts work properly if command was queued and delayed.
  const auto submit_time = std::max(transfer.target_time, Clock::now());

  m_unacknowledged_commands.emplace_back(OutstandingCommand{cmd.opcode, submit_time});

  // This function is only invoked when this value is at least 1.
  assert(m_num_hci_command_packets >= 1);
  --m_num_hci_command_packets;

  m_output_worker.EmplaceItem(transfer);
}

void LibUSBBluetoothAdapter::SubmitTimedTransfer(TimedTransfer transfer)
{
  if (Config::Get(Config::MAIN_PRECISION_FRAME_TIMING))
    m_precision_timer.SleepUntil(transfer.target_time);
  else
    std::this_thread::sleep_until(transfer.target_time);

  SubmitTransfer(transfer.transfer);
}

void LibUSBBluetoothAdapter::SubmitTransfer(libusb_transfer* transfer)
{
  const int ret = libusb_submit_transfer(transfer);

  if (ret == LIBUSB_SUCCESS)
    return;

  ERROR_LOG_FMT(IOS_WIIMOTE, "libusb_submit_transfer: {}", LibusbUtils::ErrorWrap(ret));

  // Failed transers will not invoke callbacks so we must mark the buffer to be free'd here.
  std::lock_guard lk{m_transfers_mutex};
  m_transfers_to_free.Emplace(transfer);
}

void LibUSBBluetoothAdapter::ScheduleBulkTransfer(u8 endpoint, std::span<const u8> data,
                                                  TimePoint target_time)
{
  constexpr auto callback = LibUSBMemFunCallback<&LibUSBBluetoothAdapter::HandleOutputTransfer>();

  auto* const transfer = AllocateTransfer(data.size());
  libusb_fill_bulk_transfer(transfer, m_handle, endpoint, transfer->buffer, int(data.size()),
                            callback, this, 0);

  std::ranges::copy(data, transfer->buffer);

  m_output_worker.EmplaceItem(transfer, target_time);
}

void LibUSBBluetoothAdapter::ScheduleControlTransfer(u8 type, u8 request, u8 value, u8 index,
                                                     std::span<const u8> data,
                                                     TimePoint target_time)
{
  constexpr auto callback = LibUSBMemFunCallback<&LibUSBBluetoothAdapter::HandleOutputTransfer>();

  auto* const transfer = AllocateTransfer(data.size() + LIBUSB_CONTROL_SETUP_SIZE);

  libusb_fill_control_setup(transfer->buffer, type, request, value, index, u16(data.size()));
  libusb_fill_control_transfer(transfer, m_handle, transfer->buffer, callback, this, 0);

  std::ranges::copy(data, transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE);

  const TimedTransfer timed_transfer{transfer, target_time};

  if (IsControllerReadyForCommand())
  {
    PushHCICommand(timed_transfer);
  }
  else
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "Queueing HCI command while controller is busy.");
    m_pending_hci_transfers.emplace(timed_transfer);
  }
}

void LibUSBBluetoothAdapter::SendControlTransfer(std::span<const u8> data)
{
  ScheduleControlTransfer(REQUEST_TYPE, 0, 0, 0, data, Clock::now());
}

void LibUSBBluetoothAdapter::StartInputTransfers()
{
  constexpr auto callback = LibUSBMemFunCallback<&LibUSBBluetoothAdapter::HandleInputTransfer>();

  // Incoming HCI events.
  {
    auto* const transfer = AllocateTransfer(BUFFER_SIZE);
    libusb_fill_interrupt_transfer(transfer, m_handle, HCI_EVENT, transfer->buffer, BUFFER_SIZE,
                                   callback, this, 0);
    SubmitTransfer(transfer);
  }

  // Incoming ACL data.
  {
    auto* const transfer = AllocateTransfer(BUFFER_SIZE);
    libusb_fill_bulk_transfer(transfer, m_handle, ACL_DATA_IN, transfer->buffer, BUFFER_SIZE,
                              callback, this, 0);
    SubmitTransfer(transfer);
  }
}

bool LibUSBBluetoothAdapter::OpenDevice(const libusb_device_descriptor& device_descriptor,
                                        libusb_device* device)
{
  const int ret = libusb_open(device, &m_handle);
  if (ret != LIBUSB_SUCCESS)
  {
    m_last_open_error = Common::FmtFormatT("Failed to open Bluetooth device {:04x}:{:04x}: {}",
                                           device_descriptor.idVendor, device_descriptor.idProduct,
                                           LibusbUtils::ErrorWrap(ret));
    return false;
  }

// Detaching always fails as a regular user on FreeBSD
// https://lists.freebsd.org/pipermail/freebsd-usb/2016-March/014161.html
#ifndef __FreeBSD__
  int result = libusb_set_auto_detach_kernel_driver(m_handle, 1);
  if (result != LIBUSB_SUCCESS)
  {
    result = libusb_detach_kernel_driver(m_handle, INTERFACE);
    if (result != LIBUSB_SUCCESS && result != LIBUSB_ERROR_NOT_FOUND &&
        result != LIBUSB_ERROR_NOT_SUPPORTED)
    {
      m_last_open_error = Common::FmtFormatT(
          "Failed to detach kernel driver for BT passthrough: {0}", LibusbUtils::ErrorWrap(result));
      return false;
    }
  }
#endif
  if (const int result2 = libusb_claim_interface(m_handle, INTERFACE); result2 != LIBUSB_SUCCESS)
  {
    m_last_open_error = Common::FmtFormatT("Failed to claim interface for BT passthrough: {0}",
                                           LibusbUtils::ErrorWrap(result2));
    return false;
  }

  return true;
}

void LibUSBBluetoothAdapter::HandleOutputTransfer(libusb_transfer* tr)
{
  HandleTransferError(tr);

  std::lock_guard lg{m_transfers_mutex};
  m_transfers_to_free.Emplace(tr);
}

void LibUSBBluetoothAdapter::HandleInputTransfer(libusb_transfer* tr)
{
  if (tr->status == LIBUSB_TRANSFER_COMPLETED)
  {
    const bool is_hci_event = tr->endpoint == HCI_EVENT;
    auto& queue = is_hci_event ? m_hci_event_queue : m_acl_data_queue;

    if (queue.Size() < MAX_INPUT_QUEUE_SIZE)
    {
      if (tr->actual_length != 0)
      {
        BufferType buffer(tr->actual_length);
        std::copy_n(tr->buffer, tr->actual_length, buffer.data());
        queue.Emplace(std::move(buffer));
      }

      m_showed_long_queue_drop = false;
    }
    else if (!std::exchange(m_showed_long_queue_drop, true))
    {
      // This will happen when pausing the emulator.
      WARN_LOG_FMT(IOS_WIIMOTE, "{} queue is too long. Packets will be dropped.",
                   (is_hci_event ? "HCI" : "ACL"));
    }
  }

  HandleTransferError(tr);

  std::lock_guard lg{m_transfers_mutex};

  if (m_run_input_transfers)
  {
    // Continuously repeat this input transfer so long as we're still running.
    const auto ret = libusb_submit_transfer(tr);
    if (ret == LIBUSB_SUCCESS)
      return;

    ERROR_LOG_FMT(IOS_WIIMOTE, "HandleInputTransfer libusb_submit_transfer: {}",
                  LibusbUtils::ErrorWrap(ret));
  }

  m_transfers_to_free.Emplace(tr);
}

void LibUSBBluetoothAdapter::HandleTransferError(libusb_transfer* transfer)
{
  if (transfer->status != LIBUSB_TRANSFER_COMPLETED &&
      transfer->status != LIBUSB_TRANSFER_CANCELLED)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "libusb transfer failed: {}",
                  LibusbUtils::ErrorWrap(transfer->status));
    if (!std::exchange(m_showed_failed_transfer, true))
    {
      Core::DisplayMessage("Failed to send a command to the Bluetooth adapter.", 10000);
      Core::DisplayMessage("It may not be compatible with passthrough mode.", 10000);
    }
  }
  else
  {
    m_showed_failed_transfer = false;
  }
}
