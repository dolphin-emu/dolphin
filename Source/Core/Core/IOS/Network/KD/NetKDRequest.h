// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <queue>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/Network/KD/NWC24Config.h"
#include "Core/IOS/Network/KD/NWC24DL.h"
#include "Core/IOS/Network/KD/WC24Send.h"

namespace IOS::HLE
{
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class NetKDRequestDevice : public EmulationDevice
{
public:
  NetKDRequestDevice(EmulationKernel& ios, const std::string& device_name);
  IPCReply HandleNWC24DownloadNowEx(const IOCtlRequest& request);
  IPCReply HandleNWC24SendMailNow(const IOCtlRequest& request);
  IPCReply HandleNWC24CheckMailNow(const IOCtlRequest& request);
  NWC24::ErrorCode KDDownload(const u16 entry_index, const std::optional<u8> subtask_id);
  NWC24::ErrorCode KDCheckMail(u32* _mail_flag, u32* _interval);
  NWC24::ErrorCode KDSendMail();
  NWC24::ErrorCode KDReceiveMail();
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

  enum class ErrorType
  {
    Account,
    KD_Download,
    Client,
    Server,
    CheckMail,
    SendMail,
    ReceiveMail,
    CGI
  };

  void LogError(ErrorType error_type, s32 error_code);

  void SchedulerTimer()
  {
    u32 mail_time_state = 0;
    u32 download_time_state = 0;
    Common::SetCurrentThreadName("KD Scheduler Timer");
    while (true)
    {
      {
        std::lock_guard lg(m_scheduler_lock);
        if (m_mail_span <= mail_time_state)
        {
          m_scheduler_work_queue.EmplaceItem([this] { SchedulerWorker(SchedulerEvent::Mail); });
          INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: Dispatching Mail Task from Scheduler");
          mail_time_state = 0;
        }

        if (m_download_span <= download_time_state)
        {
          INFO_LOG_FMT(IOS_WC24, "NET_KD_REQ: Dispatching Download Task from Scheduler");
          m_scheduler_work_queue.EmplaceItem([this] { SchedulerWorker(SchedulerEvent::Download); });
          download_time_state = 0;
        }
      }

      if (m_shutdown_event.WaitFor(std::chrono::minutes{1}))
        return;

      mail_time_state++;
      download_time_state++;
    }
  }

  enum class SchedulerEvent
  {
    Mail,
    Download,
  };

  void SchedulerWorker(SchedulerEvent event);

  static std::string GetValueFromCGIResponse(const std::string& response, const std::string& key);

  static constexpr std::array<u8, 20> MAIL_CHECK_KEY = {0xce, 0x4c, 0xf2, 0x9a, 0x3d, 0x6b, 0xe1,
                                                        0xc2, 0x61, 0x91, 0x72, 0xb5, 0xcb, 0x29,
                                                        0x8c, 0x89, 0x72, 0xd4, 0x50, 0xad};

  static constexpr u32 MAX_MAIL_SIZE = 208952;
  static constexpr u32 MAX_MAIL_RECEIVE_SIZE = 1578040;
  static constexpr char TEMP_MAIL_PATH[] = "/" WII_WC24CONF_DIR "/mbox/recvtmp.msg";

  NWC24::NWC24Config m_config;
  NWC24::NWC24Dl m_dl_list;
  NWC24::WC24SendList m_send_list;
  Common::WorkQueueThread<AsyncTask> m_work_queue;
  Common::WorkQueueThread<std::function<void()>> m_scheduler_work_queue;
  std::mutex m_async_reply_lock;
  std::mutex m_scheduler_buffer_lock;
  std::queue<AsyncReply> m_async_replies;
  u32 m_error_count = 0;
  std::array<u32, 256> m_scheduler_buffer{};
  Common::HttpRequest m_http{std::chrono::minutes{1}};
  u32 m_download_span = 2;
  u32 m_mail_span = 1;
  Common::Event m_shutdown_event;
  std::mutex m_scheduler_lock;
  std::thread m_scheduler_timer_thread;
};
}  // namespace IOS::HLE