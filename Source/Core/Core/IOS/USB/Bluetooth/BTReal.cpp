// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Bluetooth/BTReal.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <ranges>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Host.h"
#include "Core/System.h"

#include "VideoCommon/OnScreenDisplay.h"

namespace IOS::HLE
{

// FYI: Some BT adapters don't handle storing link keys very well.
// Some can't store very many keys. Some can't store any at all.
// So we entirely fake stored link keys for non-Wii BT adapters.
static constexpr bool SHOULD_FAKE_STORED_KEYS_ON_OTHER_ADAPTERS = true;

BluetoothRealDevice::BluetoothRealDevice(EmulationKernel& ios, const std::string& device_name)
    : BluetoothBaseDevice(ios, device_name)
{
  LoadLinkKeys();
}

BluetoothRealDevice::~BluetoothRealDevice()
{
  SaveLinkKeys();
}

std::optional<IPCReply> BluetoothRealDevice::Open(const OpenRequest& request)
{
  m_lib_usb_bt_adapter = std::make_unique<LibUSBBluetoothAdapter>();

  // Attempt to have a consistent initial state.
  SendHCIReset();
  SendHCIDeleteLinkKeys();

  // This is a good place to restore our link keys.
  // We need to do this because the adapter was potentially being controlled by
  // the host OS bluetooth stack or a Dolphin instance with different link keys.
  if (!ShouldFakeStoredLinkKeys())
    SendHCIWriteLinkKeys();

  return Device::Open(request);
}

std::optional<IPCReply> BluetoothRealDevice::Close(u32 fd)
{
  m_hci_endpoint.reset();
  m_acl_endpoint.reset();

  m_fake_replies = {};

  // FYI: LibUSBBluetoothAdapter destruction will attempt to wait for command completion.
  SendHCIReset();

  m_lib_usb_bt_adapter.reset();

  return Device::Close(fd);
}

void BluetoothRealDevice::HandleHCICommand(const USB::V0CtrlMessage& cmd)
{
  auto& memory = GetSystem().GetMemory();
  const u16 opcode = Common::swap16(memory.Read_U16(cmd.data_address));

  switch (opcode)
  {
  case 0xFC4C:
  case 0xFC4F:
  {
    if (!m_lib_usb_bt_adapter->IsWiiBTModule())
    {
      QueueFakeReply(&BluetoothRealDevice::FakeVendorCommand, this, opcode);
      return;
    }
    break;
  }
  case HCI_CMD_READ_STORED_LINK_KEY:
  {
    if (ShouldFakeStoredLinkKeys())
    {
      hci_read_stored_link_key_cp read_cmd;
      memory.CopyFromEmu(&read_cmd, cmd.data_address + sizeof(hci_cmd_hdr_t), sizeof(read_cmd));

      FakeReadStoredLinkKey(read_cmd);
      return;
    }
    break;
  }
  case HCI_CMD_WRITE_STORED_LINK_KEY:
  {
    hci_write_stored_link_key_cp write_cmd;
    memory.CopyFromEmu(&write_cmd, cmd.data_address + sizeof(hci_cmd_hdr_t), sizeof(write_cmd));

    for (u32 i = 0; i != write_cmd.num_keys_write; ++i)
    {
      struct
      {
        bdaddr_t bdaddr;
        linkkey_t key;
      } entry{};
      memory.CopyFromEmu(&entry,
                         cmd.data_address + sizeof(hci_cmd_hdr_t) + sizeof(write_cmd) +
                             (sizeof(entry) * i),
                         sizeof(entry));

      // Update link key in our own storage when console writes to the adapter.
      // INFO_LOG_FMT(IOS_WIIMOTE, "write link key: {} {}",
      //              HexDump(std::data(entry.bdaddr), std::size(entry.bdaddr)),
      //              HexDump(std::data(entry.key), std::size(entry.key)));
      m_link_keys[entry.bdaddr] = entry.key;
    }

    if (ShouldFakeStoredLinkKeys())
    {
      QueueFakeReply(&BluetoothRealDevice::FakeWriteStoredLinkKey, this, write_cmd.num_keys_write);
      return;
    }
    break;
  }
  case HCI_CMD_DELETE_STORED_LINK_KEY:
  {
    hci_delete_stored_link_key_cp delete_cmd;
    memory.CopyFromEmu(&delete_cmd, cmd.data_address + sizeof(hci_cmd_hdr_t), sizeof(delete_cmd));

    u16 num_keys_deleted = u16(m_link_keys.size());

    // Delete link keys from our own storage when the game makes the adapter do so.
    if (delete_cmd.delete_all == 0x00)
      num_keys_deleted = u16(m_link_keys.erase(delete_cmd.bdaddr));
    else
      m_link_keys.clear();

    if (ShouldFakeStoredLinkKeys())
    {
      QueueFakeReply(&BluetoothRealDevice::FakeDeleteStoredLinkKey, this, num_keys_deleted);
      return;
    }
    break;
  }
  case HCI_CMD_SNIFF_MODE:
    // FYI: This is what causes Wii Remotes to operate at 200hz instead of 100hz.
    DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI_CMD_SNIFF_MODE");
    break;
  default:
    break;
  }

  // Send the command to the physical bluetooth adapter.
  const auto payload = memory.GetSpanForAddress(cmd.data_address).first(cmd.length);
  m_lib_usb_bt_adapter->ScheduleControlTransfer(cmd.request_type, cmd.request, cmd.value, cmd.index,
                                                payload, GetTargetTime());
}

std::optional<IPCReply> BluetoothRealDevice::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  // HCI commands to the Bluetooth adapter
  case USB::IOCTLV_USBV0_CTRLMSG:
  {
    auto cmd = USB::V0CtrlMessage(GetEmulationKernel(), request);
    HandleHCICommand(cmd);
    return IPCReply{cmd.length};
  }
  // ACL data (incoming or outgoing)
  case USB::IOCTLV_USBV0_BLKMSG:
  {
    auto cmd = std::make_unique<USB::V0BulkMessage>(GetEmulationKernel(), request);
    if (cmd->endpoint == ACL_DATA_IN)
    {
      m_acl_endpoint = std::move(cmd);
      TryToFillACLEndpoint();
      return std::nullopt;
    }

    auto& memory = GetSystem().GetMemory();
    const auto payload = memory.GetSpanForAddress(cmd->data_address).first(cmd->length);
    m_lib_usb_bt_adapter->ScheduleBulkTransfer(cmd->endpoint, payload, GetTargetTime());
    break;
  }
  // Incoming HCI events
  case USB::IOCTLV_USBV0_INTRMSG:
  {
    auto cmd = std::make_unique<USB::V0IntrMessage>(GetEmulationKernel(), request);
    if (cmd->endpoint == HCI_EVENT)
    {
      m_hci_endpoint = std::move(cmd);
      TryToFillHCIEndpoint();
      return std::nullopt;
    }

    ERROR_LOG_FMT(IOS_WIIMOTE, "IOCTLV_USBV0_INTRMSG: Unknown endpoint: 0x{:02x}", cmd->endpoint);
  }
  default:
    ERROR_LOG_FMT(IOS_WIIMOTE, "IOCtlV: Unknown request: 0x{:08x}", request.request);
    break;
  }

  return IPCReply{IPC_SUCCESS};
}

