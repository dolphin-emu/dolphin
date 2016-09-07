// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WII_IPC.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_57e_305.h"
#include "Core/IPC_HLE/hci.h"

// This flag is set when we want to send a sync button event (to trigger a BT inquiry)
// and is cleared after the event is sent.
static Common::Flag s_trigger_sync_button;

static void EnqueueReply(const u32 command_address,
                         CoreTiming::FromThread from_thread = CoreTiming::FromThread::CPU)
{
  Memory::Write_U32(Memory::Read_U32(command_address), command_address + 8);
  Memory::Write_U32(IPC_REP_ASYNC, command_address);
  WII_IPC_HLE_Interface::EnqueueReply(command_address, 0, from_thread);
}

static bool IsWantedDevice(libusb_device_descriptor& descriptor)
{
  const int vid = SConfig::GetInstance().m_bt_passthrough_vid;
  const int pid = SConfig::GetInstance().m_bt_passthrough_pid;
  if (vid == -1 || pid == -1)
    return true;
  return descriptor.idVendor == vid && descriptor.idProduct == pid;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305::CWII_IPC_HLE_Device_usb_oh1_57e_305(
    u32 device_id, const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
  _assert_msg_(WII_IPC_WIIMOTE, libusb_init(&m_libusb_context) == 0, "Failed to init libusb.");

  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Couldn't get device list");
    return;
  }

  libusb_device* device;
  int i = 0;
  while ((device = list[i++]))
  {
    libusb_device_descriptor device_descriptor;
    libusb_config_descriptor* config_descriptor;
    if (libusb_get_device_descriptor(device, &device_descriptor) != 0 ||
        libusb_get_active_config_descriptor(device, &config_descriptor) != 0)
    {
      ERROR_LOG(WII_IPC_WIIMOTE, "Failed to get device or config descriptor");
      continue;
    }

    const libusb_interface* interface = &config_descriptor->interface[INTERFACE];
    const libusb_interface_descriptor* descriptor = &interface->altsetting[0];
    if (descriptor->bNumEndpoints == 3 && descriptor->bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
        descriptor->bInterfaceProtocol == PROTOCOL_BLUETOOTH && IsWantedDevice(device_descriptor) &&
        OpenDevice(device))
    {
      NOTICE_LOG(WII_IPC_WIIMOTE, "Using device %02x:%02x for Bluetooth",
                 device_descriptor.idVendor, device_descriptor.idProduct);
      libusb_free_config_descriptor(config_descriptor);
      break;
    }
    libusb_free_config_descriptor(config_descriptor);
  }
  libusb_free_device_list(list, 1);
  _assert_msg_(WII_IPC_WIIMOTE, m_handle != nullptr, "No usable Bluetooth USB device.");
  StartThread();
}

