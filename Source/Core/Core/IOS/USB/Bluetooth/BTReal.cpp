// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/BTReal.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <libusb.h>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace IOS
{
namespace HLE
{
static bool IsWantedDevice(const libusb_device_descriptor& descriptor)
{
  const int vid = SConfig::GetInstance().m_bt_passthrough_vid;
  const int pid = SConfig::GetInstance().m_bt_passthrough_pid;
  if (vid == -1 || pid == -1)
    return true;
  return descriptor.idVendor == vid && descriptor.idProduct == pid;
}

static bool IsBluetoothDevice(const libusb_interface_descriptor& descriptor)
{
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif
  CONSTEXPR(u8, SUBCLASS, 0x01);
  CONSTEXPR(u8, PROTOCOL_BLUETOOTH, 0x01);
  if (SConfig::GetInstance().m_bt_passthrough_vid != -1 &&
      SConfig::GetInstance().m_bt_passthrough_pid != -1)
    return true;
  return descriptor.bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
         descriptor.bInterfaceSubClass == SUBCLASS &&
         descriptor.bInterfaceProtocol == PROTOCOL_BLUETOOTH;
}

namespace Device
{
BluetoothReal::BluetoothReal(Kernel& ios, const std::string& device_name)
    : BluetoothBase(ios, device_name)
{
  const int ret = libusb_init(&m_libusb_context);
  if (ret < 0)
  {
    PanicAlertT("Couldn't initialise libusb for Bluetooth passthrough: %s", libusb_error_name(ret));
  }
  LoadLinkKeys();
}

BluetoothReal::~BluetoothReal()
{
  if (m_handle != nullptr)
  {
    SendHCIResetCommand();
    WaitForHCICommandComplete(HCI_CMD_RESET);
    libusb_release_interface(m_handle, 0);
    // libusb_handle_events() may block the libusb thread indefinitely, so we need to
    // call libusb_close() first then immediately stop the thread in StopTransferThread.
    StopTransferThread();
    libusb_unref_device(m_device);
  }

  libusb_exit(m_libusb_context);
  SaveLinkKeys();
}

ReturnCode BluetoothReal::Open(const OpenRequest& request)
{
  if (!m_libusb_context)
    return IPC_EACCES;

  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    ERROR_LOG(IOS_WIIMOTE, "Couldn't get device list: %s",
              libusb_error_name(static_cast<int>(cnt)));
    return IPC_ENOENT;
  }

  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device* device = list[i];
    libusb_device_descriptor device_descriptor;
    libusb_config_descriptor* config_descriptor;
    libusb_get_device_descriptor(device, &device_descriptor);
    const int ret = libusb_get_config_descriptor(device, 0, &config_descriptor);
    if (ret != 0)
    {
      ERROR_LOG(IOS_WIIMOTE, "Failed to get config descriptor for device %04x:%04x: %s",
                device_descriptor.idVendor, device_descriptor.idProduct, libusb_error_name(ret));
      continue;
    }

    const libusb_interface& interface = config_descriptor->interface[INTERFACE];
    const libusb_interface_descriptor& descriptor = interface.altsetting[0];
    if (IsBluetoothDevice(descriptor) && IsWantedDevice(device_descriptor) && OpenDevice(device))
    {
      unsigned char manufacturer[50] = {}, product[50] = {}, serial_number[50] = {};
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iManufacturer, manufacturer,
                                         sizeof(manufacturer));
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iProduct, product,
                                         sizeof(product));
      libusb_get_string_descriptor_ascii(m_handle, device_descriptor.iSerialNumber, serial_number,
                                         sizeof(serial_number));
      NOTICE_LOG(IOS_WIIMOTE, "Using device %04x:%04x (rev %x) for Bluetooth: %s %s %s",
                 device_descriptor.idVendor, device_descriptor.idProduct,
                 device_descriptor.bcdDevice, manufacturer, product, serial_number);
      m_is_wii_bt_module =
          device_descriptor.idVendor == 0x57e && device_descriptor.idProduct == 0x305;
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
    return IPC_ENOENT;
  }

  StartTransferThread();

  m_is_active = true;
  return IPC_SUCCESS;
}

