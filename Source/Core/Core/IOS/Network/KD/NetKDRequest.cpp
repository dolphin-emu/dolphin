// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/NetKDRequest.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "Common/BitUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"

#include "Core/CommonTitles.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/VFF/VFFUtil.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/IOS/Uids.h"
#include "Core/System.h"

namespace IOS::HLE
{
namespace
{
enum class HardwareModel : u8
{
  RVT = 0,
  RVV = 0,
  RVL = 1,
  RVD = 2,
  Unknown = 7
};

u8 GetAreaCode(std::string_view area)
{
  static constexpr std::array<std::pair<std::string_view, u8>, 13> regions{{
      {"JPN", 0},
      {"USA", 1},
      {"EUR", 2},
      {"AUS", 2},
      {"BRA", 1},
      {"TWN", 3},
      {"ROC", 3},
      {"KOR", 4},
      {"HKG", 5},
      {"ASI", 5},
      {"LTN", 1},
      {"SAF", 2},
      {"CHN", 6},
  }};

  const auto entry_pos = std::find_if(regions.cbegin(), regions.cend(),
                                      [&area](const auto& entry) { return entry.first == area; });
  if (entry_pos != regions.end())
    return entry_pos->second;

  return 7;  // Unknown
}

HardwareModel GetHardwareModel(std::string_view model)
{
  static constexpr std::array<std::pair<std::string_view, HardwareModel>, 4> models{{
      {"RVL", HardwareModel::RVL},
      {"RVT", HardwareModel::RVT},
      {"RVV", HardwareModel::RVV},
      {"RVD", HardwareModel::RVD},
  }};

  const auto entry_pos = std::find_if(models.cbegin(), models.cend(),
                                      [&model](const auto& entry) { return entry.first == model; });
  if (entry_pos != models.cend())
    return entry_pos->second;

  return HardwareModel::Unknown;
}

s32 NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, HardwareModel hardware_model,
                    u8 area_code)
{
  static constexpr std::array<u8, 8> table2{
      0x1, 0x5, 0x0, 0x4, 0x2, 0x3, 0x6, 0x7,
  };
  static constexpr std::array<u8, 16> table1{
      0x4, 0xB, 0x7, 0x9, 0xF, 0x1, 0xD, 0x3, 0xC, 0x2, 0x6, 0xE, 0x8, 0x0, 0xA, 0x5,
  };

  constexpr auto u64_get_byte = [](u64 value, u32 shift) -> u8 { return u8(value >> (shift * 8)); };

  constexpr auto u64_insert_byte = [](u64 value, u32 shift, u8 byte) -> u64 {
    const u64 mask = 0x00000000000000FFULL << (shift * 8);
    const u64 inst = u64{byte} << (shift * 8);
    return (value & ~mask) | inst;
  };

  u64 mix_id = (u64{area_code} << 50) | (u64(hardware_model) << 47) | (u64{hollywood_id} << 15) |
               (u64{id_ctr} << 10);
  const u64 mix_id_copy1 = mix_id;

  u32 ctr = 0;
  for (ctr = 0; ctr <= 42; ctr++)
  {
    u64 value = mix_id >> (52 - ctr);
    if ((value & 1) != 0)
    {
      value = 0x0000000000000635ULL << (42 - ctr);
      mix_id ^= value;
    }
  }

  mix_id = (mix_id_copy1 | (mix_id & 0xFFFFFFFFUL)) ^ 0x0000B3B3B3B3B3B3ULL;
  mix_id = (mix_id >> 10) | ((mix_id & 0x3FF) << (11 + 32));

  for (ctr = 0; ctr <= 5; ctr++)
  {
    const u8 ret = u64_get_byte(mix_id, ctr);
    const u8 foobar = u8((u32{table1[(ret >> 4) & 0xF]} << 4) | table1[ret & 0xF]);
    mix_id = u64_insert_byte(mix_id, ctr, foobar & 0xff);
  }

  const u64 mix_id_copy2 = mix_id;

  for (ctr = 0; ctr <= 5; ctr++)
  {
    const u8 ret = u64_get_byte(mix_id_copy2, ctr);
    mix_id = u64_insert_byte(mix_id, table2[ctr], ret);
  }

  mix_id &= 0x001FFFFFFFFFFFFFULL;
  mix_id = (mix_id << 1) | ((mix_id >> 52) & 1);

  mix_id ^= 0x00005E5E5E5E5E5EULL;
  mix_id &= 0x001FFFFFFFFFFFFFULL;

  *nwc24_id = mix_id;

  if (mix_id > 9999999999999999ULL)
    return NWC24::WC24_ERR_FATAL;

  return NWC24::WC24_OK;
}
}  // Anonymous namespace

