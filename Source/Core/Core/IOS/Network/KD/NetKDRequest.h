// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <queue>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"
#include "Core/IOS/Network/KD/Mail/WC24Send.h"
#include "Core/IOS/Network/KD/NWC24Config.h"
#include "Core/IOS/Network/KD/NWC24DL.h"
#include "Core/IOS/Network/KD/NetKDTime.h"

namespace IOS::HLE
{
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class NetKDRequestDevice : public EmulationDevice
{
public:
  NetKDRequestDevice(EmulationKernel& ios, const std::string& device_name,
                     const std::shared_ptr<NetKDTimeDevice>& time_device);
  IPCReply HandleNWC24DownloadNowEx(const IOCtlRequest& request);
  NWC24::ErrorCode KDDownload(const u16 entry_index, const std::optional<u8> subtask_id);
  IPCReply HandleNWC24CheckMailNow(const IOCtlRequest& request);
  ~NetKDRequestDevice() override;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  void Update() override;

private:
  struct AsyncTask
  {
    IOS::HLE::Request request;
    std::function<IPCReply()> handler;
  };

  struct AsyncReply
  {
    IOS::HLE::Request request;
    s32 return_value;
  };

  template <typename Method, typename Request>
  std::optional<IPCReply> LaunchAsyncTask(Method method, const Request& request)
  {
    m_work_queue.EmplaceItem(AsyncTask{request, std::bind(method, this, request)});
    return std::nullopt;
  }

  enum class CurrentFunction : u32
  {
    None = 0,
    Account = 1,
    Check = 2,
    Receive = 3,
    Send = 5,
    Save = 6,
    Download = 7,
  };

  enum class ErrorType
  {
    Account,
    KD_Download,
    Client,
    Server,
    CheckMail,
    SendMail,
    ReceiveMail,
    CGI,
  };

  enum class SchedulerEvent
  {
    Mail,
    Download,
  };

  IPCReply HandleNWC24SendMailNow(const IOCtlRequest& request);
  NWC24::ErrorCode KDCheckMail(u32* mail_flag, u32* interval);
  IPCReply HandleRequestRegisterUserId(const IOCtlRequest& request);
  NWC24::ErrorCode KDSendMail();

  void LogError(ErrorType error_type, s32 error_code);
  void SchedulerTimer();
  void SchedulerWorker(SchedulerEvent event);
  NWC24::ErrorCode DetermineDownloadTask(u16* entry_index, std::optional<u8>* subtask_id) const;
  NWC24::ErrorCode DetermineSubtask(u16 entry_index, std::optional<u8>* subtask_id) const;

  static constexpr u32 MAX_MAIL_SIZE = 208952;
  static std::string GetValueFromCGIResponse(const std::string& response, const std::string& key);
  static constexpr std::array<u8, 20> MAIL_CHECK_KEY = {0xce, 0x4c, 0xf2, 0x9a, 0x3d, 0x6b, 0xe1,
                                                        0xc2, 0x61, 0x91, 0x72, 0xb5, 0xcb, 0x29,
                                                        0x8c, 0x89, 0x72, 0xd4, 0x50, 0xad};

  static constexpr u32 DEFAULT_SCHEDULER_SPAN_MINUTES = 11;

  NWC24::NWC24Config m_config;
  NWC24::NWC24Dl m_dl_list;
  NWC24::Mail::WC24SendList m_send_list;
  NWC24::Mail::WC24FriendList m_friend_list;
  Common::WorkQueueThread<AsyncTask> m_work_queue;
  Common::WorkQueueThread<std::function<void()>> m_scheduler_work_queue;
  std::mutex m_async_reply_lock;
  std::mutex m_scheduler_buffer_lock;
  std::queue<AsyncReply> m_async_replies;
  u32 m_error_count = 0;
  std::array<u32, 256> m_scheduler_buffer{};
  std::shared_ptr<NetKDTimeDevice> m_time_device;
  // TODO: Maybe move away from Common::HttpRequest?
  Common::HttpRequest m_http{std::chrono::minutes{1}};
  u32 m_download_span = 2;
  u32 m_mail_span = 1;
  bool m_handle_mail;
  Common::Event m_shutdown_event;
  std::mutex m_scheduler_lock;
  std::thread m_scheduler_timer_thread;
};
}  // namespace IOS::HLE