ReturnCode BluetoothReal::Close(u32 fd)
{
  if (m_handle)
  {
    libusb_release_interface(m_handle, 0);
    StopTransferThread();
    libusb_unref_device(m_device);
    m_handle = nullptr;
  }

  return Device::Close(fd);
}

IPCCommandResult BluetoothReal::IOCtlV(const IOCtlVRequest& request)
{
  if (!m_is_wii_bt_module && m_need_reset_keys.TestAndClear())
  {
    // Do this now before transferring any more data, so that this is fully transparent to games
    SendHCIDeleteLinkKeyCommand();
    WaitForHCICommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY);
    if (SendHCIStoreLinkKeyCommand())
      WaitForHCICommandComplete(HCI_CMD_WRITE_STORED_LINK_KEY);
  }

  switch (request.request)
  {
  // HCI commands to the Bluetooth adapter
  case USB::IOCTLV_USBV0_CTRLMSG:
  {
    std::lock_guard<std::mutex> lk(m_transfers_mutex);
    auto cmd = std::make_unique<USB::V0CtrlMessage>(m_ios, request);
    const u16 opcode = Common::swap16(Memory::Read_U16(cmd->data_address));
    if (opcode == HCI_CMD_READ_BUFFER_SIZE)
    {
      m_fake_read_buffer_size_reply.Set();
      return GetNoReply();
    }
    if (!m_is_wii_bt_module && (opcode == 0xFC4C || opcode == 0xFC4F))
    {
      m_fake_vendor_command_reply.Set();
      m_fake_vendor_command_reply_opcode = opcode;
      return GetNoReply();
    }
    if (opcode == HCI_CMD_DELETE_STORED_LINK_KEY)
    {
      // Delete link key(s) from our own link key storage when the game tells the adapter to
      hci_delete_stored_link_key_cp delete_cmd;
      Memory::CopyFromEmu(&delete_cmd, cmd->data_address, sizeof(delete_cmd));
      if (delete_cmd.delete_all)
        m_link_keys.clear();
      else
        m_link_keys.erase(delete_cmd.bdaddr);
    }
    auto buffer = std::make_unique<u8[]>(cmd->length + LIBUSB_CONTROL_SETUP_SIZE);
    libusb_fill_control_setup(buffer.get(), cmd->request_type, cmd->request, cmd->value, cmd->index,
                              cmd->length);
    Memory::CopyFromEmu(buffer.get() + LIBUSB_CONTROL_SETUP_SIZE, cmd->data_address, cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_fill_control_transfer(transfer, m_handle, buffer.get(), nullptr, this, 0);
    transfer->callback = [](libusb_transfer* tr) {
      static_cast<BluetoothReal*>(tr->user_data)->HandleCtrlTransfer(tr);
    };
    PendingTransfer pending_transfer{std::move(cmd), std::move(buffer)};
    m_current_transfers.emplace(transfer, std::move(pending_transfer));
    libusb_submit_transfer(transfer);
    break;
  }
  // ACL data (incoming or outgoing) and incoming HCI events (respectively)
  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_INTRMSG:
  {
    std::lock_guard<std::mutex> lk(m_transfers_mutex);
    auto cmd = std::make_unique<USB::V0IntrMessage>(m_ios, request);
    if (request.request == USB::IOCTLV_USBV0_INTRMSG)
    {
      if (m_sync_button_state == SyncButtonState::Pressed)
      {
        Core::DisplayMessage("Scanning for Wii Remotes", 2000);
        FakeSyncButtonPressedEvent(*cmd);
        return GetNoReply();
      }
      if (m_sync_button_state == SyncButtonState::LongPressed)
      {
        Core::DisplayMessage("Reset saved Wii Remote pairings", 2000);
        FakeSyncButtonHeldEvent(*cmd);
        return GetNoReply();
      }
      if (m_fake_read_buffer_size_reply.TestAndClear())
      {
        FakeReadBufferSizeReply(*cmd);
        return GetNoReply();
      }
      if (m_fake_vendor_command_reply.TestAndClear())
      {
        FakeVendorCommandReply(*cmd);
        return GetNoReply();
      }
    }
    auto buffer = cmd->MakeBuffer(cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->buffer = buffer.get();
    transfer->callback = [](libusb_transfer* tr) {
      static_cast<BluetoothReal*>(tr->user_data)->HandleBulkOrIntrTransfer(tr);
    };
    transfer->dev_handle = m_handle;
    transfer->endpoint = cmd->endpoint;
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    transfer->length = cmd->length;
    transfer->timeout = TIMEOUT;
    transfer->type = request.request == USB::IOCTLV_USBV0_BLKMSG ? LIBUSB_TRANSFER_TYPE_BULK :
                                                                   LIBUSB_TRANSFER_TYPE_INTERRUPT;
    transfer->user_data = this;
    PendingTransfer pending_transfer{std::move(cmd), std::move(buffer)};
    m_current_transfers.emplace(transfer, std::move(pending_transfer));
    libusb_submit_transfer(transfer);
    break;
  }
  }
  // Replies are generated inside of the message handlers (and asynchronously).
  return GetNoReply();
}

static bool s_has_shown_savestate_warning = false;
void BluetoothReal::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = true;
  p.Do(passthrough_bluetooth);
  if (!passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be disabled. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
    return;
  }

  // Prevent the transfer callbacks from messing with m_current_transfers after we have started
  // writing a savestate. We cannot use a scoped lock here because DoState is called twice and
  // we would lose the lock between the two calls.
  if (p.GetMode() == PointerWrap::MODE_MEASURE || p.GetMode() == PointerWrap::MODE_VERIFY)
    m_transfers_mutex.lock();

  std::vector<u32> addresses_to_discard;
  if (p.GetMode() != PointerWrap::MODE_READ)
  {
    // Save addresses of transfer commands to discard on savestate load.
    for (const auto& transfer : m_current_transfers)
      addresses_to_discard.push_back(transfer.second.command->ios_request.address);
  }
  p.Do(addresses_to_discard);
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // On load, discard any pending transfer to make sure the emulated software is not stuck
    // waiting for the previous request to complete. This is usually not an issue as long as
    // the Bluetooth state is the same (same Wii Remote connections).
    for (const auto& address_to_discard : addresses_to_discard)
      m_ios.EnqueueIPCReply(Request{address_to_discard}, 0);

    // Prevent the callbacks from replying to a request that has already been discarded.
    m_current_transfers.clear();

    OSD::AddMessage("If the savestate does not load correctly, disconnect all Wii Remotes "
                    "and reload it.",
                    OSD::Duration::NORMAL);
  }

  if (!s_has_shown_savestate_warning && p.GetMode() == PointerWrap::MODE_WRITE)
  {
    OSD::AddMessage("Savestates may not work with Bluetooth passthrough in all cases.\n"
                    "They will only work if no remote is connected when restoring the state,\n"
                    "or no remote is disconnected after saving.",
                    OSD::Duration::VERY_LONG);
    s_has_shown_savestate_warning = true;
  }

  // We have finished the savestate now, so the transfers mutex can be unlocked.
  if (p.GetMode() == PointerWrap::MODE_WRITE)
    m_transfers_mutex.unlock();
}