NetKDRequestDevice::NetKDRequestDevice(Kernel& ios, const std::string& device_name)
    : Device(ios, device_name), config{ios.GetFS()}, m_dl_list{ios.GetFS()}
{
  m_work_queue.Reset([this](AsyncTask task) {
    const IPCReply reply = task.handler();
    {
      std::lock_guard lg(m_async_reply_lock);
      m_async_replies.emplace(AsyncReply{task.request, reply.return_value});
    }
  });
}

NetKDRequestDevice::~NetKDRequestDevice()
{
  WiiSockMan::GetInstance().Clean();
}

void NetKDRequestDevice::Update()
{
  {
    std::lock_guard lg(m_async_reply_lock);
    while (!m_async_replies.empty())
    {
      const auto& reply = m_async_replies.front();
      GetIOS()->EnqueueIPCReply(reply.request, reply.return_value);
      m_async_replies.pop();
    }
  }
}

NWC24::ErrorCode NetKDRequestDevice::KDDownload(const u16 entry_index,
                                                const std::optional<u8> subtask_id)
{
  std::vector<u8> file_data;

  // Content metadata
  const std::string content_name = m_dl_list.GetVFFContentName(entry_index, subtask_id);
  const std::string url = m_dl_list.GetDownloadURL(entry_index, subtask_id);

  INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_DOWNLOAD_NOW_EX - NI - URL: {}", url);

  INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_DOWNLOAD_NOW_EX - NI - Name: {}", content_name);

  const Common::HttpRequest::Response response = m_http.Get(url);

  if (!response)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to request data at {}", url);
    return NWC24::WC24_ERR_SERVER;
  }

  // Check if the filesize is smaller than the header size.
  if (response->size() < sizeof(NWC24::WC24File))
  {
    ERROR_LOG_FMT(IOS_WC24, "File at {} is too small to be a valid file.", url);
    return NWC24::WC24_ERR_BROKEN;
  }

  // Now we read the file
  NWC24::WC24File wc24File;
  std::memcpy(&wc24File, response->data(), sizeof(NWC24::WC24File));

  std::vector<u8> temp_buffer(response->begin() + 320, response->end());

  if (m_dl_list.IsEncrypted(entry_index))
  {
    NWC24::WC24PubkMod pubkMod = m_dl_list.GetWC24PubkMod(entry_index);

    file_data = std::vector<u8>(response->size() - 320);

    Common::AES::CryptOFB(pubkMod.aes_key, wc24File.iv, wc24File.iv, temp_buffer.data(),
                          file_data.data(), temp_buffer.size());
  }
  else
  {
    file_data = std::move(temp_buffer);
  }

  NWC24::ErrorCode reply = IOS::HLE::NWC24::OpenVFF(m_dl_list.GetVFFPath(entry_index), content_name,
                                                    m_ios.GetFS(), file_data);

  return reply;
}

