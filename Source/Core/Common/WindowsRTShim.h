// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This header contains type definitions that are shared between the Dolphin core and
// other parts of the code. Any definitions that are only used by the core should be
// placed in "Common.h" instead.

#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include <winerror.h>

namespace winrt
{
struct event_token
{
  int value{};
};
inline void init_apartment()
{
}
inline void uninit_apartment()
{
}

struct hresult
{
  HRESULT value;
  constexpr hresult(HRESULT v = 0) noexcept : value(v) {}
  constexpr operator HRESULT() const noexcept { return value; }
};

struct hresult_error
{
  HRESULT m_hr;
  explicit hresult_error(HRESULT hr) : m_hr(hr) {}
  HRESULT code() const noexcept { return m_hr; }

  std::string message() const noexcept
  {
    return "HRESULT 0x" + std::to_string(static_cast<unsigned long>(m_hr));
  }
};

inline void check_hresult(HRESULT hr) noexcept
{
  if (FAILED(hr))
    std::abort();
}

inline std::string to_string(const std::string& s)
{
  return s;
}

template <typename T>
struct array_view
{
  using pointer = T*;
  using size_type = size_t;
  array_view(pointer, size_type) {}
};

namespace Windows
{
namespace System
{
namespace Power
{
enum BatteryStatus
{
  NotPresent,
  Discharging,
  Idle,
  Charging
};
}
}  // namespace System

namespace Devices
{
namespace Haptics
{
struct SimpleHapticsControllerFeedback
{
  uint16_t Waveform() const { return 0; }
};
struct SimpleHapticsController
{
  void StopFeedback() {}
  void SendHapticFeedback(SimpleHapticsControllerFeedback, double) {}
  std::vector<SimpleHapticsControllerFeedback> SupportedFeedback() const { return {}; }
};
struct KnownSimpleHapticsControllerWaveforms
{
  static uint16_t Click() { return 1; }
  static uint16_t BuzzContinuous() { return 2; }
  static uint16_t RumbleContinuous() { return 3; }
};
}  // namespace Haptics

namespace Power
{
struct BatteryReport
{
  winrt::Windows::System::Power::BatteryStatus Status() const
  {
    return winrt::Windows::System::Power::BatteryStatus::NotPresent;
  }
  struct Capacity
  {
    int GetInt32() const { return 0; }
  };
  Capacity FullChargeCapacityInMilliwattHours() const { return {}; }
  Capacity RemainingCapacityInMilliwattHours() const { return {}; }
  explicit operator bool() const { return false; }
};
}  // namespace Power
}  // namespace Devices

namespace Foundation
{
namespace Collections
{
template <typename T>
struct IVectorView
{
};
}  // namespace Collections
}  // namespace Foundation

namespace Gaming
{
namespace Input
{
struct GamepadReading
{
  double LeftTrigger{}, RightTrigger{}, LeftThumbstickX{}, LeftThumbstickY{}, RightThumbstickX{},
      RightThumbstickY{};
};
struct GamepadVibration
{
  double LeftMotor{}, RightMotor{}, LeftTrigger{}, RightTrigger{};
};
enum GameControllerSwitchPosition
{
  Center,
  Up,
  Down,
  Left,
  Right
};
enum GameControllerSwitchKind
{
  TwoWay,
  FourWay
};
enum GameControllerButtonLabel
{
  None
};

struct RawGameController
{
  int AxisCount() const { return 0; }
  int SwitchCount() const { return 0; }
  GameControllerSwitchKind GetSwitchKind(int) const { return TwoWay; }
  int ButtonCount() const { return 0; }
  GameControllerButtonLabel GetButtonLabel(int) const { return None; }

  std::vector<winrt::Windows::Devices::Haptics::SimpleHapticsController>
  SimpleHapticsControllers() const
  {
    return {};
  }

  void GetCurrentReading(const winrt::array_view<bool>&,
                         const std::vector<GameControllerSwitchPosition>&,
                         const std::vector<double>&) const
  {
  }

  std::wstring DisplayName() const { return L""; }
  uint16_t HardwareVendorId() const { return 0; }
  uint16_t HardwareProductId() const { return 0; }

  template <typename T>
  T try_as() const
  {
    return T{};
  }

  static std::vector<RawGameController> RawGameControllers() { return {}; }
  static winrt::event_token RawGameControllerAdded(auto) { return {}; }
  static winrt::event_token RawGameControllerRemoved(auto) { return {}; }

  bool operator==(const RawGameController&) const { return false; }
};

struct Gamepad
{
  Gamepad() = default;
  Gamepad(std::nullptr_t) {}
  explicit operator bool() const { return false; }
  static Gamepad FromGameController(const RawGameController&) { return Gamepad(nullptr); }
  GamepadReading GetCurrentReading() const { return {}; }
  void Vibration(const GamepadVibration&) {}
};

struct IGameControllerBatteryInfo
{
  winrt::Windows::Devices::Power::BatteryReport TryGetBatteryReport() const { return {}; }
  explicit operator bool() const { return false; }
};
}  // namespace Input
}  // namespace Gaming
}  // namespace Windows
}  // namespace winrt
