// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include "Common/WorkQueueThread.h"
#endif

struct cubeb;

namespace CubebUtils
{
std::shared_ptr<cubeb> GetContext();
std::vector<std::pair<std::string, std::string>> ListInputDevices();
const void* GetInputDeviceById(std::string_view id);

// Helper used to handle Windows COM library for cubeb WASAPI backend
class CoInitSyncWorker
{
public:
  using FunctionType = std::function<void()>;

  CoInitSyncWorker(std::string_view worker_name);
  ~CoInitSyncWorker();

  bool Execute(FunctionType f);

#ifdef _WIN32
private:
  Common::WorkQueueThread<FunctionType> m_work_queue;
  bool m_coinit_success = false;
  bool m_should_couninit = false;
#endif
};
}  // namespace CubebUtils
