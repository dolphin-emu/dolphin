// SPDX-License-Identifier: GPL-2.0-or-later

#include "DSUInput.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace
{
std::atomic<bool> s_background_execution_allowed{true};
std::thread s_background_thread;
std::mutex s_background_thread_mutex;
std::atomic<bool> s_background_thread_should_stop{false};
}

static void BackgroundThreadMain()
{
  while (!s_background_thread_should_stop.load())
  {
    g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::Host);
    g_controller_interface.UpdateInput();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}


void SetBackgroundInputExecutionAllowed(bool allowed)
{
  s_background_execution_allowed.store(allowed);

  if (allowed)
  {
    std::lock_guard<std::mutex> lock(s_background_thread_mutex);
    if (!s_background_thread.joinable())
    {
      s_background_thread_should_stop.store(false);
      s_background_thread = std::thread(BackgroundThreadMain);
    }
  }
  else
  {
    std::lock_guard<std::mutex> lock(s_background_thread_mutex);
    if (s_background_thread.joinable())
    {
      s_background_thread_should_stop.store(true);
      s_background_thread.join();
    }
  }
}
