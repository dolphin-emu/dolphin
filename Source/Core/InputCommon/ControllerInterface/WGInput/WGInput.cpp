// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/WGInput/WGInput.h"

#include <array>
#include <map>
#include <string_view>
#include <utility>

// For CoGetApartmentType
#include <objbase.h>
#pragma comment(lib, "ole32")

// NOTE: winrt translates com failures to c++ exceptions, so we must use try/catch in this file to
// prevent possible errors from escaping and terminating Dolphin.
#include <winrt/base.h>
#include <winrt/windows.devices.haptics.h>
#include <winrt/windows.devices.power.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.gaming.input.h>
#include <winrt/windows.system.power.h>
#pragma comment(lib, "windowsapp")

#include <fmt/format.h>

#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/WorkQueueThread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace WGI = winrt::Windows::Gaming::Input;
namespace Haptics = winrt::Windows::Devices::Haptics;
using winrt::Windows::Foundation::Collections::IVectorView;

namespace ciface::WGInput
{
static constexpr std::string_view SOURCE_NAME = "WGInput";

// These names correspond to the values of the GameControllerButtonLabel enum.
// "None" is not used.
// There are some overlapping names assuming no device exposes both
// GameControllerButtonLabel::XboxLeftBumper and GameControllerButtonLabel::LeftBumper.
// If needed we can prepend "Xbox" to relevant input names on conflict in the future.
static constexpr std::array wgi_button_names = {
    "None",      "Back",     "Start",      "Menu",     "View",     "Pad N",
    "Pad S",     "Pad W",    "Pad E",      "Button A", "Button B", "Button X",
    "Button Y",  "Bumper L", "Trigger L",  "Thumb L",  "Bumper R", "Trigger R",
    "Thumb R",   "Paddle 1", "Paddle 2",   "Paddle 3", "Paddle 4", "Mode",
    "Select",    "Menu",     "View",       "Back",     "Start",    "Options",
    "Share",     "Pad N",    "Pad S",      "Pad W",    "Pad E",    "Letter A",
    "Letter B",  "Letter C", "Letter L",   "Letter R", "Letter X", "Letter Y",
    "Letter Z",  "Cross",    "Circle",     "Square",   "Triangle", "Bumper L",
    "Trigger L", "Thumb L",  "Left 1",     "Left 2",   "Left 3",   "Bumper R",
    "Trigger R", "Thumb R",  "Right 1",    "Right 2",  "Right 3",  "Paddle 1",
    "Paddle 2",  "Paddle 3", "Paddle 4",   "Plus",     "Minus",    "Down Left Arrow",
    "Dial L",    "Dial R",   "Suspension",
};

template <typename T, typename M>
struct MemberName
{
  M T::*ptr;
  const char* name;
};

static constexpr MemberName<WGI::GamepadReading, double> gamepad_trigger_names[] = {
    {&WGI::GamepadReading::LeftTrigger, "Trigger L"},
    {&WGI::GamepadReading::RightTrigger, "Trigger R"}};

static constexpr MemberName<WGI::GamepadReading, double> gamepad_axis_names[] = {
    {&WGI::GamepadReading::LeftThumbstickX, "Left X"},
    {&WGI::GamepadReading::LeftThumbstickY, "Left Y"},
    {&WGI::GamepadReading::RightThumbstickX, "Right X"},
    {&WGI::GamepadReading::RightThumbstickY, "Right Y"}};

static constexpr MemberName<WGI::GamepadVibration, double> gamepad_motor_names[] = {
    {&WGI::GamepadVibration::LeftMotor, "Motor L"},
    {&WGI::GamepadVibration::RightMotor, "Motor R"},
    {&WGI::GamepadVibration::LeftTrigger, "Trigger L"},
    {&WGI::GamepadVibration::RightTrigger, "Trigger R"}};

class Device : public Core::Device
{
public:
  Device(std::string name, WGI::RawGameController raw_controller, WGI::Gamepad gamepad)
      : m_name(std::move(name)), m_raw_controller(raw_controller), m_gamepad(gamepad)
  {
    // Buttons:
    PopulateButtons();

    // Add inputs for IGamepad interface if available.
    if (m_gamepad)
    {
      // Axes:
      for (auto& axis : gamepad_axis_names)
      {
        AddInput(new NamedAxis(&(m_gamepad_reading.*axis.ptr), 0.0, -1.0, axis.name));
        AddInput(new NamedAxis(&(m_gamepad_reading.*axis.ptr), 0.0, +1.0, axis.name));
      }

      // Triggers:
      for (auto& trigger : gamepad_trigger_names)
        AddInput(new NamedTrigger(&(m_gamepad_reading.*trigger.ptr), trigger.name));

      // Motors:
      for (auto& motor : gamepad_motor_names)
        AddOutput(new NamedMotor(&(m_state_out.*motor.ptr), motor.name, this));
    }

    // Add IRawGameController's axes if IGamepad is not available.
    // This may need to change if some devices expose additional axes.
    // Maybe we can determine which additional axes are not in IGamepad's collection.
    const bool use_raw_controller_axes = !m_gamepad;

    if (use_raw_controller_axes)
    {
      try
      {
        // Axes:
        m_axes.resize(m_raw_controller.AxisCount());

        u32 i = 0;
        for (auto& axis : m_axes)
        {
          // AddAnalogInputs adds additional "FullAnalogSurface" Inputs.
          AddAnalogInputs(new IndexedAxis(&axis, 0.5, +0.5, i),
                          new IndexedAxis(&axis, 0.5, -0.5, i));
          ++i;
        }
      }
      catch (winrt::hresult_error)
      {
        // If AxisCount failed, m_axes will remain zero-sized; nothing to do.
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Error populating axes");
      }
    }

    // Apparently some devices (e.g. DS4) provide the IGameController interface
    // but expose the dpad only on a switch (IRawGameController interface).
    // We'll need to add switches regardless of available interfaces.
    constexpr bool use_raw_controller_switches = true;

    // Switches (Hats):
    if (use_raw_controller_switches)
    {
      try
      {
        m_switches.resize(m_raw_controller.SwitchCount());
        // Accumulate switch kinds first, to ensure no inputs are added if there is any error.
        std::vector<WGI::GameControllerSwitchKind> switch_kinds;
        for (u32 i = 0; i < m_switches.size(); i++)
          switch_kinds.push_back(m_raw_controller.GetSwitchKind(i));

        u32 i = 0;
        for (auto& swtch : m_switches)
        {
          using gcsp = WGI::GameControllerSwitchPosition;

          AddInput(new IndexedSwitch(&swtch, i, gcsp::Up));
          AddInput(new IndexedSwitch(&swtch, i, gcsp::Down));

          if (switch_kinds[i] != WGI::GameControllerSwitchKind::TwoWay)
          {
            // If it's not a "two-way" switch (up/down only) then add the left/right inputs.
            AddInput(new IndexedSwitch(&swtch, i, gcsp::Left));
            AddInput(new IndexedSwitch(&swtch, i, gcsp::Right));
          }

          ++i;
        }
      }
      catch (winrt::hresult_error)
      {
        // Safe as no inputs will have been added.
        m_switches.clear();
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Error populating switches");
      }
    }

    // Haptics:
    PopulateHaptics();

    // Battery:
    if (UpdateBatteryLevel())
      AddInput(new Battery(&m_battery_level));
  }