void BluetoothRealDevice::Update()
{
  TryToFillHCIEndpoint();
  TryToFillACLEndpoint();
}

void BluetoothRealDevice::TryToFillHCIEndpoint()
{
  if (!m_hci_endpoint)
    return;

  if (m_sync_button_state == SyncButtonState::Pressed)
  {
    Core::DisplayMessage("Scanning for Wii Remotes", 2000);
    FakeSyncButtonPressedEvent(*m_hci_endpoint);
  }
  else if (m_sync_button_state == SyncButtonState::LongPressed)
  {
    Core::DisplayMessage("Reset saved Wii Remote pairings", 2000);
    FakeSyncButtonHeldEvent(*m_hci_endpoint);
  }
  else if (!m_fake_replies.empty())
  {
    std::invoke(m_fake_replies.front(), *m_hci_endpoint);
    m_fake_replies.pop();
  }
  else
  {
    const auto buffer = ProcessHCIEvent(m_lib_usb_bt_adapter->ReceiveHCIEvent());
    if (buffer.empty())
      return;

    assert(buffer.size() <= m_hci_endpoint->length);

    m_hci_endpoint->FillBuffer(buffer.data(), buffer.size());
    GetEmulationKernel().EnqueueIPCReply(m_hci_endpoint->ios_request, s32(buffer.size()));
  }

  m_hci_endpoint.reset();
}

