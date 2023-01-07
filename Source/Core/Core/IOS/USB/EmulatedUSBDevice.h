#pragma once

#include <string>
#include <thread>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"

namespace IOS::HLE::USB
{
class EmulatedUSBDevice : public Device
{
public:
  EmulatedUSBDevice(Kernel& ios, const std::string& device_name);
  virtual ~EmulatedUSBDevice();

protected:
  class FakeTransferThread final
  {
  public:
    explicit FakeTransferThread(EmulatedUSBDevice* device) : m_device(device) {}
    ~FakeTransferThread();
    void Start();
    void Stop();
    void AddTransfer(std::unique_ptr<TransferCommand> command, std::array<u8, 64> data);
    void ClearTransfers();
    bool GetTransfers();

  private:
    EmulatedUSBDevice* m_device = nullptr;
    Common::Flag m_thread_running;
    std::thread m_thread;
    Common::Flag m_is_initialized;
    std::map<std::array<u8, 64>, std::unique_ptr<TransferCommand>> m_transfers;
    std::mutex m_transfers_mutex;
  };
  FakeTransferThread m_transfer_thread{this};
  FakeTransferThread& GetTransferThread() { return m_transfer_thread; }

private:
  Kernel& m_ios;
};
}  // namespace IOS::HLE::USB