  int GetSortPriority() const override { return -1; }

  const WGI::RawGameController GetRawGameController() const { return m_raw_controller; }

private:
  using ButtonValueType = u8;

  class Button : public Input
  {
  public:
    explicit Button(const ButtonValueType* button) : m_button(*button) {}
    ControlState GetState() const override { return ControlState(m_button != 0); }

  private:
    const ButtonValueType& m_button;
  };

  // A button with one of the "labels" that WGI provides.
  class NamedButton final : public Button
  {
  public:
    NamedButton(const ButtonValueType* button, std::string_view name) : Button(button), m_name(name)
    {
    }
    std::string GetName() const override { return std::string(m_name); }

  private:
    const std::string_view m_name;
  };

  // A button with no label so we name it by its index.
  class IndexedButton final : public Button
  {
  public:
    IndexedButton(const ButtonValueType* button, u32 index) : Button(button), m_index(index) {}
    std::string GetName() const override { return fmt::format("Button {}", m_index); }

  private:
    u32 m_index;
  };

  class Axis : public Input
  {
  public:
    Axis(const double* axis, double base, double range)
        : m_base(base), m_range(range), m_axis(*axis)
    {
    }

    ControlState GetState() const override { return ControlState(m_axis - m_base) / m_range; }

  protected:
    const double m_base;
    const double m_range;

