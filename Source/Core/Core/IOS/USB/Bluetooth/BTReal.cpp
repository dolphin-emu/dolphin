// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <fmt/format.h>
#include <libusb.h>

#include "Common/ChunkFile.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/System.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace IOS::HLE
{
constexpr u8 REQUEST_TYPE = static_cast<u8>(LIBUSB_ENDPOINT_OUT) |
                            static_cast<u8>(LIBUSB_REQUEST_TYPE_CLASS) |
                            static_cast<u8>(LIBUSB_RECIPIENT_INTERFACE);

static bool IsWantedDevice(const libusb_device_descriptor& descriptor)
{
  const int vid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID);
  const int pid = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID);
  if (vid == -1 || pid == -1)
    return true;
  return descriptor.idVendor == vid && descriptor.idProduct == pid;
}

static bool IsBluetoothDevice(const libusb_interface_descriptor& descriptor)
{
  constexpr u8 SUBCLASS = 0x01;
  constexpr u8 PROTOCOL_BLUETOOTH = 0x01;
  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_VID) != -1 &&
      Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_PID) != -1)
  {
    return true;
  }
  return descriptor.bInterfaceClass == LIBUSB_CLASS_WIRELESS &&
         descriptor.bInterfaceSubClass == SUBCLASS &&
         descriptor.bInterfaceProtocol == PROTOCOL_BLUETOOTH;
}

BluetoothRealDevice::BluetoothRealDevice(EmulationKernel& ios, const std::string& device_name)
    : BluetoothBaseDevice(ios, device_name)
{
  LoadLinkKeys();
}

BluetoothRealDevice::~BluetoothRealDevice()
{
  if (m_handle != nullptr)
  {
    SendHCIResetCommand();
    WaitForHCICommandComplete(HCI_CMD_RESET);
    const int ret = libusb_release_interface(m_handle, 0);
    if (ret != LIBUSB_SUCCESS)
      WARN_LOG_FMT(IOS_WIIMOTE, "libusb_release_interface failed: {}", LibusbUtils::ErrorWrap(ret));
    libusb_close(m_handle);
    libusb_unref_device(m_device);
  }
  SaveLinkKeys();
}

