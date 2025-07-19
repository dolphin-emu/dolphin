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

namespace
{
template <u16 Opcode, typename CommandType>
struct HCICommandPayload
{
  hci_cmd_hdr_t header{Opcode, sizeof(CommandType)};
  CommandType command{};
};

template <typename T>
requires(std::is_trivially_copyable_v<T>)
constexpr auto AsU8Span(const T& obj)
{
  return std::span{reinterpret_cast<const u8*>(std::addressof(obj)), sizeof(obj)};
}

}  // namespace

namespace IOS::HLE
{

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

  return Device::Open(request);
}

std::optional<IPCReply> BluetoothRealDevice::Close(u32 fd)
{
  m_hci_endpoint.reset();
  m_acl_endpoint.reset();

  m_fake_replies = {};

  // FYI: LibUSBBluetoothAdapter destruction will attempt to wait for command completion.
  SendHCIResetCommand();

  m_lib_usb_bt_adapter.reset();

  return Device::Close(fd);
}

std::optional<IPCReply> BluetoothRealDevice::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  // HCI commands to the Bluetooth adapter
  case USB::IOCTLV_USBV0_CTRLMSG:
  {
    auto& memory = GetSystem().GetMemory();

    auto cmd = std::make_unique<USB::V0CtrlMessage>(GetEmulationKernel(), request);
    const u16 opcode = Common::swap16(memory.Read_U16(cmd->data_address));
    if (!m_lib_usb_bt_adapter->IsWiiBTModule() && (opcode == 0xFC4C || opcode == 0xFC4F))
    {
      m_fake_replies.emplace(
          std::bind_front(&BluetoothRealDevice::FakeVendorCommandReply, this, opcode));
    }
    else
    {
      if (opcode == HCI_CMD_DELETE_STORED_LINK_KEY)
      {
        INFO_LOG_FMT(IOS_WIIMOTE, "HCI_CMD_DELETE_STORED_LINK_KEY");

        // Delete link key(s) from our own link key storage when the game tells the adapter to
        hci_delete_stored_link_key_cp delete_cmd;
        memory.CopyFromEmu(&delete_cmd, cmd->data_address, sizeof(delete_cmd));

        if (delete_cmd.delete_all != 0)
          m_link_keys.clear();
        else
          m_link_keys.erase(delete_cmd.bdaddr);
      }
      else if (opcode == HCI_CMD_SNIFF_MODE)
      {
        // FYI: This is what causes Wii Remotes to operate at 200hz instead of 100hz.
        DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI_CMD_SNIFF_MODE");
      }

      const auto payload = memory.GetSpanForAddress(cmd->data_address).first(cmd->length);
      m_lib_usb_bt_adapter->ScheduleControlTransfer(cmd->request_type, cmd->request, cmd->value,
                                                    cmd->index, payload, GetTargetTime());

      if (opcode == HCI_CMD_RESET)
      {
        // After the console issues HCI reset is a good place to restore our link keys.
        //  We need to do this because:
        // Some adapters apparently incorrectly delete keys on HCI reset.
        // The adapter was potentially being controlled by the host OS bluetooth stack
        //  or a Dolphin instance with different link keys.
        SendHCIDeleteLinkKeyCommand();
        SendHCIStoreLinkKeyCommand();
      }
    }
    return IPCReply{cmd->length};
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
  if (event == HCI_EVENT_LINK_KEY_NOTIFICATION)
  {
    INFO_LOG_FMT(IOS_WIIMOTE, "HCI_EVENT_LINK_KEY_NOTIFICATION");

    hci_link_key_notification_ep notification;
    std::memcpy(&notification, buffer.data() + sizeof(hci_event_hdr_t), sizeof(notification));
    std::ranges::copy(notification.key, std::begin(m_link_keys[notification.bdaddr]));
  }

  if (m_lib_usb_bt_adapter->IsWiiBTModule())
    return buffer;

  // Handle some quirks for non-Wii BT adapters below.

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

    INFO_LOG_FMT(IOS_WIIMOTE, "Sending HCI_CMD_QOS_SETUP");

    HCICommandPayload<HCI_CMD_QOS_SETUP, hci_qos_setup_cp> payload;

    // Copy the connection handle.
    std::memcpy(&payload.command.con_handle, buffer.data() + 3, sizeof(payload.command.con_handle));

    payload.command.service_type = HCI_SERVICE_TYPE_GUARANTEED;
    payload.command.token_rate = 0xffffffff;
    payload.command.peak_bandwidth = 0xffffffff;
    payload.command.latency = 10000;
    payload.command.delay_variation = 0xffffffff;

    m_lib_usb_bt_adapter->SendControlTransfer(AsU8Span(payload));
  }
  else if (event == HCI_EVENT_QOS_SETUP_COMPL)
  {
    const auto service_type = buffer[6];
    if (service_type != HCI_SERVICE_TYPE_GUARANTEED)
      WARN_LOG_FMT(IOS_WIIMOTE, "Got HCI_EVENT_QOS_SETUP_COMPL service_type: {}", service_type);
    else
      INFO_LOG_FMT(IOS_WIIMOTE, "Got HCI_EVENT_QOS_SETUP_COMPL HCI_SERVICE_TYPE_GUARANTEED");
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

void BluetoothRealDevice::SendHCIResetCommand()
{
  INFO_LOG_FMT(IOS_WIIMOTE, "SendHCIResetCommand");
  m_lib_usb_bt_adapter->SendControlTransfer(AsU8Span(hci_cmd_hdr_t{HCI_CMD_RESET, 0}));
}

void BluetoothRealDevice::SendHCIDeleteLinkKeyCommand()
{
  INFO_LOG_FMT(IOS_WIIMOTE, "SendHCIDeleteLinkKeyCommand");

  HCICommandPayload<HCI_CMD_DELETE_STORED_LINK_KEY, hci_delete_stored_link_key_cp> payload;
  payload.command.bdaddr = {};
  payload.command.delete_all = 0x01;

  m_lib_usb_bt_adapter->SendControlTransfer(AsU8Span(payload));
}

bool BluetoothRealDevice::SendHCIStoreLinkKeyCommand()
{
  if (m_link_keys.empty())
    return false;

  // Range: 0x01 to 0x0B per Bluetooth spec.
  static constexpr std::size_t MAX_LINK_KEYS = 0x0B;
  const auto num_link_keys = u8(std::min(m_link_keys.size(), MAX_LINK_KEYS));

  INFO_LOG_FMT(IOS_WIIMOTE, "SendHCIStoreLinkKeyCommand num_link_keys: {}", num_link_keys);

  struct Payload
  {
    hci_cmd_hdr_t header{HCI_CMD_WRITE_STORED_LINK_KEY};
    hci_write_stored_link_key_cp command{};
    struct LinkKey
    {
      bdaddr_t bdaddr;
      linkkey_t linkkey;
    };
    std::array<LinkKey, MAX_LINK_KEYS> link_keys{};
  } payload;
  static_assert(sizeof(Payload) == 4 + (6 + 16) * MAX_LINK_KEYS);

  const u8 payload_size =
      sizeof(payload.header) + sizeof(payload.command) + (sizeof(Payload::LinkKey) * num_link_keys);

  payload.header.length = payload_size - sizeof(payload.header);
  payload.command.num_keys_write = num_link_keys;

  int index = 0;
  for (auto& [bdaddr, linkkey] : m_link_keys | std::views::take(num_link_keys))
    payload.link_keys[index++] = {bdaddr, linkkey};

  m_lib_usb_bt_adapter->SendControlTransfer(AsU8Span(payload).first(payload_size));
  return true;
}

void BluetoothRealDevice::FakeVendorCommandReply(u16 opcode, USB::V0IntrMessage& ctrl)
{
  DEBUG_LOG_FMT(IOS_WIIMOTE, "FakeVendorCommandReply");

  struct Payload
  {
    hci_event_hdr_t header{HCI_EVENT_COMMAND_COMPL};
    hci_command_compl_ep command{};
  } payload;

  payload.header.length = sizeof(payload) - sizeof(payload.header);

  payload.command.num_cmd_pkts = 0x01;
  payload.command.opcode = opcode;

  assert(sizeof(payload) <= ctrl.length);

  GetSystem().GetMemory().CopyToEmu(ctrl.data_address, &payload, sizeof(payload));
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request, s32(sizeof(payload)));
}