  private:
    const double& m_axis;
  };

  class NamedAxis final : public Axis
  {
  public:
    NamedAxis(const double* axis, double base, double range, std::string_view name)
        : Axis(axis, base, range), m_name(name)
    {
    }
    std::string GetName() const override
    {
      return fmt::format("{}{}", m_name, m_range < 0 ? '-' : '+');
    }

  private:
    const std::string_view m_name;
  };

  class NamedTrigger final : public Axis
  {
  public:
    NamedTrigger(const double* axis, std::string_view name) : Axis(axis, 0.0, 1.0), m_name(name) {}
    std::string GetName() const override { return std::string(m_name); }

  private:
    const std::string_view m_name;
  };

  class NamedMotor final : public Output
  {
  public:
    NamedMotor(double* motor, std::string_view name, Device* parent)
        : m_motor(*motor), m_name(name), m_parent(*parent)
    {
    }
    std::string GetName() const override { return std::string(m_name); }
    void SetState(ControlState state) override
    {
      if (m_motor == state)
        return;

      m_motor = state;
      m_parent.UpdateMotors();
    }

  private:
    double& m_motor;
    const std::string_view m_name;
    Device& m_parent;
  };

  class IndexedAxis final : public Axis
  {
  public:
    IndexedAxis(const double* axis, double base, double range, u32 index)
        : Axis(axis, base, range), m_index(index)
    {
    }
    std::string GetName() const override
    {
      return fmt::format("Axis {}{}", m_index, m_range < 0 ? '-' : '+');
    }

  private:
    const u32 m_index;
  };

  class IndexedSwitch final : public Input
  {
  public:
    IndexedSwitch(const WGI::GameControllerSwitchPosition* swtch, u32 index,
                  WGI::GameControllerSwitchPosition direction)
        : m_switch(*swtch), m_index(index), m_direction(static_cast<int32_t>(direction))
    {
    }
    std::string GetName() const override
    {
      return fmt::format("Switch {} {}", m_index, "NESW"[m_direction / 2]);
    }
    ControlState GetState() const override
    {
      if (m_switch == WGI::GameControllerSwitchPosition::Center)
        return 0.0;

      // All of the "inbetween" states (e.g. Up-Right) are one-off from the four cardinal
      // directions. This tests that the current switch state value is within 1 of the desired
      // state.
      const auto direction_diff = std::abs(static_cast<int32_t>(m_switch) - m_direction);
      return ControlState(direction_diff <= 1 || direction_diff == 7);
    }

  private:
    const WGI::GameControllerSwitchPosition& m_switch;
    const u32 m_index;
    const int32_t m_direction;
  };

  class Battery : public Input
  {
  public:
    Battery(const double* level) : m_level(*level) {}

    bool IsDetectable() const override { return false; }

    ControlState GetState() const override { return m_level; }

    std::string GetName() const override { return "Battery"; }

  private:
    const double& m_level;
  };

  class SimpleHaptics : public Output
  {
  public:
    SimpleHaptics(Haptics::SimpleHapticsController haptics,
                  Haptics::SimpleHapticsControllerFeedback feedback, u32 haptics_index)
        : m_haptics(haptics), m_feedback(feedback), m_haptics_index(haptics_index)
    {
    }

    void SetState(ControlState state) override
    {
      if (m_current_state == state)
        return;

      m_current_state = state;

      try
      {
        if (state)
          m_haptics.SendHapticFeedback(m_feedback, state);
        else
          m_haptics.StopFeedback();
      }
      catch (winrt::hresult_error)
      {
        // Ignore
      }
    }

  protected:
    u32 GetHapticsIndex() const { return m_haptics_index; }

  private:
    Haptics::SimpleHapticsController m_haptics;
    Haptics::SimpleHapticsControllerFeedback m_feedback;
    const u32 m_haptics_index;
    ControlState m_current_state = 0;
  };