std::optional<IPCReply> BluetoothRealDevice::Open(const OpenRequest& request)
{
  if (!m_context.IsValid())
    return IPCReply(IPC_EACCES);

  m_last_open_error.clear();
  const int ret = m_context.GetDeviceList([this](libusb_device* device) {
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

    const libusb_interface& interface = config_descriptor->interface[INTERFACE];
    const libusb_interface_descriptor& descriptor = interface.altsetting[0];
    if (IsBluetoothDevice(descriptor) && IsWantedDevice(device_descriptor) && OpenDevice(device))
    {
      unsigned char manufacturer[50] = {}, product[50] = {}, serial_number[50] = {};
      const int manufacturer_ret = libusb_get_string_descriptor_ascii(
          m_handle, device_descriptor.iManufacturer, manufacturer, sizeof(manufacturer));
      if (manufacturer_ret < LIBUSB_SUCCESS)
      {
        WARN_LOG_FMT(IOS_WIIMOTE,
                     "Failed to get string for manufacturer descriptor {:02x} for device "
                     "{:04x}:{:04x} (rev {:x}): {}",
                     device_descriptor.iManufacturer, device_descriptor.idVendor,
                     device_descriptor.idProduct, device_descriptor.bcdDevice,
                     LibusbUtils::ErrorWrap(manufacturer_ret));
        manufacturer[0] = '?';
        manufacturer[1] = '\0';
      }
      const int product_ret = libusb_get_string_descriptor_ascii(
          m_handle, device_descriptor.iProduct, product, sizeof(product));
      if (product_ret < LIBUSB_SUCCESS)
      {
        WARN_LOG_FMT(IOS_WIIMOTE,
                     "Failed to get string for product descriptor {:02x} for device "
                     "{:04x}:{:04x} (rev {:x}): {}",
                     device_descriptor.iProduct, device_descriptor.idVendor,
                     device_descriptor.idProduct, device_descriptor.bcdDevice,
                     LibusbUtils::ErrorWrap(product_ret));
        product[0] = '?';
        product[1] = '\0';
      }
      const int serial_ret = libusb_get_string_descriptor_ascii(
          m_handle, device_descriptor.iSerialNumber, serial_number, sizeof(serial_number));
      if (serial_ret < LIBUSB_SUCCESS)
      {
        WARN_LOG_FMT(IOS_WIIMOTE,
                     "Failed to get string for serial number descriptor {:02x} for device "
                     "{:04x}:{:04x} (rev {:x}): {}",
                     device_descriptor.iSerialNumber, device_descriptor.idVendor,
                     device_descriptor.idProduct, device_descriptor.bcdDevice,
                     LibusbUtils::ErrorWrap(serial_ret));
        serial_number[0] = '?';
        serial_number[1] = '\0';
      }
      NOTICE_LOG_FMT(IOS_WIIMOTE, "Using device {:04x}:{:04x} (rev {:x}) for Bluetooth: {} {} {}",
                     device_descriptor.idVendor, device_descriptor.idProduct,
                     device_descriptor.bcdDevice, reinterpret_cast<char*>(manufacturer),
                     reinterpret_cast<char*>(product), reinterpret_cast<char*>(serial_number));
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
    return IPCReply(IPC_ENOENT);
  }

  return Device::Open(request);
}

std::optional<IPCReply> BluetoothRealDevice::Close(u32 fd)
{
  if (m_handle)
  {
    const int ret = libusb_release_interface(m_handle, 0);
    if (ret != LIBUSB_SUCCESS)
      WARN_LOG_FMT(IOS_WIIMOTE, "libusb_release_interface failed: {}", LibusbUtils::ErrorWrap(ret));
    libusb_close(m_handle);
    libusb_unref_device(m_device);
    m_handle = nullptr;
  }

  return Device::Close(fd);
}

std::optional<IPCReply> BluetoothRealDevice::IOCtlV(const IOCtlVRequest& request)
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
    auto& system = GetSystem();
    auto& memory = system.GetMemory();

    std::lock_guard lk(m_transfers_mutex);
    auto cmd = std::make_unique<USB::V0CtrlMessage>(GetEmulationKernel(), request);
    const u16 opcode = Common::swap16(memory.Read_U16(cmd->data_address));
    if (opcode == HCI_CMD_READ_BUFFER_SIZE)
    {
      m_fake_read_buffer_size_reply.Set();
      return std::nullopt;
    }
    if (!m_is_wii_bt_module && (opcode == 0xFC4C || opcode == 0xFC4F))
    {
      m_fake_vendor_command_reply.Set();
      m_fake_vendor_command_reply_opcode = opcode;
      return std::nullopt;
    }
    if (opcode == HCI_CMD_DELETE_STORED_LINK_KEY)
    {
      // Delete link key(s) from our own link key storage when the game tells the adapter to
      hci_delete_stored_link_key_cp delete_cmd;
      memory.CopyFromEmu(&delete_cmd, cmd->data_address, sizeof(delete_cmd));
      if (delete_cmd.delete_all)
        m_link_keys.clear();
      else
        m_link_keys.erase(delete_cmd.bdaddr);
    }
    auto buffer = std::make_unique<u8[]>(cmd->length + LIBUSB_CONTROL_SETUP_SIZE);
    libusb_fill_control_setup(buffer.get(), cmd->request_type, cmd->request, cmd->value, cmd->index,
                              cmd->length);
    memory.CopyFromEmu(buffer.get() + LIBUSB_CONTROL_SETUP_SIZE, cmd->data_address, cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    libusb_fill_control_transfer(transfer, m_handle, buffer.get(), nullptr, this, 0);
    transfer->callback = [](libusb_transfer* tr) {
      static_cast<BluetoothRealDevice*>(tr->user_data)->HandleCtrlTransfer(tr);
    };
    PendingTransfer pending_transfer{std::move(cmd), std::move(buffer)};
    m_current_transfers.emplace(transfer, std::move(pending_transfer));
    const int ret = libusb_submit_transfer(transfer);
    if (ret != LIBUSB_SUCCESS)
      WARN_LOG_FMT(IOS_WIIMOTE, "libusb_submit_transfer failed: {}", LibusbUtils::ErrorWrap(ret));
    break;
  }
  // ACL data (incoming or outgoing) and incoming HCI events (respectively)
  case USB::IOCTLV_USBV0_BLKMSG:
  case USB::IOCTLV_USBV0_INTRMSG:
  {
    std::lock_guard lk(m_transfers_mutex);
    auto cmd = std::make_unique<USB::V0IntrMessage>(GetEmulationKernel(), request);
    if (request.request == USB::IOCTLV_USBV0_INTRMSG)
    {
      if (m_sync_button_state == SyncButtonState::Pressed)
      {
        Core::DisplayMessage("Scanning for Wii Remotes", 2000);
        FakeSyncButtonPressedEvent(*cmd);
        return std::nullopt;
      }
      if (m_sync_button_state == SyncButtonState::LongPressed)
      {
        Core::DisplayMessage("Reset saved Wii Remote pairings", 2000);
        FakeSyncButtonHeldEvent(*cmd);
        return std::nullopt;
      }
      if (m_fake_read_buffer_size_reply.TestAndClear())
      {
        FakeReadBufferSizeReply(*cmd);
        return std::nullopt;
      }
      if (m_fake_vendor_command_reply.TestAndClear())
      {
        FakeVendorCommandReply(*cmd);
        return std::nullopt;
      }
    }
    auto buffer = cmd->MakeBuffer(cmd->length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    transfer->buffer = buffer.get();
    transfer->callback = [](libusb_transfer* tr) {
      static_cast<BluetoothRealDevice*>(tr->user_data)->HandleBulkOrIntrTransfer(tr);
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
    const int ret = libusb_submit_transfer(transfer);
    if (ret != LIBUSB_SUCCESS)
      WARN_LOG_FMT(IOS_WIIMOTE, "libusb_submit_transfer failed: {}", LibusbUtils::ErrorWrap(ret));
    break;
  }
  }
  // Replies are generated inside of the message handlers (and asynchronously).
  return std::nullopt;
}

static bool s_has_shown_savestate_warning = false;
void BluetoothRealDevice::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = true;
  p.Do(passthrough_bluetooth);
  if (!passthrough_bluetooth && p.IsReadMode())
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be disabled. Aborting load.", 4000);
    p.SetVerifyMode();
    return;
  }

  Device::DoState(p);
  // Prevent the transfer callbacks from messing with m_current_transfers after we have started
  // writing a savestate. We cannot use a scoped lock here because DoState is called twice and
  // we would lose the lock between the two calls.
  if (p.IsMeasureMode() || p.IsVerifyMode())
    m_transfers_mutex.lock();

  std::vector<u32> addresses_to_discard;
  if (!p.IsReadMode())
  {
    // Save addresses of transfer commands to discard on savestate load.
    for (const auto& transfer : m_current_transfers)
      addresses_to_discard.push_back(transfer.second.command->ios_request.address);
  }
  p.Do(addresses_to_discard);
  if (p.IsReadMode())
  {
    // On load, discard any pending transfer to make sure the emulated software is not stuck
    // waiting for the previous request to complete. This is usually not an issue as long as
    // the Bluetooth state is the same (same Wii Remote connections).
    auto& system = GetSystem();
    for (const auto& address_to_discard : addresses_to_discard)
      GetEmulationKernel().EnqueueIPCReply(Request{system, address_to_discard}, 0);

    // Prevent the callbacks from replying to a request that has already been discarded.
    m_current_transfers.clear();

    OSD::AddMessage("If the savestate does not load correctly, disconnect all Wii Remotes "
                    "and reload it.",
                    OSD::Duration::NORMAL);
  }

  if (!s_has_shown_savestate_warning && p.IsWriteMode())
  {
    OSD::AddMessage("Savestates may not work with Bluetooth passthrough in all cases.\n"
                    "They will only work if no remote is connected when restoring the state,\n"
                    "or no remote is disconnected after saving.",
                    OSD::Duration::VERY_LONG);
    s_has_shown_savestate_warning = true;
  }

  // We have finished the savestate now, so the transfers mutex can be unlocked.
  if (p.IsWriteMode())
    m_transfers_mutex.unlock();
}