CWII_IPC_HLE_Device_usb_oh1_57e_305::~CWII_IPC_HLE_Device_usb_oh1_57e_305()
{
  SendHCIResetCommand();

  libusb_release_interface(m_handle, 0);
  StopThread();
  libusb_unref_device(m_device);
  libusb_exit(m_libusb_context);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::Open(u32 command_address, u32 mode)
{
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::Close(u32 command_address, bool force)
{
  if (!force)
    Memory::Write_U32(0, command_address + 4);
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtl(u32 command_address)
{
  // NeoGamma (homebrew) is known to use this path.
  ERROR_LOG(WII_IPC_WIIMOTE, "Bad IOCtl in CWII_IPC_HLE_Device_usb_oh1_57e_305");
  Memory::Write_U32(FS_EINVAL, command_address + 4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtlV(u32 command_address)
{
  const SIOCtlVBuffer cmd_buffer(command_address);
  Memory::Write_U32(0, command_address + 4);

  switch (cmd_buffer.Parameter)
  {
  // HCI commands to the Bluetooth adapter
  case USBV0_IOCTL_CTRLMSG:
  {
    const CtrlMessage message(cmd_buffer);
    const auto opcode = Memory::Read_U16(message.payload_addr);
    switch (opcode)
    {
    case 0xFC4C:
    case 0xFC4F:
      hci_status_rp reply;
      reply.status = 0x00;
      FakeEventCommandComplete(opcode, &reply, sizeof(hci_status_rp), message);
      break;
    default:
      SendHCICommandPacket(message);
    }
    EnqueueReply(message.address);
    break;
  }
  // ACL data (incoming or outgoing) and incoming HCI events (respectively)
  case USBV0_IOCTL_BLKMSG:
  case USBV0_IOCTL_INTRMSG:
  {
    auto* buffer = new CtrlBuffer(cmd_buffer, command_address);
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG && s_trigger_sync_button.IsSet())
    {
      Core::DisplayMessage("Scanning for Wiimotes", 2000);
      FakeSyncButtonEvent(*buffer);
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
    transfer->user_data = buffer;
    libusb_submit_transfer(transfer);
    break;
  }
  }
  // Replies are generated inside of the message handlers (and asynchronously).
  return GetNoReply();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = true;
  p.Do(passthrough_bluetooth);
  // TODO: fix state loading. Currently it completely breaks commands and transfers, as a reset
  // throws away the adapter's state; but without a reset we end up in an inconsistent state.
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    PanicAlert("Attempted to load a state. Bluetooth will likely be broken now.");
  }
  if (!passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be disabled. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendHCICommandPacket(const CtrlMessage& cmd)
{
  u8* packet = Memory::GetPointer(cmd.payload_addr);
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  const int ret = libusb_control_transfer(m_handle, type, 0, 0, 0, packet, cmd.length, TIMEOUT);
  if (ret < 0)
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to send HCI command: %s", libusb_error_name(ret));
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendHCIResetCommand()
{
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  u8 packet[3] = {};
  *reinterpret_cast<u16*>(&packet[0]) = HCI_CMD_RESET;
  libusb_control_transfer(m_handle, type, 0, 0, 0, packet, sizeof(packet), TIMEOUT);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::FakeEventCommandComplete(const u16 opcode,
                                                                   const void* data,
                                                                   const u32 data_size,
                                                                   const CtrlMessage& ctrl)
{
  WARN_LOG(WII_IPC_WIIMOTE, "Faking event command complete packet");
  u8* packet = Memory::GetPointer(ctrl.payload_addr);
  auto* hci_event = reinterpret_cast<HCIEventCommand*>(packet);
  hci_event->event_type = HCI_EVENT_COMMAND_COMPL;
  hci_event->payload_length = (u8)(sizeof(HCIEventCommand) - 2 + data_size);
  hci_event->packet_indicator = 0x01;
  hci_event->opcode = opcode;

  if (data != nullptr && data_size > 0)
  {
    u8* payload = packet + sizeof(HCIEventCommand);
    memcpy(payload, data, data_size);
  }

  // return value
  Memory::Write_U32(sizeof(HCIEventCommand) + data_size, ctrl.address + 4);
}

// When the red sync button is pressed, a HCI event is generated:
//   > HCI Event: Vendor (0xff) plen 1
//   08
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void CWII_IPC_HLE_Device_usb_oh1_57e_305::FakeSyncButtonEvent(const CtrlBuffer& ctrl)
{
  NOTICE_LOG(WII_IPC_WIIMOTE, "Faking sync button event packet");
  const u8 payload[1] = {0x08};
  u8* packet = Memory::GetPointer(ctrl.m_payload_addr);
  auto* hci_event = reinterpret_cast<hci_event_hdr_t*>(packet);
  hci_event->event = HCI_EVENT_VENDOR;
  hci_event->length = sizeof(payload);
  memcpy(packet + sizeof(hci_event_hdr_t), payload, sizeof(payload));

  ctrl.SetRetVal(sizeof(hci_event_hdr_t) + sizeof(payload));
  EnqueueReply(ctrl.m_cmd_address);
  s_trigger_sync_button.Clear();
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::TriggerSyncButtonEvent()
{
  s_trigger_sync_button.Set();
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::OpenDevice(libusb_device* device)
{
  m_device = libusb_ref_device(device);
  const int ret = libusb_open(m_device, &m_handle);
  if (ret != 0)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to open Bluetooth device: %s", libusb_error_name(ret));
    return false;
  }

  const int result = libusb_detach_kernel_driver(m_handle, INTERFACE);
  if (result < 0 && result != LIBUSB_ERROR_NOT_FOUND)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to detach kernel driver: %s", libusb_error_name(result));
    return false;
  }
  if (libusb_claim_interface(m_handle, INTERFACE) < 0)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to claim interface");
    return false;
  }

  return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::StartThread()
{
  if (m_thread_running.IsSet())
    return;
  m_thread_running.Set();
  m_thread = std::thread(&CWII_IPC_HLE_Device_usb_oh1_57e_305::ThreadFunc, this);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::StopThread()
{
  if (m_thread_running.TestAndClear())
  {
    libusb_close(m_handle);
    m_thread.join();
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::ThreadFunc()
{
  Common::SetCurrentThreadName("BT USB Thread");
  while (m_thread_running.IsSet())
  {
    libusb_handle_events_completed(m_libusb_context, nullptr);
  }
}

// This is called from libusb code on a separate thread.
void CWII_IPC_HLE_Device_usb_oh1_57e_305::TransferCallback(libusb_transfer* tr)
{
  const auto* ctrl = static_cast<CtrlBuffer*>(tr->user_data);
  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_TIMED_OUT)
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "libusb transfer failed, status: 0x%02x", tr->status);
    ctrl->SetRetVal(0);
    EnqueueReply(ctrl->m_cmd_address, CoreTiming::FromThread::NON_CPU);
    return;
  }
  ctrl->FillBuffer(tr->buffer, tr->actual_length);
  ctrl->SetRetVal(tr->actual_length);
  EnqueueReply(ctrl->m_cmd_address, CoreTiming::FromThread::NON_CPU);
  delete ctrl;
}
