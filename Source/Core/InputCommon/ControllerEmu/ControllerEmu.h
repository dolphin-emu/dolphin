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

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/IniFile.h"
#include "Common/MathUtil.h"
#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerInterface/Device.h"

class ControllerInterface;

const char* const named_directions[] = {_trans("Up"), _trans("Down"), _trans("Left"),
                                        _trans("Right")};

class ControlReference;

namespace ControllerEmu
{
class ControlGroup;

// Represents calibration data found on Wii Remotes + extensions with a zero and a max value.
// (e.g. accelerometer data)
// Bits of precision specified to handle common situation of differing precision in the actual data.
template <typename T, size_t Bits>
struct TwoPointCalibration
{
  TwoPointCalibration() = default;
  TwoPointCalibration(const T& zero_, const T& max_) : zero{zero_}, max{max_} {}

  // Sanity check is that max and zero are not equal.
  constexpr bool IsSane() const
  {
    if constexpr (std::is_arithmetic_v<T>)
    {
      return max != zero;
    }
    else
    {
      return std::equal(std::begin(max.data), std::end(max.data), std::begin(zero.data),
                        std::not_equal_to<>());
    }
  }

  static constexpr size_t BITS_OF_PRECISION = Bits;

  T zero;
  T max;
};

// Represents calibration data with a min, zero, and max value. (e.g. joystick data)
template <typename T, size_t Bits>
struct ThreePointCalibration
{
  ThreePointCalibration() = default;
  ThreePointCalibration(const T& min_, const T& zero_, const T& max_)
      : min{min_}, zero{zero_}, max{max_}
  {
  }

  // Sanity check is that min and max are on opposite sides of the zero value.
  constexpr bool IsSane() const
  {
    if constexpr (std::is_arithmetic_v<T>)
    {
      return MathUtil::Sign(zero - min) * MathUtil::Sign(zero - max) == -1;
    }
    else
    {
      for (size_t i = 0; i != std::size(zero.data); ++i)
      {
        if (MathUtil::Sign(zero.data[i] - min.data[i]) *
                MathUtil::Sign(zero.data[i] - max.data[i]) !=
            -1)
        {
          return false;
        }
      }
      return true;
    }
  }

  static constexpr size_t BITS_OF_PRECISION = Bits;

  T min;
  T zero;
  T max;
};

// Represents a raw/uncalibrated N-dimensional value of input data. (e.g. Joystick X and Y)
// A normalized value can be calculated with a provided {Two,Three}PointCalibration.
// Values are adjusted with mismatched bits of precision.
// Underlying type may be an unsigned type or a a Common::TVecN<> of an unsigned type.
template <typename T, size_t Bits>
struct RawValue
{
  RawValue() = default;
  explicit RawValue(const T& value_) : value{value_} {}

  static constexpr size_t BITS_OF_PRECISION = Bits;

  T value;

  template <typename OtherT, size_t OtherBits>
  auto GetNormalizedValue(const TwoPointCalibration<OtherT, OtherBits>& calibration) const
  {
    const auto value_expansion =
        std::max(0, int(calibration.BITS_OF_PRECISION) - int(BITS_OF_PRECISION));

    const auto calibration_expansion =
        std::max(0, int(BITS_OF_PRECISION) - int(calibration.BITS_OF_PRECISION));

    const auto calibration_zero = ExpandValue(calibration.zero, calibration_expansion) * 1.f;
    const auto calibration_max = ExpandValue(calibration.max, calibration_expansion) * 1.f;

    // Multiplication by 1.f to floatify either a scalar or a Vec.
    return (ExpandValue(value, value_expansion) * 1.f - calibration_zero) /
           (calibration_max - calibration_zero);
  }

  template <typename OtherT, size_t OtherBits>
  auto GetNormalizedValue(const ThreePointCalibration<OtherT, OtherBits>& calibration) const
  {
    const auto value_expansion =
        std::max(0, int(calibration.BITS_OF_PRECISION) - int(BITS_OF_PRECISION));

    const auto calibration_expansion =
        std::max(0, int(BITS_OF_PRECISION) - int(calibration.BITS_OF_PRECISION));

    const auto calibration_min = ExpandValue(calibration.min, calibration_expansion) * 1.f;
    const auto calibration_zero = ExpandValue(calibration.zero, calibration_expansion) * 1.f;
    const auto calibration_max = ExpandValue(calibration.max, calibration_expansion) * 1.f;

    const auto use_max = calibration.zero < value;

    // Multiplication by 1.f to floatify either a scalar or a Vec.
    return (ExpandValue(value, value_expansion) * 1.f - calibration_zero) /
           (use_max * 1.f * (calibration_max - calibration_zero) +
            !use_max * 1.f * (calibration_zero - calibration_min));
  }

  template <typename OtherT>
  static OtherT ExpandValue(OtherT value, size_t bits)
  {
    if constexpr (std::is_arithmetic_v<OtherT>)
    {
      return Common::ExpandValue(value, bits);
    }
    else
    {
      for (size_t i = 0; i != std::size(value.data); ++i)
        value.data[i] = Common::ExpandValue(value.data[i], bits);
      return value;
    }
  }
};

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
  void UpdateSingleControlReference(const ControllerInterface& devi, ControlReference* ref);

  // This returns a lock that should be held before calling State() on any control
  // references and GetState(), by extension. This prevents a race condition
  // which happens while handling a hotplug event because a control reference's State()
  // could be called before we have finished updating the reference.
  [[nodiscard]] static std::unique_lock<std::recursive_mutex> GetStateLock();

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

protected:
  // TODO: Wiimote attachment has its own member that isn't being used..
  ciface::ExpressionParser::ControlEnvironment::VariableContainer m_expression_vars;

  void UpdateReferences(ciface::ExpressionParser::ControlEnvironment& env);

private:
  ciface::Core::DeviceQualifier m_default_device;
  bool m_default_device_is_connected{false};
};
}  // namespace ControllerEmu
