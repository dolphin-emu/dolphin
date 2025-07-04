// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Logic is heavily copied from Linux source:
// drivers/bluetooth/btusb.c
// drivers/bluetooth/btrtl.c
// drivers/bluetooth/btrtl.h

#include "Core/IOS/USB/Bluetooth/RealtekFirmwareLoader.h"

#include <algorithm>

#include "Common/BitUtils.h"
#include "Common/FileUtil.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/IOS/USB/Bluetooth/LibUSBBluetoothAdapter.h"

namespace
{
constexpr u16 RTL_ROM_LMP_8723A = 0x1200;
constexpr u16 RTL_ROM_LMP_8723B = 0x8723;
constexpr u16 RTL_ROM_LMP_8821A = 0x8821;
constexpr u16 RTL_ROM_LMP_8761A = 0x8761;
constexpr u16 RTL_ROM_LMP_8822B = 0x8822;
constexpr u16 RTL_ROM_LMP_8852A = 0x8852;
constexpr u16 RTL_ROM_LMP_8851B = 0x8851;
constexpr u16 RTL_ROM_LMP_8922A = 0x8922;

constexpr std::string_view RTL_EPATCH_SIGNATURE = "Realtech";
constexpr std::string_view RTL_EPATCH_SIGNATURE_V2 = "RTBTCore";

#pragma pack(push, 1)
struct rtl_vendor_cmd
{
  std::array<u8, 5> data;
};
static_assert(sizeof(rtl_vendor_cmd) == 5);
struct rtl_epatch_header
{
  std::array<u8, 8> signature;
  u32 fw_version;
  u16 num_patches;
};
static_assert(sizeof(rtl_epatch_header) == 8 + 4 + 2);
struct rtl_download_cmd
{
  static constexpr int DATA_LEN = 252;

  u8 index;
  std::array<u8, DATA_LEN> data;
};
static_assert(sizeof(rtl_download_cmd) == 253);
struct rtl_download_response
{
  u8 status;
  u8 index;
};
static_assert(sizeof(rtl_download_response) == 2);
struct rtl_rom_version_evt
{
  u8 status;
  u8 version;
};
static_assert(sizeof(rtl_rom_version_evt) == 2);
#pragma pack(pop)
}  // namespace

bool IsRealtekVID(u16 vid)
{
  return vid == 0x0bda;
}