void BluetoothRealDevice::UpdateSyncButtonState(const bool is_held)
{
  if (m_sync_button_state == SyncButtonState::Unpressed && is_held)
  {
    m_sync_button_held_timer.Start();
    m_sync_button_state = SyncButtonState::Held;
  }

  if (m_sync_button_state == SyncButtonState::Held && is_held &&
      m_sync_button_held_timer.ElapsedMs() > SYNC_BUTTON_HOLD_MS_TO_RESET)
    m_sync_button_state = SyncButtonState::LongPressed;
  else if (m_sync_button_state == SyncButtonState::Held && !is_held)
    m_sync_button_state = SyncButtonState::Pressed;

  if (m_sync_button_state == SyncButtonState::Ignored && !is_held)
    m_sync_button_state = SyncButtonState::Unpressed;
}

void BluetoothRealDevice::TriggerSyncButtonPressedEvent()
{
  m_sync_button_state = SyncButtonState::Pressed;
}

void BluetoothRealDevice::TriggerSyncButtonHeldEvent()
{
  m_sync_button_state = SyncButtonState::LongPressed;
}

void BluetoothRealDevice::WaitForHCICommandComplete(const u16 opcode)
{
  int actual_length;
  SHCIEventCommand packet;
  std::vector<u8> buffer(1024);
  // Only try 100 transfers at most, to avoid being stuck in an infinite loop
  for (int tries = 0; tries < 100; ++tries)
  {
    const int ret = libusb_interrupt_transfer(m_handle, HCI_EVENT, buffer.data(),
                                              static_cast<int>(buffer.size()), &actual_length, 20);
    if (ret != LIBUSB_SUCCESS || actual_length < static_cast<int>(sizeof(packet)))
      continue;
    std::memcpy(&packet, buffer.data(), sizeof(packet));
    if (packet.EventType == HCI_EVENT_COMMAND_COMPL && packet.Opcode == opcode)
      break;
  }
}