auto BluetoothRealDevice::ProcessHCIEvent(BufferType buffer) -> BufferType
{
  if (buffer.empty())
    return buffer;

  const auto event = buffer[0];
  if (event == HCI_EVENT_LINK_KEY_REQ && ShouldFakeStoredLinkKeys())
  {
    // The controller is requesting a link key from host storage.
    // This is normal procedure when keys are not stored on the controller.
    // I'd expect the emulated software to be able to respond to this.
    // But it seems to often ignore the request, so we'll fake.

    hci_link_key_req_ep link_key_req;
    std::memcpy(&link_key_req, buffer.data() + sizeof(hci_event_hdr_t), sizeof(link_key_req));

    const auto key_iter = m_link_keys.find(link_key_req.bdaddr);
    if (key_iter != m_link_keys.end())
    {
      HCICommandPayload<HCI_CMD_LINK_KEY_REP, hci_link_key_rep_cp> payload;
      payload.command.bdaddr = link_key_req.bdaddr;
      std::ranges::copy(key_iter->second, payload.command.key);

      INFO_LOG_FMT(IOS_WIIMOTE, "Sending link key to controller");
      m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(payload));
    }
    else
    {
      HCICommandPayload<HCI_CMD_LINK_KEY_NEG_REP, hci_link_key_neg_rep_cp> payload;
      payload.command.bdaddr = link_key_req.bdaddr;

      INFO_LOG_FMT(IOS_WIIMOTE, "Sending negative link key reply");
      m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(payload));
    }

    // Don't let the emulated software see the request.
    return {};
  }

  if (m_lib_usb_bt_adapter->IsWiiBTModule())
    return buffer;

  // Handle some more quirks for non-Wii BT adapters below.

  if (event == HCI_EVENT_COMMAND_COMPL)
  {
    hci_command_compl_ep ev;
    std::memcpy(&ev, buffer.data() + sizeof(hci_event_hdr_t), sizeof(ev));

    if (ev.opcode == HCI_CMD_READ_BUFFER_SIZE)
    {
      hci_read_buffer_size_rp reply;
      std::memcpy(&reply, buffer.data() + sizeof(hci_event_hdr_t) + sizeof(ev), sizeof(reply));

      if (reply.status != 0x00)
      {
        ERROR_LOG_FMT(IOS_WIIMOTE, "HCI_CMD_READ_BUFFER_SIZE status: 0x{:02x}.", reply.status);

        reply.status = 0x00;
        reply.num_acl_pkts = ACL_PKT_NUM;
      }
      else if (reply.num_acl_pkts > ACL_PKT_NUM)
      {
        // Due to how the widcomm stack which Nintendo uses is coded, we must never
        // let the stack think the controller is buffering more than 10 data packets
        // - it will cause a u8 underflow and royally screw things up.
        // Therefore, the reply to this command has to be faked to avoid random, weird issues
        // (including Wiimote disconnects and "event mismatch" warning messages).
        reply.num_acl_pkts = ACL_PKT_NUM;
      }
      else if (reply.num_acl_pkts < ACL_PKT_NUM)
      {
        // The controller buffers fewer ACL packets than the original BT module.
        WARN_LOG_FMT(IOS_WIIMOTE, "HCI_CMD_READ_BUFFER_SIZE num_acl_pkts({}) < ACL_PKT_NUM({})",
                     reply.num_acl_pkts, ACL_PKT_NUM);
      }

      // Force the other the parameters to match that of the original BT module.
      reply.max_acl_size = ACL_PKT_SIZE;
      reply.max_sco_size = SCO_PKT_SIZE;
      reply.num_sco_pkts = SCO_PKT_NUM;

      std::memcpy(buffer.data() + sizeof(hci_event_hdr_t) + sizeof(ev), &reply, sizeof(reply));
    }
  }
  else if (event == HCI_EVENT_CON_COMPL)
  {
    // Some devices (e.g. Sena UD100 0a12:0001) default to HCI_SERVICE_TYPE_BEST_EFFORT.
    // This can cause less than 200hz input and drop-outs in some games (e.g. Brawl).

    // We configure HCI_SERVICE_TYPE_GUARANTEED for each new connection.
    // This solves dropped input issues at least for the mentioned Sena adapter.

    INFO_LOG_FMT(IOS_WIIMOTE, "Sending QOS_SETUP");

    HCICommandPayload<HCI_CMD_QOS_SETUP, hci_qos_setup_cp> payload;

    // Copy the connection handle.
    std::memcpy(&payload.command.con_handle, buffer.data() + 3, sizeof(payload.command.con_handle));

    payload.command.service_type = HCI_SERVICE_TYPE_GUARANTEED;
    payload.command.token_rate = 0xffffffff;
    payload.command.peak_bandwidth = 0xffffffff;
    payload.command.latency = 10000;
    payload.command.delay_variation = 0xffffffff;

    m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(payload));
  }
  else if (event == HCI_EVENT_QOS_SETUP_COMPL)
  {
    const auto service_type = buffer[6];
    if (service_type != HCI_SERVICE_TYPE_GUARANTEED)
      WARN_LOG_FMT(IOS_WIIMOTE, "Got QOS service_type: {}", service_type);
    else
      INFO_LOG_FMT(IOS_WIIMOTE, "Got QOS SERVICE_TYPE_GUARANTEED");
  }

  return buffer;
}