bool IsKnownRealtekBluetoothDevice(u16 vid, u16 pid)
{
  // This list comes from Linux source: drivers/bluetooth/btusb.c
  constexpr std::pair<u16, u16> realtek_ids[] = {
      // Realtek 8821CE Bluetooth devices
      {0x13d3, 0x3529},
      // Realtek 8822CE Bluetooth devices
      {0x0bda, 0xb00c},
      {0x0bda, 0xc822},
      // Realtek 8822CU Bluetooth devices
      {0x13d3, 0x3549},
      // Realtek 8852AE Bluetooth devices
      {0x0bda, 0x2852},
      {0x0bda, 0xc852},
      {0x0bda, 0x385a},
      {0x0bda, 0x4852},
      {0x04c5, 0x165c},
      {0x04ca, 0x4006},
      {0x0cb8, 0xc549},
      // Realtek 8852CE Bluetooth devices
      {0x04ca, 0x4007},
      {0x04c5, 0x1675},
      {0x0cb8, 0xc558},
      {0x13d3, 0x3587},
      {0x13d3, 0x3586},
      {0x13d3, 0x3592},
      {0x0489, 0xe122},
      // Realtek 8852BE Bluetooth devices
      {0x0cb8, 0xc559},
      {0x0bda, 0x4853},
      {0x0bda, 0x887b},
      {0x0bda, 0xb85b},
      {0x13d3, 0x3570},
      {0x13d3, 0x3571},
      {0x13d3, 0x3572},
      {0x13d3, 0x3591},
      {0x0489, 0xe125},
      // Realtek 8852BT/8852BE-VT Bluetooth devices
      {0x0bda, 0x8520},
      // Realtek 8922AE Bluetooth devices
      {0x0bda, 0x8922},
      {0x13d3, 0x3617},
      {0x13d3, 0x3616},
      {0x0489, 0xe130},
      // Additional Realtek 8723AE Bluetooth devices
      {0x0930, 0x021d},
      {0x13d3, 0x3394},
      // Additional Realtek 8723BE Bluetooth devices
      {0x0489, 0xe085},
      {0x0489, 0xe08b},
      {0x04f2, 0xb49f},
      {0x13d3, 0x3410},
      {0x13d3, 0x3416},
      {0x13d3, 0x3459},
      {0x13d3, 0x3494},
      // Additional Realtek 8723BU Bluetooth devices
      {0x7392, 0xa611},
      // Additional Realtek 8723DE Bluetooth devices
      {0x0bda, 0xb009},
      {0x2ff8, 0xb011},
      // Additional Realtek 8761BUV Bluetooth devices
      {0x2357, 0x0604},
      {0x0b05, 0x190e},
      {0x2550, 0x8761},
      {0x0bda, 0x8771},
      {0x6655, 0x8771},
      {0x7392, 0xc611},
      {0x2b89, 0x8761},
      // Additional Realtek 8821AE Bluetooth devices
      {0x0b05, 0x17dc},
      {0x13d3, 0x3414},
      {0x13d3, 0x3458},
      {0x13d3, 0x3461},
      {0x13d3, 0x3462},
      // Additional Realtek 8822BE Bluetooth devices
      {0x13d3, 0x3526},
      {0x0b05, 0x185c},
      // Additional Realtek 8822CE Bluetooth devices
      {0x04ca, 0x4005},
      {0x04c5, 0x161f},
      {0x0b05, 0x18ef},
      {0x13d3, 0x3548},
      {0x13d3, 0x3549},
      {0x13d3, 0x3553},
      {0x13d3, 0x3555},
      {0x2ff8, 0x3051},
      {0x1358, 0xc123},
      {0x0bda, 0xc123},
      {0x0cb5, 0xc547},
  };

  return std::ranges::find(realtek_ids, std::pair(vid, pid)) != std::end(realtek_ids);
}