void BluetoothRealDevice::SendHCIResetCommand()
{
  u8 packet[3] = {};
  const u16 payload[] = {HCI_CMD_RESET};
  memcpy(packet, payload, sizeof(payload));
  const int ret =
      libusb_control_transfer(m_handle, REQUEST_TYPE, 0, 0, 0, packet, sizeof(packet), TIMEOUT);
  if (ret < LIBUSB_SUCCESS)
    WARN_LOG_FMT(IOS_WIIMOTE, "libusb_control_transfer failed: {}", LibusbUtils::ErrorWrap(ret));
  else
    INFO_LOG_FMT(IOS_WIIMOTE, "Sent a reset command to adapter");
}

void BluetoothRealDevice::SendHCIDeleteLinkKeyCommand()
{
  struct Payload
  {
    hci_cmd_hdr_t header;
    hci_delete_stored_link_key_cp command;
  };
  Payload payload;
  payload.header.opcode = HCI_CMD_DELETE_STORED_LINK_KEY;
  payload.header.length = sizeof(payload.command);
  payload.command.bdaddr = {};
  payload.command.delete_all = true;

  const int ret =
      libusb_control_transfer(m_handle, REQUEST_TYPE, 0, 0, 0, reinterpret_cast<u8*>(&payload),
                              static_cast<u16>(sizeof(payload)), TIMEOUT);
  if (ret < LIBUSB_SUCCESS)
    WARN_LOG_FMT(IOS_WIIMOTE, "libusb_control_transfer failed: {}", LibusbUtils::ErrorWrap(ret));
}