void BluetoothRealDevice::TryToFillACLEndpoint()
{
  if (!m_acl_endpoint)
    return;

  // Throttle to minimize input latency.
  auto& core_timing = GetSystem().GetCoreTiming();
  core_timing.Throttle(core_timing.GetTicks());

  const auto buffer = m_lib_usb_bt_adapter->ReceiveACLData();
  if (buffer.empty())
    return;

  assert(buffer.size() <= m_acl_endpoint->length);

  m_acl_endpoint->FillBuffer(buffer.data(), buffer.size());
  GetEmulationKernel().EnqueueIPCReply(m_acl_endpoint->ios_request, s32(buffer.size()));

  m_acl_endpoint.reset();
}

TimePoint BluetoothRealDevice::GetTargetTime() const
{
  auto& core_timing = GetSystem().GetCoreTiming();
  return core_timing.GetTargetHostTime(core_timing.GetTicks());
}

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

  DoStateForMessage(GetEmulationKernel(), p, m_hci_endpoint);
  DoStateForMessage(GetEmulationKernel(), p, m_acl_endpoint);

  if (p.IsReadMode())
  {
    OSD::AddMessage("If the savestate does not load correctly, disconnect all Wii Remotes "
                    "and reload it.",
                    OSD::Duration::NORMAL);
  }

  static bool s_has_shown_savestate_warning = false;

  if (!s_has_shown_savestate_warning && p.IsWriteMode())
  {
    OSD::AddMessage("Savestates may not work with Bluetooth passthrough in all cases.\n"
                    "They will only work if no remote is connected when restoring the state,\n"
                    "or no remote is disconnected after saving.",
                    OSD::Duration::VERY_LONG);
    s_has_shown_savestate_warning = true;
  }
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

void BluetoothRealDevice::SendHCIReset()
{
  INFO_LOG_FMT(IOS_WIIMOTE, "Sending HCI Reset");
  m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(hci_cmd_hdr_t{HCI_CMD_RESET, 0}));
}