  class NamedFeedback final : public SimpleHaptics
  {
  public:
    NamedFeedback(Haptics::SimpleHapticsController haptics,
                  Haptics::SimpleHapticsControllerFeedback feedback, u32 haptics_index,
                  std::string_view feedback_name)
        : SimpleHaptics(haptics, feedback, haptics_index), m_feedback_name(feedback_name)
    {
    }
    std::string GetName() const override
    {
      return fmt::format("{} {}", m_feedback_name, GetHapticsIndex());
    }

  private:
    const std::string_view m_feedback_name;
  };

  void PopulateButtons()
  {
    try
    {
      // Using RawGameController for buttons because it gives us a nice array instead of a bitmask.
      m_buttons.resize(m_raw_controller.ButtonCount());

      u32 i = 0;
      for (const auto& button : m_buttons)
      {
        WGI::GameControllerButtonLabel lbl{WGI::GameControllerButtonLabel::None};
        try
        {
          lbl = m_raw_controller.GetButtonLabel(i);
        }
        catch (winrt::hresult_error)
        {
          lbl = WGI::GameControllerButtonLabel::None;
        }

        const int32_t button_name_idx = static_cast<int32_t>(lbl);
        if (lbl != WGI::GameControllerButtonLabel::None &&
            button_name_idx < wgi_button_names.size())
          AddInput(new NamedButton(&button, wgi_button_names[button_name_idx]));
        else
          AddInput(new IndexedButton(&button, i));

        ++i;
      }
    }
    catch (winrt::hresult_error)
    {
      // If ButtonCount failed, m_buttons will be zero-sized.
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Error populating buttons");
    }
  }

  void PopulateHaptics()
  {
    static const std::map<uint16_t, std::string> waveform_name_map{
        {Haptics::KnownSimpleHapticsControllerWaveforms::Click(), "Click"},
        {Haptics::KnownSimpleHapticsControllerWaveforms::BuzzContinuous(), "Buzz"},
        {Haptics::KnownSimpleHapticsControllerWaveforms::RumbleContinuous(), "Rumble"},
    };

    try
    {
      // SimpleHapticsControllers is available from Windows 1709.
      u32 haptics_index = 0;
      for (auto haptics_controller : m_raw_controller.SimpleHapticsControllers())
      {
        for (Haptics::SimpleHapticsControllerFeedback feedback :
             haptics_controller.SupportedFeedback())
        {
          const uint16_t waveform = feedback.Waveform();
          auto waveform_name_it = waveform_name_map.find(waveform);
          if (waveform_name_it == waveform_name_map.end())
          {
            WARN_LOG_FMT(CONTROLLERINTERFACE,
                         "WGInput: Unhandled haptics feedback waveform: 0x{:04x}.", waveform);
            continue;
          }
          AddOutput(new NamedFeedback(haptics_controller, feedback, haptics_index,
                                      waveform_name_it->second));
        }
        ++haptics_index;
      }
    }
    catch (winrt::hresult_error)
    {
      // Don't need to cleanup any state. It's OK if some outputs were added.
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Error populating haptics");
    }
  }

  std::string GetName() const override { return m_name; }

  std::string GetSource() const override { return std::string(SOURCE_NAME); }

  Core::DeviceRemoval UpdateInput() override
  {
    // IRawGameController:
    static_assert(sizeof(bool) == sizeof(ButtonValueType));
    // This cludge is needed to workaround GetCurrentReading wanting array_view<bool>, while
    // using std::vector<bool> would create a bit-packed array, which isn't wanted. So, we keep
    // vector<u8> and view it as array<bool>.
    auto buttons =
        winrt::array_view<bool>(reinterpret_cast<winrt::array_view<bool>::pointer>(&m_buttons[0]),
                                static_cast<winrt::array_view<bool>::size_type>(m_buttons.size()));
    try
    {
      m_raw_controller.GetCurrentReading(buttons, m_switches, m_axes);
    }
    catch (winrt::hresult_error error)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE,
                    "WGInput: IRawGameController::GetCurrentReading failed: {}", error.code());
    }

