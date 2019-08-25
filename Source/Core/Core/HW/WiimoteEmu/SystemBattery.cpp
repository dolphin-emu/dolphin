// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/SystemBattery.h"

#include "Common/ScopeGuard.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CFDictionary.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#endif

namespace WiimoteEmu
{

#if defined(_WIN32)

std::optional<float> GetSystemBatteryLevel()
{
  SYSTEM_POWER_STATUS power_status;
  if (!GetSystemPowerStatus(&power_status))
  {
    return std::nullopt;
  }

  // Not on battery power, treat as 100%
  if (power_status.ACLineStatus == 1)
  {
    return 1.0;
  }

  if (power_status.BatteryLifePercent != 255)
  {
    return power_status.BatteryLifePercent / 100.0f;
  }

  return std::nullopt;
}

#elif defined(__APPLE__)

std::optional<float> GetSystemBatteryLevel()
{
  CFTypeRef power_blob = IOPSCopyPowerSourcesInfo();
  if (!power_blob)
  {
    return std::nullopt;
  }
  Common::ScopeGuard power_blob_guard([power_blob] { CFRelease(power_blob); });

  CFArrayRef power_sources = IOPSCopyPowerSourcesList(power_blob);
  if (!power_sources)
  {
    return std::nullopt;
  }
  Common::ScopeGuard power_sources_guard([power_sources] { CFRelease(power_sources); });

  CFStringRef current_power = IOPSGetProvidingPowerSourceType(power_blob);
  // Not on battery power, treat as 100%
  if (kCFCompareEqualTo != CFStringCompare(current_power, CFSTR(kIOPMBatteryPowerKey), 0))
  {
    return 1.0;
  }

  for (CFIndex i = 0; i < CFArrayGetCount(power_sources); i++)
  {
    CFTypeRef power_source = CFArrayGetValueAtIndex(power_sources, i);
    CFDictionaryRef power_source_desc = IOPSGetPowerSourceDescription(power_blob, power_source);
    if (!power_source_desc)
    {
      continue;
    }

    CFNumberRef capacity_current, capacity_max;
    if (!CFDictionaryGetValueIfPresent(power_source_desc, CFSTR(kIOPSCurrentCapacityKey),
                                       (const void**)&capacity_current) ||
        !CFDictionaryGetValueIfPresent(power_source_desc, CFSTR(kIOPSMaxCapacityKey),
                                       (const void**)&capacity_max))
    {
      continue;
    }

    float cur, max;
    if (!CFNumberGetValue(capacity_current, kCFNumberFloat32Type, &cur) ||
        !CFNumberGetValue(capacity_max, kCFNumberFloat32Type, &max))
    {
      continue;
    }

    return cur / max;
  }

  return std::nullopt;
}

#endif

};