void BluetoothRealDevice::FakeSyncButtonEvent(USB::V0IntrMessage& ctrl, std::span<const u8> payload)
{
  const hci_event_hdr_t hci_event{HCI_EVENT_VENDOR, u8(payload.size())};

  auto& memory = GetSystem().GetMemory();

  memory.CopyToEmu(ctrl.data_address, &hci_event, sizeof(hci_event));
  memory.CopyToEmu(ctrl.data_address + sizeof(hci_event), payload.data(), payload.size());
  GetEmulationKernel().EnqueueIPCReply(ctrl.ios_request, s32(sizeof(hci_event) + payload.size()));
}

// When the red sync button is pressed, a HCI event is generated.
// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
void BluetoothRealDevice::FakeSyncButtonPressedEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button pressed' (0x08) event packet");
  constexpr u8 payload[1] = {0x08};
  FakeSyncButtonEvent(ctrl, payload);
  m_sync_button_state = SyncButtonState::Ignored;
}

// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
void BluetoothRealDevice::FakeSyncButtonHeldEvent(USB::V0IntrMessage& ctrl)
{
  NOTICE_LOG_FMT(IOS_WIIMOTE, "Faking 'sync button held' (0x09) event packet");
  constexpr u8 payload[1] = {0x09};
  FakeSyncButtonEvent(ctrl, payload);
  m_sync_button_state = SyncButtonState::Ignored;
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