bool BluetoothRealDevice::SendHCIStoreLinkKeyCommand()
{
  if (m_link_keys.empty())
    return false;

  // The HCI command field is limited to uint8_t, and libusb to uint16_t.
  const u8 payload_size =
      static_cast<u8>(sizeof(hci_write_stored_link_key_cp)) +
      (sizeof(bdaddr_t) + sizeof(linkkey_t)) * static_cast<u8>(m_link_keys.size());
  std::vector<u8> packet(sizeof(hci_cmd_hdr_t) + payload_size);

  hci_cmd_hdr_t header{};
  header.opcode = HCI_CMD_WRITE_STORED_LINK_KEY;
  header.length = payload_size;
  std::memcpy(packet.data(), &header, sizeof(header));

  hci_write_stored_link_key_cp command{};
  command.num_keys_write = static_cast<u8>(m_link_keys.size());
  std::memcpy(packet.data() + sizeof(hci_cmd_hdr_t), &command, sizeof(command));

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

  const int ret = libusb_control_transfer(m_handle, REQUEST_TYPE, 0, 0, 0, packet.data(),
                                          static_cast<u16>(packet.size()), TIMEOUT);
  if (ret < LIBUSB_SUCCESS)
    WARN_LOG_FMT(IOS_WIIMOTE, "libusb_control_transfer failed: {}", LibusbUtils::ErrorWrap(ret));
  return true;
}

void BluetoothRealDevice::FakeVendorCommandReply(USB::V0IntrMessage& ctrl)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  SHCIEventCommand hci_event;
  memory.CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event.PayloadLength = sizeof(SHCIEventCommand) - 2;
  hci_event.PacketIndicator = 0x01;
  hci_event.Opcode = m_fake_vendor_command_reply_opcode;
  memory.CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request, static_cast<s32>(sizeof(hci_event)));
}

// Due to how the widcomm stack which Nintendo uses is coded, we must never
// let the stack think the controller is buffering more than 10 data packets
// - it will cause a u8 underflow and royally screw things up.
// Therefore, the reply to this command has to be faked to avoid random, weird issues
// (including Wiimote disconnects and "event mismatch" warning messages).
void BluetoothRealDevice::FakeReadBufferSizeReply(USB::V0IntrMessage& ctrl)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  SHCIEventCommand hci_event;
  memory.CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event.PayloadLength = sizeof(SHCIEventCommand) - 2 + sizeof(hci_read_buffer_size_rp);
  hci_event.PacketIndicator = 0x01;
  hci_event.Opcode = HCI_CMD_READ_BUFFER_SIZE;
  memory.CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));

  hci_read_buffer_size_rp reply;
  reply.status = 0x00;
  reply.max_acl_size = ACL_PKT_SIZE;
  reply.num_acl_pkts = ACL_PKT_NUM;
  reply.max_sco_size = SCO_PKT_SIZE;
  reply.num_sco_pkts = SCO_PKT_NUM;
  memory.CopyToEmu(ctrl.data_address + sizeof(hci_event), &reply, sizeof(reply));
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request,
                                       static_cast<s32>(sizeof(hci_event) + sizeof(reply)));
}

void BluetoothRealDevice::FakeSyncButtonEvent(USB::V0IntrMessage& ctrl, const u8* payload,
                                              const u8 size)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_event_hdr_t hci_event;
  memory.CopyFromEmu(&hci_event, ctrl.data_address, sizeof(hci_event));
  hci_event.event = HCI_EVENT_VENDOR;
  hci_event.length = size;
  memory.CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));
  memory.CopyToEmu(ctrl.data_address + sizeof(hci_event), payload, size);
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request,
                                       static_cast<s32>(sizeof(hci_event) + size));
}

// When the red sync button is pressed, a HCI event is generated:
//   > HCI Event: Vendor (0xff) plen 1
//   08
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void BluetoothRealDevice::FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button pressed' (0x08) event packet");
  constexpr u8 payload[1] = {0x08};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void BluetoothRealDevice::FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  constexpr u8 payload[1] = {0x09};
  FakeSyncButtonEvent(ctrl, payload, sizeof(payload));
  m_sync_button_state = SyncButtonState::Ignored;
}