    // IGamepad:
    if (m_gamepad)
    {
      try
      {
        m_gamepad_reading = m_gamepad.GetCurrentReading();
      }
      catch (winrt::hresult_error error)
      {
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: IGamepad::GetCurrentReading failed: {}",
                      error.code());
      }
    }

    // IGameControllerBatteryInfo:
    if (!UpdateBatteryLevel())
      DEBUG_LOG_FMT(CONTROLLERINTERFACE, "WGInput: UpdateBatteryLevel failed.");

    return Core::DeviceRemoval::Keep;
  }

  void UpdateMotors()
  {
    try
    {
      m_gamepad.Vibration(m_state_out);
    }
    catch (winrt::hresult_error)
    {
      // Ignore
    }
  }

  bool UpdateBatteryLevel()
  {
    try
    {
      // Workaround for Steam. If Steam's GameOverlayRenderer64.dll is loaded, battery_info is null.
      auto battery_info = m_raw_controller.try_as<WGI::IGameControllerBatteryInfo>();
      if (!battery_info)
        return false;
      const winrt::Windows::Devices::Power::BatteryReport report =
          battery_info.TryGetBatteryReport();
      if (!report)
        return false;

      // TryGetBatteryReport causes a memleak of 0x58 bytes each time it is called, up to at
      // least Windows 21H2. A hack to fix the memleak is recorded here for posterity.
      // auto report_ = *(uintptr_t***)&report;
      // auto rc = &report_[0x40 / 8][0x30 / 8];
      // if (*rc == 2)
      //  (*rc)--;

      using winrt::Windows::System::Power::BatteryStatus;
      const BatteryStatus status = report.Status();
      switch (status)
      {
      case BatteryStatus::NotPresent:
        m_battery_level = 0;
        return true;

      case BatteryStatus::Idle:
      case BatteryStatus::Charging:
        m_battery_level = BATTERY_INPUT_MAX_VALUE;
        return true;

      default:
        break;
      }

      const int32_t full_value = report.FullChargeCapacityInMilliwattHours().GetInt32();
      const int32_t remaining_value = report.RemainingCapacityInMilliwattHours().GetInt32();
      m_battery_level = BATTERY_INPUT_MAX_VALUE * remaining_value / full_value;
      return true;
    }
    catch (winrt::hresult_error)
    {
      return false;
    }
  }

  const std::string m_name;

  const WGI::RawGameController m_raw_controller;
  std::vector<ButtonValueType> m_buttons;
  std::vector<WGI::GameControllerSwitchPosition> m_switches;
  std::vector<double> m_axes;

  WGI::Gamepad m_gamepad = nullptr;
  WGI::GamepadReading m_gamepad_reading{};
  WGI::GamepadVibration m_state_out{};

  ControlState m_battery_level = 0;
};

enum class AddRemoveEventType
{
  AddOrReplace,
  Remove,
};

struct AddRemoveEvent
{
  AddRemoveEventType type;
  WGI::RawGameController raw_game_controller;
};

static thread_local bool s_initialized_winrt;
static winrt::event_token s_event_added, s_event_removed;
static Common::WorkQueueThread<AddRemoveEvent> s_device_add_remove_queue;

static bool COMIsInitialized()
{
  APTTYPE apt_type{};
  APTTYPEQUALIFIER apt_qualifier{};
  return CoGetApartmentType(&apt_type, &apt_qualifier) == S_OK;
}

static void AddDevice(const WGI::RawGameController& raw_game_controller)
{
  // Get user-facing device name if available. Otherwise generate a name from vid/pid.
  std::string device_name;
  try
  {
    device_name = StripWhitespace(WStringToUTF8(raw_game_controller.DisplayName().c_str()));
    if (device_name.empty())
    {
      const u16 vid = raw_game_controller.HardwareVendorId();
      const u16 pid = raw_game_controller.HardwareProductId();
      device_name = fmt::format("Device_{:04x}:{:04x}", vid, pid);
      INFO_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Empty device name, using {}", device_name);
    }
  }
  catch (winrt::hresult_error)
  {
    device_name = "Device";
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Failed to get device name, using {}", device_name);
  }

  try
  {
    WGI::Gamepad gamepad = WGI::Gamepad::FromGameController(raw_game_controller);
    // Note that gamepad may be nullptr here. The Device class will deal with this.
    auto dev = std::make_shared<Device>(std::move(device_name), raw_game_controller, gamepad);

    // Only add if it has some inputs/outputs.
    if (dev->Inputs().size() || dev->Outputs().size())
      g_controller_interface.AddDevice(std::move(dev));
  }
  catch (winrt::hresult_error)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Failed to add device {}", device_name);
  }
}

