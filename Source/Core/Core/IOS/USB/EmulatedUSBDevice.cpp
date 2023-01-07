#include "Core/IOS/USB/EmulatedUSBDevice.h"

#include <thread>

#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/Core.h"

namespace IOS::HLE::USB
{
EmulatedUSBDevice::EmulatedUSBDevice(Kernel& ios, const std::string& device_name) : m_ios(ios)
{
}

EmulatedUSBDevice::~EmulatedUSBDevice()
{
}

EmulatedUSBDevice::FakeTransferThread::~FakeTransferThread()
{
  Stop();
}

void EmulatedUSBDevice::FakeTransferThread::Start()
{
  if (Core::WantsDeterminism())
    return;

  if (m_thread_running.TestAndSet())
  {
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("Fake Transfer Thread");
      while (m_thread_running.IsSet())
      {
        if (!m_transfers.empty())
        {
          std::lock_guard lk{m_transfers_mutex};
          u64 timestamp = Common::Timer::NowUs();
          for (auto iterator = m_transfers.begin(); iterator != m_transfers.end();)
          {
            auto* command = iterator->second.get();
            if (command->expected_time > timestamp)
            {
              ++iterator;
              continue;
            }
            command->FillBuffer(iterator->first.data(), command->expected_count);
            command->OnTransferComplete(command->expected_count);
            iterator = m_transfers.erase(iterator);
          }
        }
      }
    });
  }
}

void EmulatedUSBDevice::FakeTransferThread::Stop()
{
  if (m_thread_running.TestAndClear())
    m_thread.join();
}

void EmulatedUSBDevice::FakeTransferThread::AddTransfer(std::unique_ptr<TransferCommand> command,
                                                        std::array<u8, 64> data)
{
  std::lock_guard lk{m_transfers_mutex};
  m_transfers.emplace(data, std::move(command));
}

void EmulatedUSBDevice::FakeTransferThread::ClearTransfers()
{
  std::lock_guard lk{m_transfers_mutex};
  m_transfers.clear();
}

bool EmulatedUSBDevice::FakeTransferThread::GetTransfers()
{
  std::lock_guard lk{m_transfers_mutex};
  return m_transfers.empty();
}

}  // namespace IOS::HLE::USB