void BluetoothRealDevice::SendHCIDeleteLinkKeys()
{
  INFO_LOG_FMT(IOS_WIIMOTE, "Sending DELETE_STORED_LINK_KEY");

  HCICommandPayload<HCI_CMD_DELETE_STORED_LINK_KEY, hci_delete_stored_link_key_cp> payload;
  payload.command.bdaddr = {};
  payload.command.delete_all = 0x01;

  m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(payload));
}

bool BluetoothRealDevice::SendHCIWriteLinkKeys()
{
  if (m_link_keys.empty())
    return false;

  struct
  {
    hci_cmd_hdr_t header{HCI_CMD_WRITE_STORED_LINK_KEY};
    hci_write_stored_link_key_cp command{};
    bdaddr_t bdaddr{};
    linkkey_t linkkey{};
  } payload;
  static_assert(sizeof(payload) == 4 + (6 + 16));

  payload.header.length = sizeof(payload) - sizeof(payload.header);
  payload.command.num_keys_write = 1;

  for (auto& [bdaddr, linkkey] : m_link_keys)
  {
    payload.bdaddr = bdaddr;
    payload.linkkey = linkkey;

    INFO_LOG_FMT(IOS_WIIMOTE, "Sending WRITE_STORED_LINK_KEY num_link_keys: {}", 1);
    m_lib_usb_bt_adapter->SendControlTransfer(Common::AsU8Span(payload));
  }

  return true;
}

template <typename... Args>
void BluetoothRealDevice::QueueFakeReply(Args&&... args)
{
  m_fake_replies.emplace(std::bind_front(std::forward<Args>(args)...));
}

void BluetoothRealDevice::FakeEvent(u8 event, std::span<const u8> payload, USB::V0IntrMessage& ctrl)
{
  const hci_event_hdr_t header{.event = event, .length = u8(payload.size())};

  auto& memory = GetSystem().GetMemory();

  memory.CopyToEmu(ctrl.data_address, &header, sizeof(header));
  memory.CopyToEmu(ctrl.data_address + sizeof(header), payload.data(), payload.size());
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request, s32(sizeof(header) + header.length));
}

void BluetoothRealDevice::FakeCommandComplete(u16 opcode, std::span<const u8> payload,
                                              USB::V0IntrMessage& ctrl)
{
  struct
  {
    hci_event_hdr_t header{HCI_EVENT_COMMAND_COMPL};
    hci_command_compl_ep command{};
  } data;

  data.header.length = u8(sizeof(data.command) + payload.size());

  data.command.num_cmd_pkts = 0x01;
  data.command.opcode = opcode;

  auto& memory = GetSystem().GetMemory();

  memory.CopyToEmu(ctrl.data_address, &data, sizeof(data));
  memory.CopyToEmu(ctrl.data_address + sizeof(data), payload.data(), payload.size());
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request, s32(sizeof(data)));
}

void BluetoothRealDevice::FakeVendorCommand(u16 opcode, USB::V0IntrMessage& ctrl)
{
  DEBUG_LOG_FMT(IOS_WIIMOTE, "Faking vendor command");
  FakeCommandComplete(opcode, {}, ctrl);
}

// When the red sync button is pressed, a HCI event is generated.
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void BluetoothRealDevice::FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button pressed' (0x08) event packet");
  constexpr u8 payload[1] = {0x08};
  FakeEvent(HCI_EVENT_VENDOR, payload, ctrl);
  m_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void BluetoothRealDevice::FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  constexpr u8 payload[1] = {0x09};
  FakeEvent(HCI_EVENT_VENDOR, payload, ctrl);
  m_sync_button_state = SyncButtonState::Ignored;
}

bool BluetoothRealDevice::ShouldFakeStoredLinkKeys() const
{
  return SHOULD_FAKE_STORED_KEYS_ON_OTHER_ADAPTERS && !m_lib_usb_bt_adapter->IsWiiBTModule();
}