// Returns the data to be loaded, or an empty buffer on error.
static Common::UniqueBuffer<char> ParseFirmware(std::span<const char> fw_data,
                                                std::span<const char> cfg_data,
                                                const hci_read_local_ver_rp& local_ver,
                                                u8 rom_version)
{
  if (fw_data.size() < sizeof(rtl_epatch_header))
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Undersized firmware file");
    return {};
  }

  const rtl_epatch_header epatch_info = Common::BitCastPtr<rtl_epatch_header>(fw_data.data());
  if (std::memcmp(epatch_info.signature.data(), RTL_EPATCH_SIGNATURE.data(),
                  epatch_info.signature.size()) != 0)
  {
    if (std::memcmp(epatch_info.signature.data(), RTL_EPATCH_SIGNATURE_V2.data(),
                    epatch_info.signature.size()) == 0)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "EPATCH v2 not implemented. Please report device to developers.");
    }
    else
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Bad EPATCH signature");
    }
    return {};
  }

  const auto fw_version = epatch_info.fw_version;
  const auto num_patches = epatch_info.num_patches;

  INFO_LOG_FMT(IOS_WIIMOTE, "fw_version={} num_patches={}", epatch_info.fw_version, num_patches);

  static constexpr u8 EXTENSION_SIGNATURE[] = {0x51, 0x04, 0xfd, 0x77};
  const auto extension = fw_data.last(std::size(EXTENSION_SIGNATURE));
  if (std::memcmp(extension.data(), EXTENSION_SIGNATURE, extension.size()) != 0)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Bad extension section signature");
    return {};
  }

  // Look for the project ID starting from the end of the file.
  int project_id = -1;
  for (auto iter = extension.begin(); iter >= fw_data.begin() + (sizeof(rtl_epatch_header) + 3);)
  {
    const u8 opcode = *--iter;
    const u8 length = *--iter;
    const u8 data = *--iter;

    // EOF
    if (opcode == 0xff)
      break;

    if (length == 0)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Found instruction with length 0");
      return {};
    }

    if (opcode == 0 && length == 1)
    {
      project_id = data;
      break;
    }

    iter -= length;
  }

  struct LMPSubverProjectID
  {
    u16 lmp_subver;
    u8 project_id;
  };

  constexpr LMPSubverProjectID subver_project_ids[] = {
      {RTL_ROM_LMP_8723B, 1},  {RTL_ROM_LMP_8821A, 2},  {RTL_ROM_LMP_8761A, 3},
      {RTL_ROM_LMP_8822B, 8},  {RTL_ROM_LMP_8723B, 9},  {RTL_ROM_LMP_8821A, 10},
      {RTL_ROM_LMP_8822B, 13}, {RTL_ROM_LMP_8761A, 14}, {RTL_ROM_LMP_8852A, 18},
      {RTL_ROM_LMP_8852A, 20}, {RTL_ROM_LMP_8852A, 25}, {RTL_ROM_LMP_8851B, 36},
      {RTL_ROM_LMP_8922A, 44}, {RTL_ROM_LMP_8852A, 47},
  };

  // Verify that the project ID is what we expect for our LMP subversion.
  const auto* const it =
      std::ranges::find(subver_project_ids, project_id, &LMPSubverProjectID::project_id);
  if (it == std::end(subver_project_ids) || it->lmp_subver != local_ver.lmp_subversion)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Unexpected project_id: {}", project_id);
    return {};
  }

  // Structure is:
  // u16 chip_ids[num_patches]; u16 patch_lengths[num_patches]; u32 patch_offsets[num_patches];
  if (fw_data.size() <
      sizeof(rtl_epatch_header) + num_patches * (sizeof(u16) + sizeof(u16) + sizeof(u32)))
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Undersized firmware file");
    return {};
  }
  const auto* chip_id_base = fw_data.data() + sizeof(rtl_epatch_header);
  const auto* const patch_length_base = chip_id_base + (sizeof(u16) * num_patches);
  const auto* const patch_offset_base = patch_length_base + (sizeof(u16) * num_patches);
  for (int i = 0; i != num_patches; ++i)
  {
    const u16 chip_id = Common::BitCastPtr<u16>(chip_id_base + (i * sizeof(u16)));
    if (chip_id == rom_version + 1)
    {
      const u16 patch_length = Common::BitCastPtr<u16>(patch_length_base + (i * sizeof(u16)));
      const u32 patch_offset = Common::BitCastPtr<u32>(patch_offset_base + (i * sizeof(u32)));

      INFO_LOG_FMT(IOS_WIIMOTE, "Patch length:{} offset:{}", patch_length, patch_offset);

      if (patch_offset + patch_length > fw_data.size())
      {
        ERROR_LOG_FMT(IOS_WIIMOTE, "Undersized firmware file");
        return {};
      }

      auto result = Common::UniqueBuffer<char>{patch_length + cfg_data.size()};
      std::copy_n(fw_data.data() + patch_offset, patch_length - sizeof(fw_version), result.data());

      // The last 4 bytes are replaced with fw_version.
      std::memcpy(result.data() + patch_length - sizeof(fw_version), &fw_version,
                  sizeof(fw_version));

      // Append the config binary.
      std::ranges::copy(cfg_data, result.data() + patch_length);

      return result;
    }
  }

  ERROR_LOG_FMT(IOS_WIIMOTE, "Patch not found");
  return {};
}