void BluetoothRealDevice::LoadLinkKeys()
{
  std::string entries = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS);
  if (entries.empty())
    return;
  for (const auto& pair : SplitString(entries, ','))
  {
    const auto index = pair.find('=');
    if (index == std::string::npos)
      continue;

    const std::string address_string = pair.substr(0, index);
    std::optional<bdaddr_t> address = Common::StringToMacAddress(address_string);
    if (!address)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE,
                    "Malformed MAC address ({}). Skipping loading of current link key.",
                    address_string);
      continue;
    }

    auto& mac = address.value();
    std::reverse(mac.begin(), mac.end());

    const std::string& key_string = pair.substr(index + 1);
    linkkey_t key{};
    size_t pos = 0;
    for (size_t i = 0; i < key_string.length(); i = i + 2)
    {
      int value;
      std::istringstream(key_string.substr(i, 2)) >> std::hex >> value;
      key[pos++] = value;
    }

    m_link_keys[mac] = key;
  }
}

void BluetoothRealDevice::SaveLinkKeys()
{
  std::ostringstream oss;
  for (const auto& entry : m_link_keys)
  {
    bdaddr_t address;
    // Reverse the address so that it is stored in the correct order in the config file
    std::reverse_copy(entry.first.begin(), entry.first.end(), address.begin());
    oss << Common::MacAddressToString(address);
    oss << '=';
    oss << std::hex;
    for (u8 data : entry.second)
    {
      // We cast to u16 here in order to have it displayed as two nibbles.
      oss << std::setfill('0') << std::setw(2) << static_cast<u16>(data);
    }
    oss << std::dec << ',';
  }
  std::string config_string = oss.str();
  if (!config_string.empty())
    config_string.pop_back();
  Config::SetBase(Config::MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS, config_string);
}

bool BluetoothRealDevice::OpenDevice(libusb_device* device)
{
  m_device = libusb_ref_device(device);
  const int ret = libusb_open(m_device, &m_handle);
  if (ret != LIBUSB_SUCCESS)
  {
    m_last_open_error =
        Common::FmtFormatT("Failed to open Bluetooth device: {0}", LibusbUtils::ErrorWrap(ret));
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

// The callbacks are called from libusb code on a separate thread.
void BluetoothRealDevice::HandleCtrlTransfer(libusb_transfer* tr)
{
  std::lock_guard lk(m_transfers_mutex);
  if (!m_current_transfers.count(tr))
    return;

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "libusb command transfer failed, status: {:#04x}",
                  Common::ToUnderlying(tr->status));
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
  GetEmulationKernel().EnqueueIPCReply(command->ios_request, tr->actual_length, 0,
                                       CoreTiming::FromThread::ANY);
  m_current_transfers.erase(tr);
}

void BluetoothRealDevice::HandleBulkOrIntrTransfer(libusb_transfer* tr)
{
  std::lock_guard lk(m_transfers_mutex);
  if (!m_current_transfers.count(tr))
    return;

  if (tr->status != LIBUSB_TRANSFER_COMPLETED && tr->status != LIBUSB_TRANSFER_TIMED_OUT &&
      tr->status != LIBUSB_TRANSFER_NO_DEVICE)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "libusb transfer failed, status: {:#04x}",
                  Common::ToUnderlying(tr->status));
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
    const u8 event = tr->buffer[0];
    if (event == HCI_EVENT_LINK_KEY_NOTIFICATION)
    {
      hci_link_key_notification_ep notification;
      std::memcpy(&notification, tr->buffer + sizeof(hci_event_hdr_t), sizeof(notification));
      linkkey_t key;
      std::copy(std::begin(notification.key), std::end(notification.key), std::begin(key));
      m_link_keys[notification.bdaddr] = key;
    }
    else if (event == HCI_EVENT_COMMAND_COMPL)
    {
      hci_command_compl_ep complete_event;
      std::memcpy(&complete_event, tr->buffer + sizeof(hci_event_hdr_t), sizeof(complete_event));
      if (complete_event.opcode == HCI_CMD_RESET)
        m_need_reset_keys.Set();
    }
  }

  const auto& command = m_current_transfers.at(tr).command;
  command->FillBuffer(tr->buffer, tr->actual_length);
  GetEmulationKernel().EnqueueIPCReply(command->ios_request, tr->actual_length, 0,
                                       CoreTiming::FromThread::ANY);
  m_current_transfers.erase(tr);
}
}  // namespace IOS::HLE
