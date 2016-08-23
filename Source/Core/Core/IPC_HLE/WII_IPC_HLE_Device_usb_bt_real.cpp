// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include <libusb.h>

#include "Common/Network.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "Core/IPC_HLE/hci.h"

// This flag is set when a libusb transfer failed (for reasons other than timing out)
// and we showed an OSD message about it.
static Common::Flag s_showed_failed_transfer;

static void EnqueueReply(const u32 command_address)
{
  Memory::Write_U32(Memory::Read_U32(command_address), command_address + 8);
  Memory::Write_U32(IPC_REP_ASYNC, command_address);
  WII_IPC_HLE_Interface::EnqueueReply(command_address, 0, CoreTiming::FromThread::ANY);
}

static bool IsWantedDevice(libusb_device_descriptor& descriptor)
{
  const int vid = SConfig::GetInstance().m_bt_passthrough_vid;
  const int pid = SConfig::GetInstance().m_bt_passthrough_pid;
  if (vid == -1 || pid == -1)
    return true;
  return descriptor.idVendor == vid && descriptor.idProduct == pid;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_real::CWII_IPC_HLE_Device_usb_oh1_57e_305_real(
    u32 device_id, const std::string& device_name)
    : CWII_IPC_HLE_Device_usb_oh1_57e_305_base(device_id, device_name)
{
  const int ret = libusb_init(&m_libusb_context);
  _assert_msg_(WII_IPC_WIIMOTE, ret == 0, "Failed to init libusb.");
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_real::~CWII_IPC_HLE_Device_usb_oh1_57e_305_real()
{
  if (m_handle != nullptr)
  {
    SendHCIResetCommand();
    libusb_release_interface(m_handle, 0);
    // libusb_handle_events() may block the libusb thread indefinitely, so we need to
    // call libusb_close() first then immediately stop the thread in StopTransferThread.
    StopTransferThread();
    libusb_unref_device(m_device);
  }

  libusb_exit(m_libusb_context);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_real::Open(u32 command_address, u32 mode)
{
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  _dbg_assert_msg_(WII_IPC_HLE, cnt > 0, "Couldn't get device list");
  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device* device = list[i];
    libusb_device_descriptor device_descriptor;
    libusb_config_descriptor* config_descriptor;
    libusb_get_device_descriptor(device, &device_descriptor);
    const int ret = libusb_get_active_config_descriptor(device, &config_descriptor);
    if (ret != 0)
    {
      ERROR_LOG(WII_IPC_WIIMOTE, "Failed to get config descriptor for device %04x:%04x: %s",
                device_descriptor.idVendor, device_descriptor.idProduct, libusb_error_name(ret));
      continue;
    }

    const libusb_interface& interface = config_descriptor->interface[INTERFACE];
    const libusb_interface_descriptor& descriptor = interface.altsetting[0];
    if (descriptor.bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
        descriptor.bInterfaceSubClass == SUBCLASS &&
        descriptor.bInterfaceProtocol == PROTOCOL_BLUETOOTH && IsWantedDevice(device_descriptor) &&
        OpenDevice(device))
    {
      unsigned char manufacturer[50] = {}, product[50] = {}, serial_number[50] = {};
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iManufacturer, manufacturer,
                                         sizeof(manufacturer));
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iProduct, product,
                                         sizeof(product));
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iSerialNumber, serial_number,
                                         sizeof(serial_number));
      NOTICE_LOG(WII_IPC_WIIMOTE, "Using device %04x:%04x (rev %x) for Bluetooth: %s %s %s",
                 device_descriptor.idVendor, device_descriptor.idProduct,
                 device_descriptor.bcdDevice, manufacturer, product, serial_number);
      libusb_free_config_descriptor(config_descriptor);
      break;
    }
    libusb_free_config_descriptor(config_descriptor);
  }
  libusb_free_device_list(list, 1);

  if (m_handle == nullptr)
  {
    PanicAlertT("Bluetooth passthrough mode is enabled, "
                "but no usable Bluetooth USB device was found. Aborting.");
    Core::QueueHostJob(Core::Stop);
    return GetNoReply();
  }

  StartTransferThread();

  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_real::Close(u32 command_address, bool force)
{
  if (!force)
  {
    libusb_release_interface(m_handle, 0);
    StopTransferThread();
    libusb_unref_device(m_device);
    m_handle = nullptr;
    Memory::Write_U32(0, command_address + 4);
  }

  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_real::IOCtlV(u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  switch (cmd_buffer.Parameter)
  {
  // HCI commands to the Bluetooth adapter
  case USBV0_IOCTL_CTRLMSG:
  {
    auto cmd = std::make_unique<CtrlMessage>(cmd_buffer);
    auto buffer = std::vector<u8>(cmd->length + LIBUSB_CONTROL_SETUP_SIZE);
    libusb_fill_control_setup(buffer.data(), cmd->request_type, cmd->request, cmd->value,
                              cmd->index, cmd->length);
    Memory::CopyFromEmu(buffer.data() + LIBUSB_CONTROL_SETUP_SIZE, cmd->payload_addr, cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_fill_control_transfer(transfer, m_handle, buffer.data(), CommandCallback, cmd.release(),
                                 0);
    libusb_submit_transfer(transfer);
    break;
  }
  // ACL data (incoming or outgoing) and incoming HCI events (respectively)
  case USBV0_IOCTL_BLKMSG:
  case USBV0_IOCTL_INTRMSG:
  {
    auto buffer = std::make_unique<CtrlBuffer>(cmd_buffer, command_address);
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG &&
        m_sync_button_state == SyncButtonState::Pressed)
    {
      Core::DisplayMessage("Scanning for Wiimotes", 2000);
      FakeSyncButtonPressedEvent(*buffer);
      return GetNoReply();
    }
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG &&
        m_sync_button_state == SyncButtonState::LongPressed)
    {
      Core::DisplayMessage("Reset saved Wiimote pairings", 2000);
      FakeSyncButtonHeldEvent(*buffer);
      return GetNoReply();
    }
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->buffer = Memory::GetPointer(buffer->m_payload_addr);
    transfer->callback = TransferCallback;
    transfer->dev_handle = m_handle;
    transfer->endpoint = buffer->m_endpoint;
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    transfer->length = buffer->m_length;
    transfer->timeout = TIMEOUT;
    transfer->type = cmd_buffer.Parameter == USBV0_IOCTL_BLKMSG ? LIBUSB_TRANSFER_TYPE_BULK :
                                                                  LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->user_data = buffer.release();
    libusb_submit_transfer(transfer);
    break;
  }
  }
  // Replies are generated inside of the message handlers (and asynchronously).
  return GetNoReply();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = true;
  p.Do(passthrough_bluetooth);
  if (p.GetMode() == PointerWrap::MODE_READ)
    PanicAlertT("Attempted to load a state. Bluetooth will likely be broken now.");

  if (!passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be disabled. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::UpdateSyncButtonState(const bool is_held)
{
  if (m_sync_button_state == SyncButtonState::Unpressed && is_held)
  {
    m_sync_button_held_timer.Update();
    m_sync_button_state = SyncButtonState::Held;
  }

  if (m_sync_button_state == SyncButtonState::Held && is_held &&
      m_sync_button_held_timer.GetTimeDifference() > SYNC_BUTTON_HOLD_MS_TO_RESET)
    m_sync_button_state = SyncButtonState::LongPressed;
  else if (m_sync_button_state == SyncButtonState::Held && !is_held)
    m_sync_button_state = SyncButtonState::Pressed;

  if (m_sync_button_state == SyncButtonState::Ignored && !is_held)
    m_sync_button_state = SyncButtonState::Unpressed;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonPressedEvent()
{
  m_sync_button_state = SyncButtonState::Pressed;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonHeldEvent()
{
  m_sync_button_state = SyncButtonState::LongPressed;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::SendHCIResetCommand()
{
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  u8 packet[3] = {};
  const u16 payload[] = {HCI_CMD_RESET};
  memcpy(packet, payload, sizeof(payload));
  libusb_control_transfer(m_handle, type, 0, 0, 0, packet, sizeof(packet), TIMEOUT);
  INFO_LOG(WII_IPC_WIIMOTE, "Sent a reset command to adapter");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::FakeSyncButtonEvent(const CtrlBuffer& ctrl,
                                                                   const u8* payload, const u8 size)
{
  u8* packet = Memory::GetPointer(ctrl.m_payload_addr);
  auto* hci_event = reinterpret_cast<hci_event_hdr_t*>(packet);
  hci_event->event = HCI_EVENT_VENDOR;
  hci_event->length = size;
  memcpy(packet + sizeof(hci_event_hdr_t), payload, size);
  ctrl.SetRetVal(sizeof(hci_event_hdr_t) + size);
  EnqueueReply(ctrl.m_cmd_address);
}

// When the red sync button is pressed, a HCI event is generated:
//   > HCI Event: Vendor (0xff) plen 1
//   08
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::FakeSyncButtonPressedEvent(const CtrlBuffer& ctrl)
{
  NOTICE_LOG(WII_IPC_WIIMOTE, "Faking 'sync button pressed' (0x08) event packet");
  const u8 payload[1] = {0x08};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::FakeSyncButtonHeldEvent(const CtrlBuffer& ctrl)
{
  NOTICE_LOG(WII_IPC_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  const u8 payload[1] = {0x09};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305_real::OpenDevice(libusb_device* device)
{
  m_device = libusb_ref_device(device);
  const int ret = libusb_open(m_device, &m_handle);
  if (ret != 0)
  {
    PanicAlertT("Failed to open Bluetooth device: %s", libusb_error_name(ret));
    return false;
  }

  const int result = libusb_detach_kernel_driver(m_handle, INTERFACE);
  if (result < 0 && result != LIBUSB_ERROR_NOT_FOUND && result != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    PanicAlertT("Failed to detach kernel driver for BT passthrough: %s", libusb_error_name(result));
    return false;
  }
  if (libusb_claim_interface(m_handle, INTERFACE) < 0)
  {
    PanicAlertT("Failed to claim interface for BT passthrough");
    return false;
  }

  return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::StartTransferThread()
{
  if (m_thread_running.IsSet())
    return;
  m_thread_running.Set();
  m_thread = std::thread(&CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TransferThread, this);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::StopTransferThread()
{
  if (m_thread_running.TestAndClear())
  {
    libusb_close(m_handle);
    m_thread.join();
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TransferThread()
{
  Common::SetCurrentThreadName("BT USB Thread");
  while (m_thread_running.IsSet())
  {
    libusb_handle_events_completed(m_libusb_context, nullptr);
  }
}

// The callbacks are called from libusb code on a separate thread.
void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::CommandCallback(libusb_transfer* tr)
{
  const std::unique_ptr<CtrlMessage> cmd(static_cast<CtrlMessage*>(tr->user_data));
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "libusb command transfer failed, status: 0x%02x", tr->status);
    if (!s_showed_failed_transfer.IsSet())
    {
      Core::DisplayMessage("Failed to send a command to the Bluetooth adapter.", 10000);
      Core::DisplayMessage("It may not be compatible with passthrough mode.", 10000);
      s_showed_failed_transfer.Set();
    }
  }
  else
  {
    s_showed_failed_transfer.Clear();
  }

  EnqueueReply(cmd->address);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TransferCallback(libusb_transfer* tr)
{
  const std::unique_ptr<CtrlBuffer> ctrl(static_cast<CtrlBuffer*>(tr->user_data));
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_TIMED_OUT &&
      tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "libusb transfer failed, status: 0x%02x", tr->status);
    if (!s_showed_failed_transfer.IsSet())
    {
      Core::DisplayMessage("Failed to transfer to or from to the Bluetooth adapter.", 10000);
      Core::DisplayMessage("It may not be compatible with passthrough mode.", 10000);
      s_showed_failed_transfer.Set();
    }
  }
  else
  {
    s_showed_failed_transfer.Clear();
  }
  ctrl->SetRetVal(tr->actual_length);
  EnqueueReply(ctrl->m_cmd_address);
}