void BluetoothReal::UpdateSyncButtonState(const bool is_held)
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

void BluetoothReal::TriggerSyncButtonPressedEvent()
{
  m_sync_button_state = SyncButtonState::Pressed;
}

void BluetoothReal::TriggerSyncButtonHeldEvent()
{
  m_sync_button_state = SyncButtonState::LongPressed;
}

void BluetoothReal::WaitForHCICommandComplete(const u16 opcode)
{
  int actual_length;
  std::vector<u8> buffer(1024);
  // Only try 100 transfers at most, to avoid being stuck in an infinite loop
  for (int tries = 0; tries < 100; ++tries)
  {
    if (libusb_interrupt_transfer(m_handle, HCI_EVENT, buffer.data(),
                                  static_cast<int>(buffer.size()), &actual_length, 20) == 0 &&
        reinterpret_cast<hci_event_hdr_t*>(buffer.data())->event == HCI_EVENT_COMMAND_COMPL &&
        reinterpret_cast<SHCIEventCommand*>(buffer.data())->Opcode == opcode)
      break;
  }
}

void BluetoothReal::SendHCIResetCommand()
{
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  u8 packet[3] = {};
  const u16 payload[] = {HCI_CMD_RESET};
  memcpy(packet, payload, sizeof(payload));
  libusb_control_transfer(m_handle, type, 0, 0, 0, packet, sizeof(packet), TIMEOUT);
  INFO_LOG(IOS_WIIMOTE, "Sent a reset command to adapter");
}