IPCReply NetKDRequestDevice::HandleNWC24DownloadNowEx(const IOCtlRequest& request)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  const u32 flags = memory.Read_U32(request.buffer_in);
  // Nintendo converts the entry ID between a u32 and u16
  // several times, presumably for alignment purposes.
  // We'll skip past buffer_in+4 and keep the entry index as a u16.
  const u16 entry_index = memory.Read_U16(request.buffer_in + 6);
  const u32 subtask_bitmask = memory.Read_U32(request.buffer_in + 8);

  INFO_LOG_FMT(IOS_WC24,
               "NET_KD_REQ: IOCTL_NWC24_DOWNLOAD_NOW_EX - NI - flags: {}, index: {}, bitmask: {}",
               flags, entry_index, subtask_bitmask);

  if (entry_index >= NWC24::NWC24Dl::MAX_ENTRIES)
  {
    ERROR_LOG_FMT(IOS_WC24, "NET_KD_REQ: Entry index out of range.");
    WriteReturnValue(NWC24::WC24_ERR_BROKEN, request.buffer_out);
    return IPCReply(NWC24::WC24_ERR_BROKEN);
  }

  if (!m_dl_list.DoesEntryExist(entry_index))
  {
    ERROR_LOG_FMT(IOS_WC24, "NET_KD_REQ: Requested entry does not exist in download list!");
    WriteReturnValue(NWC24::WC24_ERR_NOT_FOUND, request.buffer_out);
    return IPCReply(NWC24::WC24_ERR_NOT_FOUND);
  }

  // While in theory reply will always get initialized by KDDownload, things happen.
  // Returning NWC24::WC24_ERR_BROKEN or anything that isn't OK will prompt the channel to fix the
  // entry's data.
  NWC24::ErrorCode reply = NWC24::WC24_ERR_BROKEN;

  // Determine if we have subtasks to handle
  if (Common::ExtractBit(flags, 2))
  {
    for (u8 subtask_id = 0; subtask_id < 32; subtask_id++)
    {
      if (Common::ExtractBit(subtask_bitmask, subtask_id))
      {
        reply = KDDownload(entry_index, subtask_id);
        if (reply != NWC24::WC24_OK)
        {
          // An error has occurred, break out and return error.
          break;
        }
      }
    }
  }
  else
  {
    reply = KDDownload(entry_index, std::nullopt);
  }

  WriteReturnValue(reply, request.buffer_out);
  return IPCReply(reply);
}