// Blocks and eats events until a command complete event of the correct opcode is received.
// Returns a result of the specified type or nullopt on error/timeout.
template <typename ResultType>
static std::optional<ResultType> SendBlockingCommand(LibUSBBluetoothAdapter& adapter,
                                                     std::span<const u8> data)
{
  adapter.SendControlTransfer(data);

  const hci_cmd_hdr_t cmd = Common::BitCastPtr<hci_cmd_hdr_t>(data.data());

  while (!adapter.AreCommandsComplete())
  {
    auto event = adapter.ReceiveHCIEvent();

    if (event.empty())
    {
      Common::YieldCPU();
      continue;
    }

    if (event[0] != HCI_EVENT_COMMAND_COMPL)
      continue;

    if (event.size() < sizeof(hci_event_hdr_t) + sizeof(hci_command_compl_ep))
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Undersized hci_command_compl_ep");
      continue;
    }

    const hci_command_compl_ep compl_event =
        Common::BitCastPtr<hci_command_compl_ep>(event.data() + sizeof(hci_event_hdr_t));
    if (compl_event.opcode != cmd.opcode)
      continue;

    if (event.size() < sizeof(hci_event_hdr_t) + sizeof(hci_command_compl_ep) + sizeof(ResultType))
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Undersized command result");
      break;
    }

    return ResultType(Common::BitCastPtr<ResultType>(event.data() + sizeof(hci_event_hdr_t) +
                                                     sizeof(hci_command_compl_ep)));
  }

  return std::nullopt;
}

// Loads the provided data on the adapter.
static void LoadFirmware(LibUSBBluetoothAdapter& adapter, std::span<const char> data)
{
  if (data.empty())
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Firmware data is empty");
    return;
  }

  INFO_LOG_FMT(IOS_WIIMOTE, "Loading firmware, total size: {}", data.size());

  HCICommandPayload<0xfc20, rtl_download_cmd> payload{};

  const auto is_result_bad = [&](const auto& result) {
    if (!result)
      return true;

    if (result->status != 0x00)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Bad load firmware status: 0x{:02x}", result->status);
      return true;
    }

    return false;
  };

  auto iter = data.begin();
  while (data.end() - iter > rtl_download_cmd::DATA_LEN)
  {
    std::ranges::copy_n(iter, rtl_download_cmd::DATA_LEN, payload.command.data.data());

    if (is_result_bad(SendBlockingCommand<rtl_download_response>(adapter, AsU8Span(payload))))
      return;

    if (++payload.command.index == 0x80)
      payload.command.index = 1;

    iter += rtl_download_cmd::DATA_LEN;
  }

  // Mark data end.
  payload.command.index |= 0x80u;
  payload.header.length = u8(sizeof(payload.command.index) + (data.end() - iter));
  std::ranges::copy(iter, data.end(), payload.command.data.data());

  const auto span = AsU8Span(payload).first(sizeof(payload.header) + payload.header.length);
  if (is_result_bad(SendBlockingCommand<rtl_download_response>(adapter, span)))
    return;

  INFO_LOG_FMT(IOS_WIIMOTE, "Loading firmware complete");
}

static constexpr u64 LocalVerUID(u16 lmp_subversion, u16 hci_revision, u8 hci_version)
{
  return u64(lmp_subversion) | (u64(hci_revision) << 16u) | (u64(hci_version) << 32u);
}