void BluetoothReal::SendHCIDeleteLinkKeyCommand()
{
  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  std::vector<u8> packet(sizeof(hci_cmd_hdr_t) + sizeof(hci_delete_stored_link_key_cp));

  auto* header = reinterpret_cast<hci_cmd_hdr_t*>(packet.data());
  header->opcode = HCI_CMD_DELETE_STORED_LINK_KEY;
  header->length = sizeof(hci_delete_stored_link_key_cp);
  auto* cmd =
      reinterpret_cast<hci_delete_stored_link_key_cp*>(packet.data() + sizeof(hci_cmd_hdr_t));
  cmd->bdaddr = {};
  cmd->delete_all = true;

  libusb_control_transfer(m_handle, type, 0, 0, 0, packet.data(), static_cast<u16>(packet.size()),
                          TIMEOUT);
}

bool BluetoothReal::SendHCIStoreLinkKeyCommand()
{
  if (m_link_keys.empty())
    return false;

  const u8 type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
  // The HCI command field is limited to uint8_t, and libusb to uint16_t.
  const u8 payload_size =
      static_cast<u8>(sizeof(hci_write_stored_link_key_cp)) +
      (sizeof(bdaddr_t) + sizeof(linkkey_t)) * static_cast<u8>(m_link_keys.size());
  std::vector<u8> packet(sizeof(hci_cmd_hdr_t) + payload_size);

  auto* header = reinterpret_cast<hci_cmd_hdr_t*>(packet.data());
  header->opcode = HCI_CMD_WRITE_STORED_LINK_KEY;
  header->length = payload_size;

  auto* cmd =
      reinterpret_cast<hci_write_stored_link_key_cp*>(packet.data() + sizeof(hci_cmd_hdr_t));
  cmd->num_keys_write = static_cast<u8>(m_link_keys.size());

  // This is really ugly, but necessary because of the HCI command structure:
  //   u8 num_keys;
  //   u8 bdaddr[6];
  //   u8 key[16];
  // where the two last items are repeated num_keys times.
  auto iterator = packet.begin() + sizeof(hci_cmd_hdr_t) + sizeof(hci_write_stored_link_key_cp);
  for (const auto& entry : m_link_keys)
  {
    std::copy(entry.first.begin(), entry.first.end(), iterator);
    iterator += entry.first.size();
    std::copy(entry.second.begin(), entry.second.end(), iterator);
    iterator += entry.second.size();
  }

  libusb_control_transfer(m_handle, type, 0, 0, 0, packet.data(), static_cast<u16>(packet.size()),
                          TIMEOUT);
  return true;
}