std::optional<IPCReply> NetKDRequestDevice::IOCtl(const IOCtlRequest& request)
{
  enum : u32
  {
    IOCTL_NWC24_SUSPEND_SCHEDULER = 0x01,
    IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULER = 0x02,
    IOCTL_NWC24_EXEC_RESUME_SCHEDULER = 0x03,
    IOCTL_NWC24_KD_GET_TIME_TRIGGERS = 0x04,
    IOCTL_NWC24_SET_SCHEDULE_SPAN = 0x05,
    IOCTL_NWC24_STARTUP_SOCKET = 0x06,
    IOCTL_NWC24_CLEANUP_SOCKET = 0x07,
    IOCTL_NWC24_LOCK_SOCKET = 0x08,
    IOCTL_NWC24_UNLOCK_SOCKET = 0x09,
    IOCTL_NWC24_CHECK_MAIL_NOW = 0x0A,
    IOCTL_NWC24_SEND_MAIL_NOW = 0x0B,
    IOCTL_NWC24_RECEIVE_MAIL_NOW = 0x0C,
    IOCTL_NWC24_SAVE_MAIL_NOW = 0x0D,
    IOCTL_NWC24_DOWNLOAD_NOW_EX = 0x0E,
    IOCTL_NWC24_REQUEST_GENERATED_USER_ID = 0x0F,
    IOCTL_NWC24_REQUEST_REGISTER_USER_ID = 0x10,
    IOCTL_NWC24_GET_SCHEDULER_STAT = 0x1E,
    IOCTL_NWC24_SET_FILTER_MODE = 0x1F,
    IOCTL_NWC24_SET_DEBUG_MODE = 0x20,
    IOCTL_NWC24_KD_SET_NEXT_WAKEUP = 0x21,
    IOCTL_NWC24_SET_SCRIPT_MODE = 0x22,
    IOCTL_NWC24_REQUEST_SHUTDOWN = 0x28,
  };

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  s32 return_value = 0;
  switch (request.request)
  {
  case IOCTL_NWC24_SUSPEND_SCHEDULER:
    // NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes)
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULER - NI");
    WriteReturnValue(0, request.buffer_out);  // no error
    break;

  case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULER:  // NWC24iResumeForCloseLib
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULER - NI");
    break;

  case IOCTL_NWC24_EXEC_RESUME_SCHEDULER:  // NWC24iResumeForCloseLib
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULER - NI");
    WriteReturnValue(0, request.buffer_out);  // no error
    break;

  case IOCTL_NWC24_STARTUP_SOCKET:  // NWC24iStartupSocket
    WriteReturnValue(0, request.buffer_out);
    memory.Write_U32(0, request.buffer_out + 4);
    return_value = 0;
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
    break;

  case IOCTL_NWC24_CLEANUP_SOCKET:
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET");
    WiiSockMan::GetInstance().Clean();
    break;

  case IOCTL_NWC24_LOCK_SOCKET:  // WiiMenu
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
    break;

  case IOCTL_NWC24_UNLOCK_SOCKET:
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
    break;

  case IOCTL_NWC24_REQUEST_REGISTER_USER_ID:
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_REGISTER_USER_ID");
    WriteReturnValue(0, request.buffer_out);
    memory.Write_U32(0, request.buffer_out + 4);
    break;

  case IOCTL_NWC24_REQUEST_GENERATED_USER_ID:  // (Input: none, Output: 32 bytes)
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
    if (config.IsCreated())
    {
      const std::string settings_file_path =
          Common::GetTitleDataPath(Titles::SYSTEM_MENU) + "/" WII_SETTING;
      std::string area, model;

      const auto fs = m_ios.GetFS();
      if (const auto file = fs->OpenFile(PID_KD, PID_KD, settings_file_path, FS::Mode::Read))
      {
        Common::SettingsHandler::Buffer data;
        if (file->Read(data.data(), data.size()))
        {
          const Common::SettingsHandler gen{std::move(data)};
          area = gen.GetValue("AREA");
          model = gen.GetValue("MODEL");
        }
      }

      if (!area.empty() && !model.empty())
      {
        const u8 area_code = GetAreaCode(area);
        const u8 id_ctr = u8(config.IdGen());
        const HardwareModel hardware_model = GetHardwareModel(model);

        const u32 hollywood_id = m_ios.GetIOSC().GetDeviceId();
        u64 user_id = 0;

        const s32 ret = NWC24MakeUserID(&user_id, hollywood_id, id_ctr, hardware_model, area_code);
        config.SetId(user_id);
        config.IncrementIdGen();
        config.SetCreationStage(NWC24::NWC24CreationStage::Generated);
        config.WriteConfig();

        WriteReturnValue(ret, request.buffer_out);
      }
      else
      {
        WriteReturnValue(NWC24::WC24_ERR_FATAL, request.buffer_out);
      }
    }
    else if (config.IsGenerated())
    {
      WriteReturnValue(NWC24::WC24_ERR_ID_GENERATED, request.buffer_out);
    }
    else if (config.IsRegistered())
    {
      WriteReturnValue(NWC24::WC24_ERR_ID_REGISTERED, request.buffer_out);
    }
    memory.Write_U64(config.Id(), request.buffer_out + 4);
    memory.Write_U32(u32(config.CreationStage()), request.buffer_out + 0xC);
    break;

  case IOCTL_NWC24_GET_SCHEDULER_STAT:
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULER_STAT - NI");
    break;

  case IOCTL_NWC24_SAVE_MAIL_NOW:
    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
    break;

  case IOCTL_NWC24_DOWNLOAD_NOW_EX:
    return LaunchAsyncTask(&NetKDRequestDevice::HandleNWC24DownloadNowEx, request);

  case IOCTL_NWC24_REQUEST_SHUTDOWN:
  {
    if (request.buffer_in == 0 || request.buffer_in % 4 != 0 || request.buffer_in_size < 8 ||
        request.buffer_out == 0 || request.buffer_out % 4 != 0 || request.buffer_out_size < 4)
    {
      return_value = IPC_EINVAL;
      ERROR_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN = IPC_EINVAL");
      break;
    }

    INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN");
    [[maybe_unused]] const u32 event = memory.Read_U32(request.buffer_in);
    // TODO: Advertise shutdown event
    // TODO: Shutdown USB keyboard LEDs if event == 3
    // TODO: IOCTLV_NCD_SETCONFIG
    // TODO: DHCP related features
    // SOGetInterfaceOpt(0xfffe,0x4003);  // IP settings
    // SOGetInterfaceOpt(0xfffe,0xc001);  // DHCP lease time remaining?
    // SOGetInterfaceOpt(0xfffe,0x1003);  // Error
    // Call /dev/net/ip/top 0x1b (SOCleanup), it closes all sockets
    WiiSockMan::GetInstance().Clean();
    return_value = IPC_SUCCESS;
    break;
  }

  default:
    request.Log(GetDeviceName(), Common::Log::LogType::IOS_WC24);
  }

  return IPCReply(return_value);
}
}  // namespace IOS::HLE
