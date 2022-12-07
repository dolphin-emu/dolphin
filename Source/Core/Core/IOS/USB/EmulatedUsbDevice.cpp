#include "Core/IOS/USB/EmulatedUsbDevice.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <libusb.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE::USB
{
EmulatedUsbDevice::EmulatedUsbDevice(Kernel& ios, const std::string& device_name) : m_ios(ios)
{
}

EmulatedUsbDevice::~EmulatedUsbDevice() = default;

}  // namespace IOS::HLE::USB