void BluetoothReal::FakeVendorCommandReply(USB::V0IntrMessage& ctrl)
{
  SHCIEventCommand hci_event;
  Memory::CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event.PayloadLength = sizeof(SHCIEventCommand) - 2;
  hci_event.PacketIndicator = 0x01;
  hci_event.Opcode = m_fake_vendor_command_reply_opcode;
  Memory::CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));
  m_ios.EnqueueIPCReply(ctrl.ios_request, static_cast<s32>(sizeof(hci_event)));
}

// Due to how the widcomm stack which Nintendo uses is coded, we must never
// let the stack think the controller is buffering more than 10 data packets
// - it will cause a u8 underflow and royally screw things up.
// Therefore, the reply to this command has to be faked to avoid random, weird issues
// (including Wiimote disconnects and "event mismatch" warning messages).
void BluetoothReal::FakeReadBufferSizeReply(USB::V0IntrMessage& ctrl)
{
  SHCIEventCommand hci_event;
  Memory::CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event.PayloadLength = sizeof(SHCIEventCommand) - 2 + sizeof(hci_read_buffer_size_rp);
  hci_event.PacketIndicator = 0x01;
  hci_event.Opcode = HCI_CMD_READ_BUFFER_SIZE;
  Memory::CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));

  hci_read_buffer_size_rp reply;
  reply.status = 0x00;
  reply.max_acl_size = ACL_PKT_SIZE;
  reply.num_acl_pkts = ACL_PKT_NUM;
  reply.max_sco_size = SCO_PKT_SIZE;
  reply.num_sco_pkts = SCO_PKT_NUM;
  Memory::CopyToEmu(ctrl.data_address + sizeof(hci_event), &reply, sizeof(reply));
  m_ios.EnqueueIPCReply(ctrl.ios_request, static_cast<s32>(sizeof(hci_event) + sizeof(reply)));
}

void BluetoothReal::FakeSyncButtonEvent(USB::V0IntrMessage& ctrl, const u8* payload, const u8 size)
{
  hci_event_hdr_t hci_event;
  Memory::CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.event = HCI_EVENT_VENDOR;
  hci_event.length = size;
  Memory::CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));
  Memory::CopyToEmu(ctrl.data_address + sizeof(hci_event), payload, size);
  m_ios.EnqueueIPCReply(ctrl.ios_request, static_cast<s32>(sizeof(hci_event) + size));
}