static void RemoveDevice(const WGI::RawGameController& raw_game_controller)
{
  g_controller_interface.RemoveDevice([&](const auto* dev) {
    if (dev->GetSource() != SOURCE_NAME)
      return false;
    return static_cast<const Device*>(dev)->GetRawGameController() == raw_game_controller;
  });
}

#pragma warning(push)
// 'winrt::impl::implements_delegate<winrt::Windows::Foundation::EventHandler<winrt::Windows::Gaming::Input::RawGameController>,H>':
// class has virtual functions, but its non-trivial destructor is not virtual; instances of this
// class may not be destructed correctly
// (H is the lambda)
#pragma warning(disable : 4265)

static void HandleAddRemoveEvent(AddRemoveEvent evt)
{
  try
  {
    winrt::init_apartment();
  }
  catch (const winrt::hresult_error& ex)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE,
                  "WGInput: Failed to CoInitialize for add/remove controller event: {}",
                  WStringToUTF8(ex.message()));
    return;
  }
  Common::ScopeGuard coinit_guard([] { winrt::uninit_apartment(); });

  switch (evt.type)
  {
  case AddRemoveEventType::AddOrReplace:
    RemoveDevice(evt.raw_game_controller);
    AddDevice(evt.raw_game_controller);
    break;
  case AddRemoveEventType::Remove:
    RemoveDevice(evt.raw_game_controller);
    break;
  default:
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Invalid add/remove controller event: {}",
                  std::to_underlying(evt.type));
  }
}

void Init()
{
  if (!COMIsInitialized())
  {
    // NOTE: Devices in g_controller_interface should only be accessed by threads that have had
    // winrt (com) initialized.
    winrt::init_apartment();
    s_initialized_winrt = true;
  }

  s_device_add_remove_queue.Reset("WGInput Add/Remove Device Thread", HandleAddRemoveEvent);

  try
  {
    // These events will be invoked from WGI-managed threadpool.
    s_event_added = WGI::RawGameController::RawGameControllerAdded(
        [](auto&&, WGI::RawGameController raw_game_controller) {
          s_device_add_remove_queue.EmplaceItem(
              AddRemoveEvent{AddRemoveEventType::AddOrReplace, std::move(raw_game_controller)});
        });

    s_event_removed = WGI::RawGameController::RawGameControllerRemoved(
        [](auto&&, WGI::RawGameController raw_game_controller) {
          s_device_add_remove_queue.EmplaceItem(
              AddRemoveEvent{AddRemoveEventType::Remove, std::move(raw_game_controller)});
        });
  }
  catch (winrt::hresult_error)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: Failed to register event handlers");
  }
}

#pragma warning(pop)

void DeInit()
{
  s_device_add_remove_queue.Shutdown();

  WGI::RawGameController::RawGameControllerAdded(s_event_added);
  WGI::RawGameController::RawGameControllerRemoved(s_event_removed);

  if (s_initialized_winrt)
  {
    winrt::uninit_apartment();
    s_initialized_winrt = false;
  }
}

void PopulateDevices()
{
  // WGI Interfaces to potentially use:
  // Gamepad: Buttons, 2x Sticks and 2x Triggers, 4x Vibration Motors
  // RawGameController: Buttons, Switches (Hats), Axes, Haptics
  // The following are not implemented:
  // ArcadeStick: Buttons (probably no need to specialize, literally just buttons)
  // FlightStick: Buttons, HatSwitch, Pitch, Roll, Throttle, Yaw
  // RacingWheel: Buttons, Clutch, Handbrake, PatternShifterGear, Throttle, Wheel, WheelMotor
  // UINavigationController: Directions, Scrolling, etc.

  // RawGameController available from Windows 15063.
  //  properties added in 1709: DisplayName, NonRoamableId, SimpleHapticsControllers

  try
  {
    for (const WGI::RawGameController& raw_game_controller :
         WGI::RawGameController::RawGameControllers())
    {
      RemoveDevice(raw_game_controller);
      AddDevice(raw_game_controller);
    }
  }
  catch (winrt::hresult_error)
  {
    // Only reach here if RawGameControllers() failed
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "WGInput: PopulateDevices failed");
  }
}

}  // namespace ciface::WGInput
