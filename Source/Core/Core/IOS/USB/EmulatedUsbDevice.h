#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"
#include "Core/LibusbUtils.h"

namespace IOS::HLE::USB
{
class EmulatedUsbDevice : public Device
{
public:
  EmulatedUsbDevice(Kernel& ios, const std::string& device_name);
  virtual ~EmulatedUsbDevice();

private:
  Kernel& m_ios;
};
}  // namespace IOS::HLE::USB