void BluetoothRealDevice::FakeReadStoredLinkKey(hci_read_stored_link_key_cp cmd)
{
  // Default to reading all keys.
  std::ranges::subrange keys_to_read{m_link_keys.begin(), m_link_keys.end()};

  // Read just one key (if we actually have it stored).
  if (cmd.read_all == 0x00)
  {
    const auto range = m_link_keys.equal_range(cmd.bdaddr);
    keys_to_read = std::ranges::subrange{range.first, range.second};
  }

  u16 num_keys = 0;
  for (const auto& [addr, key] : keys_to_read)
  {
    ++num_keys;
    QueueFakeReply(&BluetoothRealDevice::FakeReturnLinkKey, this, addr, key);
  }

  INFO_LOG_FMT(IOS_WIIMOTE, "Faking READ_STORED_LINK_KEY num_keys_read: {}", num_keys);

  QueueFakeReply([this, num_keys](USB::V0IntrMessage& ctrl) {
    hci_read_stored_link_key_rp reply{
        .status = 0x00, .max_num_keys = 255, .num_keys_read = num_keys};
    FakeCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, Common::AsU8Span(reply), ctrl);
  });
}

void BluetoothRealDevice::FakeReturnLinkKey(bdaddr_t bdaddr, linkkey_t key,
                                            USB::V0IntrMessage& ctrl)
{
  struct
  {
    hci_return_link_keys_ep event{.num_keys = 1};
    bdaddr_t bdaddr{};
    linkkey_t key{};
  } payload{.bdaddr = bdaddr, .key = key};

  FakeEvent(HCI_EVENT_RETURN_LINK_KEYS, Common::AsU8Span(payload), ctrl);
}

void BluetoothRealDevice::FakeWriteStoredLinkKey(u8 key_count, USB::V0IntrMessage& ctrl)
{
  INFO_LOG_FMT(IOS_WIIMOTE, "Faking WRITE_STORED_LINK_KEY num_keys_written: {}", key_count);

  const hci_write_stored_link_key_rp reply{.status = 0x00, .num_keys_written = key_count};
  FakeCommandComplete(HCI_CMD_WRITE_STORED_LINK_KEY, Common::AsU8Span(reply), ctrl);
}

void BluetoothRealDevice::FakeDeleteStoredLinkKey(u16 key_count, USB::V0IntrMessage& ctrl)
{
  INFO_LOG_FMT(IOS_WIIMOTE, "Faking DELETE_STORED_LINK_KEY num_keys_deleted: {}", key_count);

  const hci_delete_stored_link_key_rp reply{.status = 0x00, .num_keys_deleted = key_count};
  FakeCommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY, Common::AsU8Span(reply), ctrl);
}

void BluetoothRealDevice::LoadLinkKeys()
{
  std::string entries = Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS);
  if (entries.empty())
    return;
  for (std::string_view pair : SplitString(entries, ','))
  {
    const auto index = pair.find('=');
    if (index == std::string::npos)
      continue;

    const auto address_string = pair.substr(0, index);
    std::optional<bdaddr_t> address = Common::StringToMacAddress(address_string);
    if (!address)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE,
                    "Malformed MAC address ({}). Skipping loading of current link key.",
                    address_string);
      continue;
    }

    auto& mac = *address;
    std::ranges::reverse(mac);

    const auto key_string = pair.substr(index + 1);
    linkkey_t key{};
    size_t pos = 0;
    for (size_t i = 0; i < key_string.length() && pos != key.size(); i += 2)
      TryParse(std::string(key_string.substr(i, 2)), &key[pos++], 16);

    m_link_keys[mac] = key;
  }
}

void BluetoothRealDevice::SaveLinkKeys()
{
  std::string config_string;
  for (const auto& [bdaddr, linkkey] : m_link_keys)
  {
    bdaddr_t address;
    // Reverse the address so that it is stored in the correct order in the config file
    std::ranges::reverse_copy(bdaddr, address.begin());

    config_string += Common::MacAddressToString(address);
    config_string += '=';
    for (u8 c : linkkey)
      config_string += fmt::format("{:02x}", c);

    config_string += ',';
  }

  if (!config_string.empty())
    config_string.pop_back();

  Config::SetBase(Config::MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS, config_string);
}

}  // namespace IOS::HLE
