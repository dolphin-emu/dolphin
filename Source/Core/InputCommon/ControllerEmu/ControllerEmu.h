// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/Common.h"
#include "Common/IniFile.h"
#include "InputCommon/ControllerInterface/Device.h"

class ControllerInterface;

const char* const named_directions[] = {_trans("Up"), _trans("Down"), _trans("Left"),
                                        _trans("Right")};

namespace ControllerEmu
{
class ControlGroup;

class EmulatedController
{
public:
  virtual ~EmulatedController();

  virtual std::string GetName() const = 0;
  virtual std::string GetDisplayName() const;

  virtual void LoadDefaults(const ControllerInterface& ciface);

  virtual void LoadConfig(IniFile::Section* sec, const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& base = "");

  bool IsDefaultDeviceConnected() const;
  const ciface::Core::DeviceQualifier& GetDefaultDevice() const;
  void SetDefaultDevice(const std::string& device);
  void SetDefaultDevice(ciface::Core::DeviceQualifier devq);

  void UpdateReferences(const ControllerInterface& devi);

  // This returns a lock that should be held before calling State() on any control
  // references and GetState(), by extension. This prevents a race condition
  // which happens while handling a hotplug event because a control reference's State()
  // could be called before we have finished updating the reference.
  static std::unique_lock<std::recursive_mutex> GetStateLock();

  std::vector<std::unique_ptr<ControlGroup>> groups;

  // Maps a float from -1.0..+1.0 to an integer of the provided values.
  template <typename T, typename F>
  static T MapFloat(F input_value, T zero_value, T neg_1_value = std::numeric_limits<T>::min(),
                    T pos_1_value = std::numeric_limits<T>::max())
  {
    static_assert(std::is_integral<T>(), "T is only sane for int types.");
    static_assert(std::is_floating_point<F>(), "F is only sane for float types.");

    static_assert(std::numeric_limits<long>::min() <= std::numeric_limits<T>::min() &&
                      std::numeric_limits<long>::max() >= std::numeric_limits<T>::max(),
                  "long is not a superset of T. use of std::lround is not sane.");

    // Here we round when converting from float to int.
    // After applying our deadzone, resizing, and reshaping math
    // we sometimes have a near-zero value which is slightly negative. (e.g. -0.0001)
    // Casting would round down but rounding will yield our "zero_value".

    if (input_value > 0)
      return T(std::lround((pos_1_value - zero_value) * input_value + zero_value));
    else
      return T(std::lround((zero_value - neg_1_value) * input_value + zero_value));
  }

private:
  ciface::Core::DeviceQualifier m_default_device;
  bool m_default_device_is_connected{false};
};
}  // namespace ControllerEmu
