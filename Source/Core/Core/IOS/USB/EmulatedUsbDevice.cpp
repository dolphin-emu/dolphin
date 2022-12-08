#include "Core/IOS/USB/EmulatedUsbDevice.h"

namespace IOS::HLE::USB
{
EmulatedUsbDevice::EmulatedUsbDevice(Kernel& ios, const std::string& device_name) : m_ios(ios)
{
}

EmulatedUsbDevice::~EmulatedUsbDevice() = default;

}  // namespace IOS::HLE::USB