// When the red sync button is pressed, a HCI event is generated:
//   > HCI Event: Vendor (0xff) plen 1
//   08
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void BluetoothReal::FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG(IOS_WIIMOTE, "Faking 'sync button pressed' (0x08) event packet");
  const u8 payload[1] = {0x08};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void BluetoothReal::FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG(IOS_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  const u8 payload[1] = {0x09};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

void BluetoothReal::LoadLinkKeys()
{
  const std::string& entries = SConfig::GetInstance().m_bt_passthrough_link_keys;
  if (entries.empty())
    return;
  for (const auto& pair : SplitString(entries, ','))
  {
    const auto index = pair.find('=');
    if (index == std::string::npos)
      continue;

    bdaddr_t address;
    Common::StringToMacAddress(pair.substr(0, index), address.data());
    std::reverse(address.begin(), address.end());

    const std::string& key_string = pair.substr(index + 1);
    linkkey_t key{};
    size_t pos = 0;
    for (size_t i = 0; i < key_string.length(); i = i + 2)
    {
      int value;
      std::stringstream(key_string.substr(i, 2)) >> std::hex >> value;
      key[pos++] = value;
    }

    m_link_keys[address] = key;
  }
}

void BluetoothReal::SaveLinkKeys()
{
  std::ostringstream oss;
  for (const auto& entry : m_link_keys)
  {
    bdaddr_t address;
    // Reverse the address so that it is stored in the correct order in the config file
    std::reverse_copy(entry.first.begin(), entry.first.end(), address.begin());
    oss << Common::MacAddressToString(address.data());
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
}

bool BluetoothReal::OpenDevice(libusb_device* device)
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

void BluetoothReal::StartTransferThread()
{
  if (m_thread_running.IsSet())
    return;
  m_thread_running.Set();
  m_thread = std::thread(&BluetoothReal::TransferThread, this);
}

void BluetoothReal::StopTransferThread()
{
  if (m_thread_running.TestAndClear())
  {
    libusb_close(m_handle);
    m_thread.join();
  }
}

void BluetoothReal::TransferThread()
{
  Common::SetCurrentThreadName("BT USB Thread");
  while (m_thread_running.IsSet())
  {
    libusb_handle_events_completed(m_libusb_context, nullptr);
  }
}

// The callbacks are called from libusb code on a separate thread.
void BluetoothReal::HandleCtrlTransfer(libusb_transfer* tr)
{
  std::lock_guard<std::mutex> lk(m_transfers_mutex);
  if (!m_current_transfers.count(tr))
    return;

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG(IOS_WIIMOTE, "libusb command transfer failed, status: 0x%02x", tr->status);
    if (!m_showed_failed_transfer.IsSet())
    {
      Core::DisplayMessage("Failed to send a command to the Bluetooth adapter.", 10000);
      Core::DisplayMessage("It may not be compatible with passthrough mode.", 10000);
      m_showed_failed_transfer.Set();
    }
  }
  else
  {
    m_showed_failed_transfer.Clear();
  }
  const auto& command = m_current_transfers.at(tr).command;
  command->FillBuffer(libusb_control_transfer_get_data(tr), tr->actual_length);
  m_ios.EnqueueIPCReply(command->ios_request, tr->actual_length, 0,
                        CoreTiming::FromThread::NON_CPU);
  m_current_transfers.erase(tr);
}

void BluetoothReal::HandleBulkOrIntrTransfer(libusb_transfer* tr)
{
  std::lock_guard<std::mutex> lk(m_transfers_mutex);
  if (!m_current_transfers.count(tr))
    return;

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_TIMED_OUT &&
      tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG(IOS_WIIMOTE, "libusb transfer failed, status: 0x%02x", tr->status);
    if (!m_showed_failed_transfer.IsSet())
    {
      Core::DisplayMessage("Failed to transfer to or from to the Bluetooth adapter.", 10000);
      Core::DisplayMessage("It may not be compatible with passthrough mode.", 10000);
      m_showed_failed_transfer.Set();
    }
  }
  else
  {
    m_showed_failed_transfer.Clear();
  }

  if (tr->status == LIBUSB_TRANSFER_COMPLETED && tr->endpoint == HCI_EVENT)
  {
    const auto* event = reinterpret_cast<hci_event_hdr_t*>(tr->buffer);
    if (event->event == HCI_EVENT_LINK_KEY_NOTIFICATION)
    {
      const auto* notification =
          reinterpret_cast<hci_link_key_notification_ep*>(tr->buffer + sizeof(hci_event_hdr_t));

      linkkey_t key;
      std::copy(std::begin(notification->key), std::end(notification->key), std::begin(key));
      m_link_keys[notification->bdaddr] = key;
    }
    else if (event->event == HCI_EVENT_COMMAND_COMPL &&
             reinterpret_cast<hci_command_compl_ep*>(tr->buffer + sizeof(*event))->opcode ==
                 HCI_CMD_RESET)
    {
      m_need_reset_keys.Set();
    }
  }

  const auto& command = m_current_transfers.at(tr).command;
  command->FillBuffer(tr->buffer, tr->actual_length);
  m_ios.EnqueueIPCReply(command->ios_request, tr->actual_length, 0,
                        CoreTiming::FromThread::NON_CPU);
  m_current_transfers.erase(tr);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