// Returns the firmware name for a given local version.
static std::string GetFirmwareName(const hci_read_local_ver_rp& local_ver)
{
  // This list comes from Linux source: drivers/bluetooth/btrtl.c
  switch (LocalVerUID(local_ver.lmp_subversion, local_ver.hci_revision, local_ver.hci_version))
  {
  case LocalVerUID(RTL_ROM_LMP_8723A, 0xb, 0x6):
    return "8723a";
  case LocalVerUID(RTL_ROM_LMP_8723B, 0xb, 0x6):
    return "8723b";
  case LocalVerUID(RTL_ROM_LMP_8723B, 0xd, 0x8):
    return "8723d";
  case LocalVerUID(RTL_ROM_LMP_8761A, 0xa, 0x6):
    return "8761a";
  case LocalVerUID(RTL_ROM_LMP_8761A, 0xb, 0xa):
    return "8761bu";
  case LocalVerUID(RTL_ROM_LMP_8821A, 0xa, 0x6):
    return "8821a";
  case LocalVerUID(RTL_ROM_LMP_8821A, 0xc, 0x8):
    return "8821c";
  case LocalVerUID(RTL_ROM_LMP_8822B, 0xc, 0xa):
    return "8822cu";
  case LocalVerUID(RTL_ROM_LMP_8822B, 0xb, 0x7):
    return "8822b";
  case LocalVerUID(RTL_ROM_LMP_8851B, 0xb, 0xc):
    return "8851bu";
  case LocalVerUID(RTL_ROM_LMP_8852A, 0xa, 0xb):
    return "8852au";
  case LocalVerUID(RTL_ROM_LMP_8852A, 0xb, 0xb):
    return "8852bu";
  case LocalVerUID(RTL_ROM_LMP_8852A, 0xc, 0xc):
    return "8852cu";
  case LocalVerUID(RTL_ROM_LMP_8852A, 0x87, 0xc):
    return "8852btu";
  case LocalVerUID(RTL_ROM_LMP_8922A, 0xa, 0xc):
    return "8922au";
  default:
    return {};
  }
}

void InitializeRealtekBluetoothDevice(LibUSBBluetoothAdapter& adapter)
{
  // Read local version.
  const auto local_ver = SendBlockingCommand<hci_read_local_ver_rp>(
      adapter, AsU8Span(hci_cmd_hdr_t{HCI_CMD_READ_LOCAL_VER, 0}));
  if (!local_ver)
    return;
  if (local_ver->status != 0x00)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Bad local version status: 0x{:02x}", local_ver->status);
    return;
  }

  const auto firmware_name = GetFirmwareName(*local_ver);
  // If firmware has already been loaded then the subversion changes and we won't find a match.
  if (firmware_name.empty())
  {
    INFO_LOG_FMT(IOS_WIIMOTE, "Assuming no firmware load is needed");
    return;
  }

  INFO_LOG_FMT(IOS_WIIMOTE, "Loading firmware: {}", firmware_name);

  // A name like "8761bu" is turned into "rtl8761bu_fw.bin"
  const auto firmware_base_path = File::GetSysDirectory() + "Firmware/rtl_bt/rtl" + firmware_name;

  std::string fw_data;
  File::ReadFileToString(firmware_base_path + "_fw.bin", fw_data);

  // 8723A is old and the binary is loaded as-is without "parsing".
  if (local_ver->lmp_subversion == RTL_ROM_LMP_8723A)
  {
    // File should *not* have the epatch signature.
    if (fw_data.starts_with(RTL_EPATCH_SIGNATURE))
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Unexpected EPATCH signature");
      return;
    }

    LoadFirmware(adapter, fw_data);
    return;
  }

  // Read ROM version.
  const auto read_rom_ver =
      SendBlockingCommand<rtl_rom_version_evt>(adapter, AsU8Span(hci_cmd_hdr_t{0xfc6d, 0}));
  if (!read_rom_ver)
    return;
  if (read_rom_ver->status != 0x00)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Bad ROM version status: 0x{:02x}", read_rom_ver->status);
    return;
  }

  const u8 rom_version = read_rom_ver->version;
  INFO_LOG_FMT(IOS_WIIMOTE, "ROM version: 0x{:02x}", rom_version);

  std::string cfg_data;
  File::ReadFileToString(firmware_base_path + "_config.bin", cfg_data);

  const auto firmware = ParseFirmware(fw_data, cfg_data, *local_ver, rom_version);
  LoadFirmware(adapter, firmware);
}
