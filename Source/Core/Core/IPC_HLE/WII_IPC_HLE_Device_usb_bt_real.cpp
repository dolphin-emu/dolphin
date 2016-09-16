// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#include "Common/Network.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_real.h"
#include "Core/IPC_HLE/hci.h"

static std::atomic<SyncButtonState> s_sync_button_state{SyncButtonState::Unpressed};
static Common::Timer s_sync_button_held_timer;  // when the sync button started to be held

static std::atomic<LinkKeysState> s_link_keys_state;
// This stores the address of paired devices and associated link keys.
// It is needed because some adapters forget all stored link keys when they are reset,
// which breaks pairings because the Wii relies on the Bluetooth module to remember them.
static std::map<btaddr_t, linkkey_t> s_link_keys;

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

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::UpdateSyncButtonState(const bool is_held)
{
  if (s_sync_button_state == SyncButtonState::Unpressed && is_held)
  {
    s_sync_button_held_timer.Update();
    s_sync_button_state = SyncButtonState::Held;
  }

  if (s_sync_button_state == SyncButtonState::Held && is_held &&
      s_sync_button_held_timer.GetTimeDifference() > SYNC_BUTTON_HOLD_MS_TO_RESET)
    s_sync_button_state = SyncButtonState::LongPressed;
  else if (s_sync_button_state == SyncButtonState::Held && !is_held)
    s_sync_button_state = SyncButtonState::Pressed;

  if (s_sync_button_state == SyncButtonState::Ignored && !is_held)
    s_sync_button_state = SyncButtonState::Unpressed;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonPressedEvent()
{
  s_sync_button_state = SyncButtonState::Pressed;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TriggerSyncButtonHeldEvent()
{
  s_sync_button_state = SyncButtonState::LongPressed;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_real::CWII_IPC_HLE_Device_usb_oh1_57e_305_real(
    u32 device_id, const std::string& device_name)
    : CWII_IPC_HLE_Device_usb_oh1_57e_305_base(device_id, device_name)
{
  _assert_msg_(WII_IPC_WIIMOTE, libusb_init(&m_libusb_context) == 0, "Failed to init libusb.");

  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    PanicAlert("Couldn't get device list");
    return;
  }

  libusb_device* device;
  int i = 0;
  while ((device = list[i++]))
  {
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

    const libusb_interface* interface = &config_descriptor->interface[INTERFACE];
    const libusb_interface_descriptor* descriptor = &interface->altsetting[0];
    if (descriptor->bNumEndpoints == 3 && descriptor->bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
        descriptor->bInterfaceProtocol == PROTOCOL_BLUETOOTH && IsWantedDevice(device_descriptor) &&
        OpenDevice(device))
    {
      NOTICE_LOG(WII_IPC_WIIMOTE, "Using device %04x:%04x for Bluetooth",
                 device_descriptor.idVendor, device_descriptor.idProduct);
      libusb_free_config_descriptor(config_descriptor);
      break;
    }
    libusb_free_config_descriptor(config_descriptor);
  }
  libusb_free_device_list(list, 1);
  if (m_handle == nullptr)
    PanicAlert("Bluetooth passthrough mode enabled, but no usable Bluetooth USB device was found."
               "\nDolphin will likely crash now.");
  StartThread();

  s_link_keys.clear();
  LoadLinkKeys();
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_real::~CWII_IPC_HLE_Device_usb_oh1_57e_305_real()
{
  libusb_release_interface(m_handle, 0);
  StopThread();
  libusb_unref_device(m_device);
  libusb_exit(m_libusb_context);

  SaveLinkKeys();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_real::Open(u32 command_address, u32 mode)
{
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_real::Close(u32 command_address, bool force)
{
  if (!force)
    Memory::Write_U32(0, command_address + 4);
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
    const auto cmd = new CtrlMessage(cmd_buffer);
    const u16 opcode = *reinterpret_cast<u16*>(Memory::GetPointer(cmd->payload_addr));
    if (opcode == HCI_CMD_READ_BUFFER_SIZE)
    {
      m_fake_read_buffer_size_reply.Set();
      delete cmd;
      return GetNoReply();
    }
    if (opcode == HCI_CMD_DELETE_STORED_LINK_KEY)
    {
      // Delete link key(s) from our own link key storage when the game tells the adapter to
      const auto* delete_cmd =
          reinterpret_cast<hci_delete_stored_link_key_cp*>(Memory::GetPointer(cmd->payload_addr));
      if (delete_cmd->delete_all)
      {
        s_link_keys.clear();
      }
      else
      {
        btaddr_t addr;
        std::copy(std::begin(delete_cmd->bdaddr.b), std::end(delete_cmd->bdaddr.b), addr.begin());
        s_link_keys.erase(addr);
      }
    }
    if (s_link_keys_state == LinkKeysState::NeedsWrite)
    {
      SendHCIStoreLinkKeyCommand();
      s_link_keys_state = LinkKeysState::SentWriteCommand;
    }
    auto buffer = std::vector<u8>(cmd->length + LIBUSB_CONTROL_SETUP_SIZE);
    libusb_fill_control_setup(buffer.data(), cmd->request_type, cmd->request, cmd->value,
                              cmd->index, cmd->length);
    Memory::CopyFromEmu(buffer.data() + LIBUSB_CONTROL_SETUP_SIZE, cmd->payload_addr, cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_fill_control_transfer(transfer, m_handle, buffer.data(), CommandCallback, cmd, 0);
    libusb_submit_transfer(transfer);
    break;
  }
  // ACL data (incoming or outgoing) and incoming HCI events (respectively)
  case USBV0_IOCTL_BLKMSG:
  case USBV0_IOCTL_INTRMSG:
  {
    auto* buffer = new CtrlBuffer(cmd_buffer, command_address);
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG && m_fake_read_buffer_size_reply.TestAndClear())
    {
      FakeReadBufferSizeReply(*buffer);
      return GetNoReply();
    }
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG &&
        s_sync_button_state == SyncButtonState::Pressed)
    {
      Core::DisplayMessage("Scanning for Wiimotes", 2000);
      FakeSyncButtonPressedEvent(*buffer);
      return GetNoReply();
    }
    if (cmd_buffer.Parameter == USBV0_IOCTL_INTRMSG &&
        s_sync_button_state == SyncButtonState::LongPressed)
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
    transfer->user_data = buffer;
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
    PanicAlert("Attempted to load a state. Bluetooth will likely be broken now.");

  if (!passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be disabled. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::SendHCIResetCommand()
{
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  u8 packet[3] = {};
  *reinterpret_cast<u16*>(&packet[0]) = HCI_CMD_RESET;
  libusb_control_transfer(m_handle, type, 0, 0, 0, packet, sizeof(packet), TIMEOUT);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::SendHCIStoreLinkKeyCommand()
{
  if (s_link_keys.empty())
    return;

  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  // The HCI command field is limited to uint8_t, and libusb to uint16_t.
  const u8 payload_size =
      static_cast<u8>(sizeof(hci_write_stored_link_key_cp)) +
      (sizeof(btaddr_t) + sizeof(linkkey_t)) * static_cast<u8>(s_link_keys.size());
  std::vector<u8> packet(sizeof(hci_cmd_hdr_t) + payload_size);

  auto* header = reinterpret_cast<hci_cmd_hdr_t*>(packet.data());
  header->opcode = HCI_CMD_WRITE_STORED_LINK_KEY;
  header->length = payload_size;

  auto* cmd =
      reinterpret_cast<hci_write_stored_link_key_cp*>(packet.data() + sizeof(hci_cmd_hdr_t));
  cmd->num_keys_write = static_cast<u8>(s_link_keys.size());

  // This is really ugly, but necessary because of the HCI command structure:
  //   u8 num_keys;
  //   u8 bdaddr[6];
  //   u8 key[16];
  // where the two last items are repeated num_keys times.
  auto iterator = packet.begin() + sizeof(hci_cmd_hdr_t) + sizeof(hci_write_stored_link_key_cp);
  for (auto& entry : s_link_keys)
  {
    std::copy(entry.first.begin(), entry.first.end(), iterator);
    iterator += entry.first.size();
    std::copy(entry.second.begin(), entry.second.end(), iterator);
    iterator += entry.second.size();
  }

  libusb_control_transfer(m_handle, type, 0, 0, 0, packet.data(), static_cast<u16>(packet.size()),
                          TIMEOUT);
}

// Due to how the widcomm stack which Nintendo uses is coded, we must never
// let the stack think the controller is buffering more than 10 data packets
// - it will cause a u8 underflow and royally screw things up.
// Therefore, the reply to this command has to be faked to avoid random, weird issues
// (including Wiimote disconnects and "event mismatch" warning messages).
void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::FakeReadBufferSizeReply(const CtrlBuffer& ctrl)
{
  u8* packet = Memory::GetPointer(ctrl.m_payload_addr);
  auto* hci_event = reinterpret_cast<SHCIEventCommand*>(packet);
  hci_event->EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event->PayloadLength = sizeof(SHCIEventCommand) - 2 + sizeof(hci_read_buffer_size_rp);
  hci_event->PacketIndicator = 0x01;
  hci_event->Opcode = HCI_CMD_READ_BUFFER_SIZE;

  hci_read_buffer_size_rp reply;
  reply.status = 0x00;
  reply.max_acl_size = ACL_PKT_SIZE;
  reply.num_acl_pkts = ACL_PKT_NUM;
  reply.max_sco_size = SCO_PKT_SIZE;
  reply.num_sco_pkts = SCO_PKT_NUM;

  memcpy(packet + sizeof(SHCIEventCommand), &reply, sizeof(hci_read_buffer_size_rp));
  ctrl.SetRetVal(sizeof(SHCIEventCommand) + sizeof(hci_read_buffer_size_rp));
  EnqueueReply(ctrl.m_cmd_address);
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
  s_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::FakeSyncButtonHeldEvent(const CtrlBuffer& ctrl)
{
  NOTICE_LOG(WII_IPC_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  const u8 payload[1] = {0x09};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  s_sync_button_state = SyncButtonState::Ignored;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::LoadLinkKeys()
{
  const std::string& entries = SConfig::GetInstance().m_bt_passthrough_link_keys;
  if (entries.empty())
    return;
  std::vector<std::string> pairs;
  SplitString(entries, ',', pairs);
  for (const auto& pair : pairs)
  {
    const auto index = pair.find('=');
    if (index == std::string::npos)
      continue;

    btaddr_t address;
    StringToMacAddress(pair.substr(0, index), address.data());
    std::reverse(address.begin(), address.end());

    const std::string& key_string = pair.substr(index + 1);
    linkkey_t key;
    size_t pos = 0;
    for (size_t i = 0; i < key_string.length(); i = i + 2)
    {
      int value;
      std::stringstream(key_string.substr(i, 2)) >> std::hex >> value;
      key[pos++] = value;
    }

    s_link_keys[address] = key;
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::SaveLinkKeys()
{
  std::ostringstream oss;
  for (const auto& entry : s_link_keys)
  {
    btaddr_t address;
    // Reverse the address so that it is stored in the correct order in the config file
    std::reverse_copy(entry.first.begin(), entry.first.end(), address.begin());
    oss << MacAddressToString(address.data());
    oss << '=';
    oss << std::hex;
    for (const u16& data : entry.second)
      oss << std::setfill('0') << std::setw(2) << data;
    oss << std::dec << ',';
  }
  std::string config_string = oss.str();
  if (!config_string.empty())
    config_string.pop_back();
  SConfig::GetInstance().m_bt_passthrough_link_keys = config_string;
  SConfig::GetInstance().SaveSettings();
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305_real::OpenDevice(libusb_device* device)
{
  m_device = libusb_ref_device(device);
  const int ret = libusb_open(m_device, &m_handle);
  if (ret != 0)
  {
    PanicAlert("Failed to open Bluetooth device: %s", libusb_error_name(ret));
    return false;
  }

  const int result = libusb_detach_kernel_driver(m_handle, INTERFACE);
  if (result < 0 && result != LIBUSB_ERROR_NOT_FOUND && result != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    PanicAlert("Failed to detach kernel driver: %s", libusb_error_name(result));
    return false;
  }
  if (libusb_claim_interface(m_handle, INTERFACE) < 0)
  {
    PanicAlert("Failed to claim interface");
    return false;
  }

  return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::StartThread()
{
  if (m_thread_running.IsSet())
    return;
  m_thread_running.Set();
  m_thread = std::thread(&CWII_IPC_HLE_Device_usb_oh1_57e_305_real::ThreadFunc, this);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::StopThread()
{
  if (m_thread_running.TestAndClear())
  {
    libusb_close(m_handle);
    m_thread.join();
  }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::ThreadFunc()
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
  const auto* cmd = static_cast<CtrlMessage*>(tr->user_data);
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
  delete cmd;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_real::TransferCallback(libusb_transfer* tr)
{
  const auto* ctrl = static_cast<CtrlBuffer*>(tr->user_data);
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

  if (tr->status == LIBUSB_TRANSFER_COMPLETED && tr->endpoint == HCI_EVENT)
  {
    const auto* event = reinterpret_cast<hci_event_hdr_t*>(tr->buffer);

    if (event->event == HCI_EVENT_COMMAND_COMPL &&
        reinterpret_cast<SHCIEventCommand*>(tr->buffer)->Opcode == HCI_CMD_RESET)
      s_link_keys_state = LinkKeysState::NeedsWrite;

    if (s_link_keys_state == LinkKeysState::SentWriteCommand &&
        event->event == HCI_EVENT_COMMAND_COMPL &&
        reinterpret_cast<SHCIEventCommand*>(tr->buffer)->Opcode == HCI_CMD_WRITE_STORED_LINK_KEY)
    {
      // Ignore this command complete event to avoid possibly confusing games.
      s_link_keys_state = LinkKeysState::Written;
      ctrl->SetRetVal(0);
      EnqueueReply(ctrl->m_cmd_address);
      delete ctrl;
      return;
    }

    if (event->event == HCI_EVENT_LINK_KEY_NOTIFICATION)
    {
      const auto* notification =
          reinterpret_cast<hci_link_key_notification_ep*>(tr->buffer + sizeof(*event));

      btaddr_t addr;
      std::copy(std::begin(notification->bdaddr.b), std::end(notification->bdaddr.b), addr.begin());
      linkkey_t key;
      std::copy(std::begin(notification->key), std::end(notification->key), std::begin(key));
      s_link_keys[addr] = key;
    }
  }

  ctrl->SetRetVal(tr->actual_length);
  EnqueueReply(ctrl->m_cmd_address);
  delete ctrl;
}
