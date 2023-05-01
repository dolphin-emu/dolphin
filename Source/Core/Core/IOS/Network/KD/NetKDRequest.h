// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <queue>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/Network/KD/NWC24Config.h"
#include "Core/IOS/Network/KD/NWC24DL.h"

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
  NWC24::ErrorCode KDDownload(const u16 entry_index, const std::optional<u8> subtask_id);
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
  };

  void LogError(ErrorType error_type, s32 error_code);

  NWC24::NWC24Config config;
  NWC24::NWC24Dl m_dl_list;
  Common::WorkQueueThread<AsyncTask> m_work_queue;
  std::mutex m_async_reply_lock;
  std::mutex m_scheduler_buffer_lock;
  std::queue<AsyncReply> m_async_replies;
  u32 m_error_count = 0;
  std::array<u32, 256> m_scheduler_buffer{};
  // TODO: Maybe move away from Common::HttpRequest?
  Common::HttpRequest m_http{std::chrono::minutes{1}};
};
}  // namespace IOS::HLE
