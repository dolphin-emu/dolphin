// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

#include "InputCommon/ControllerInterface/InputChannel.h"

// idk in case I wanted to change it from double or something, idk what's best,
// but code assumes it's double now
typedef double ControlState;

namespace ciface
{
// 100Hz which homebrew docs very roughly imply is within WiiMote normal
// range, used for periodic haptic effects though often ignored by devices
// TODO: Make this configurable.
constexpr int RUMBLE_PERIOD_MS = 10;

// This needs to be at least as long as the longest rumble that might ever be played.
// Too short and it's going to stop in the middle of a long effect.
// Infinite values are invalid for ramp effects and probably not sensible.
constexpr int RUMBLE_LENGTH_MS = 1000 * 10;

// All inputs (other than accel/gyro) return 1.0 as their maximum value.
// Battery inputs will almost always be mapped to the "Battery" setting which is a percentage.
// If someone actually wants to map a battery input to a regular control they can divide by 100.
// I think this is better than requiring multiplication by 100 for the most common usage.
constexpr ControlState BATTERY_INPUT_MAX_VALUE = 100.0;

namespace Core
{
class Device
{
public:
  class Input;
  class Output;

  //
  // Control
  //
  // Control includes inputs and outputs
  //
  class Control
  {
  public:
    virtual ~Control() = default;
    virtual std::string GetName() const = 0;
    virtual Input* ToInput() { return nullptr; }
    virtual Output* ToOutput() { return nullptr; }
    virtual void ResetState() {}

    // May be overridden to allow multiple valid names.
    // Useful for backwards-compatible configurations when names change.
    virtual bool IsMatchingName(std::string_view name) const;
  };

  // A set of flags we can define to determine whether a Control should be
  // read (or written), or ignored/blocked, based on our current game app/window focus.
  // They are in order of priority, and some of them are mutually exclusive.
  //
  // These flags are per Control on inputs, but they will ultimately be managed by
  // ControlReference expressions. This is because pre-filtering Controls indivudally
  // would have been too complicated, expensive, and ultimately, useless.
  // Outputs don't implement GetFocusFlags() because they all default to the IgnoreFocus,
  // see ControlExpression for more.
  //
  // Users can use expressions to customize the focus requirements of any input/output
  // ControlReference, but we manually hardcode them in some Inputs (e.g. mouse) so
  // users don't have to bother in most cases, as it's easy to understand as a concept.
  enum class FocusFlags : u8
  {
    // The control only passes if we have focus (or the user accepts background input).
    RequireFocus = 0x01,
    // The control only passes if we have "full" focus, which means the cursor
    // has been locked into the game window. Ignored if cursor locking is off.
    RequireFullFocus = 0x02,
    // Some inputs are able to make you lose or gain focus or full focus,
    // for example a mouse click, or the Windows/Esc key. When these are pressed
    // and there is a window focus change, ignore them for the time being, as the
    // user didn't want the application to react. It basically blocks them until release,
    // meaning they will return 0. Ignored on outputs because it just isn't necessary.
    // Shouldn't be used without RequireFocus or RequireFullFocus.
    // Note that this won't 100% work with "Pause On Focus Loss" due to the ControlReference
    // gate not updating when the emulation is not running.
    IgnoreOnFocusChanged = 0x04,
    // Forces the control to pass even if we have no focus, useful for things like battery level.
    // This is not 0 because it needs higher priority over other flags.
    IgnoreFocus = 0x80,

    // Even expressions that have a fixed value should return this by default, as their focus can
    // be ignored by other means and users have the ability to ignore it with other functions.
    // Default for inputs and outputs, except for output ControlReference.
    Default = RequireFocus,
    // Outputs ignore focus by default. Change it here if desired.
    OutputDefault = IgnoreFocus
  };

  //
  // Input
  //
  // An input on a device
  //
  class Input : public Control
  {
  public:
    // Things like absolute axes/ absolute mouse position should override this to prevent
    // undesirable behavior in our mapping logic.
    virtual bool IsDetectable() const { return true; }

    virtual FocusFlags GetFocusFlags() const { return FocusFlags::Default; }

    // Implementations should return a value from 0.0 to 1.0 across their normal range.
    // One input should be provided for each "direction". (e.g. 2 for each axis)
    // If possible, negative values may be returned in situations where an opposing input
    // is activated. (e.g. When an underlying axis, X, is currently negative, "Axis X-",
    // will return a positive value and "Axis X+" may return a negative value.)
    // Doing so is solely to allow our input detection logic to better detect false positives.
    // This is necessary when making use of "FullAnalogSurface" as multiple inputs will be seen
    // increasing from 0.0 to 1.0 as a user tries to map just one. The negative values provide
    // a view of the underlying axis. (Negative values are clamped off before they reach
    // expression-parser or controller-emu)
    virtual ControlState GetState() const = 0;

    Input* ToInput() override { return this; }

    // Overridden by CombinedInput,
    // so hotkey logic knows Ctrl, L_Ctrl, and R_Ctrl are the same,
    // and so input detection can return the parent name.
    virtual bool IsChild(const Input*) const { return false; }

  protected:
    InputChannel GetCurrentInputChannel() const;
  };

  //
  // RelativeInput
  //
  // Helper to generate a relative input from an absolute one.
  // Keeps the last 2 absolute states and returns their difference.
  // It has one state per input channel, as otherwise one SetState() would break
  // GetState() on the other channels.
  // This is not directly mappable to analog sticks, there are function expressions for that.
  // m_scale is useful to automatically scale the input to a usable value by the emulation.
  // You don't have to use this implementation.
  //
  template <typename T = ControlState>
  class RelativeInput : public Input
  {
  public:
    RelativeInput(ControlState scale = 1.0) : m_scale(scale) {}

    void UpdateState(T absolute_state)
    {
      RelativeInputState& state = m_states[u8(GetCurrentInputChannel())];
      state.relative_state =
          state.initialized ? ControlState(absolute_state - state.absolute_state) : 0.0;
      state.absolute_state = absolute_state;
      state.initialized = true;
    }
    void ResetState() override
    {
      RelativeInputState& state = m_states[u8(GetCurrentInputChannel())];
      state.relative_state = 0.0;
      state.initialized = false;
    }
    // Different input channels are never updated concurrently so you can
    // safely call this the first time your devices loses/resets its absolute value
    void ResetAllStates()
    {
      for (u8 i = 0; i < std::size(m_states); ++i)
      {
        RelativeInputState& state = m_states[i];
        state.relative_state = 0.0;
        state.initialized = false;
      }
    }
    T GetAbsoluteState() const
    {
      RelativeInputState& state = m_states[u8(GetCurrentInputChannel())];
      return state.initialized ? state.absolute_state : T(0);
    }
    ControlState GetState() const override
    {
      const RelativeInputState& state = m_states[u8(GetCurrentInputChannel())];
      return state.relative_state * m_scale;
    }

  protected:
    struct RelativeInputState
    {
      T absolute_state;
      ControlState relative_state = 0.0;
      bool initialized = false;
    };

    RelativeInputState m_states[u8(InputChannel::Max)];
    // Not really necessary but it helps to add transparency to the final user,
    // we need a multiplier to have the relative values usable. Can also be used as range
    const ControlState m_scale;
  };

  //
  // Output
  //
  // An output on a device.
  // State 0 is expected to be the resting value.
  // It has a map of states as different objects can set its value.
  // The sum of all the states will be used as output.
  // You are responsible of calling SetState(0, source_object) after calling it with any other val.
  // SetState() and ResetState() need GetDevicesOutputLock().
  //
  class Output : public Control
  {
  public:
    virtual ~Output() = default;
    void SetState(ControlState state, const void* source_object);
    Output* ToOutput() override { return this; }
    void ResetState() override;

  private:
    virtual void SetStateInternal(ControlState state) = 0;
    std::map<const void*, ControlState> m_states;
    ControlState m_final_state = 0;
  };

  virtual ~Device();

  int GetId() const { return m_id; }
  void SetId(int id) { m_id = id; }
  virtual std::string GetName() const = 0;
  virtual std::string GetSource() const = 0;
  std::string GetQualifiedName() const;
  // Outputs are instead set and updated in different places
  virtual void UpdateInput() {}
  void ResetInput();
  void ResetOutput();

  // May be overridden to implement hotplug removal.
  // Currently handled on a per-backend basis but this could change.
  virtual bool IsValid() const { return true; }

  // Returns true whether this device is "virtual/emulated", not linked
  // to any actual physical device. Mostly used by keyboard and mouse devices,
  // and to avoid uselessly recreating the device unless really necessary.
  // Doesn't necessarily need to be set to true if the device is virtual.
  virtual bool IsVirtualDevice() const { return false; }

  // (e.g. Xbox 360 controllers have controller number LEDs which should match the ID we use.)
  virtual std::optional<int> GetPreferredId() const;

  // Use this to change the order in which devices are sorted in their list.
  // A higher priority means it will be one of the first ones (smaller index), making it more
  // likely to be index 0, which is automatically set as the default device when there isn't one.
  virtual int GetSortPriority() const { return 0; }

  const std::vector<Input*>& Inputs() const { return m_inputs; }
  const std::vector<Output*>& Outputs() const { return m_outputs; }

  Input* GetParentMostInput(Input* input) const;

  Input* FindInput(std::string_view name) const;
  Output* FindOutput(std::string_view name) const;

protected:
  void AddInput(Input* const i);
  void AddOutput(Output* const o);

  class FullAnalogSurface final : public Input
  {
  public:
    FullAnalogSurface(Input* low, Input* high) : m_low(*low), m_high(*high) {}
    ControlState GetState() const override;
    std::string GetName() const override;
    bool IsMatchingName(std::string_view name) const override;

  private:
    Input& m_low;
    Input& m_high;
  };

  void AddAnalogInputs(Input* low, Input* high)
  {
    AddInput(low);
    AddInput(high);
    AddInput(new FullAnalogSurface(low, high));
    AddInput(new FullAnalogSurface(high, low));
  }

  void AddCombinedInput(std::string name, const std::pair<std::string, std::string>& inputs);

private:
  int m_id;
  std::vector<Input*> m_inputs;
  std::vector<Output*> m_outputs;
};

//
// DeviceQualifier
//
// Device qualifier used to match devices.
// Currently has ( source, id, name ) properties which match a device
//
class DeviceQualifier
{
public:
  DeviceQualifier() : cid(-1) {}
  DeviceQualifier(std::string source_, const int id_, std::string name_)
      : source(std::move(source_)), cid(id_), name(std::move(name_))
  {
  }
  void FromDevice(const Device* const dev);
  void FromString(const std::string& str);
  std::string ToString() const;

  bool operator==(const DeviceQualifier& devq) const;
  bool operator!=(const DeviceQualifier& devq) const;

  bool operator==(const Device* dev) const;
  bool operator!=(const Device* dev) const;

  std::string source;
  int cid;
  std::string name;
};

class DeviceContainer
{
public:
  using Clock = std::chrono::steady_clock;

  struct InputDetection
  {
    std::shared_ptr<Device> device;
    Device::Input* input;
    Clock::time_point press_time;
    std::optional<Clock::time_point> release_time;
    ControlState smoothness;
  };

  Device::Input* FindInput(std::string_view name, const Device* def_dev) const;
  Device::Output* FindOutput(std::string_view name, const Device* def_dev) const;

  std::vector<std::string> GetAllDeviceStrings() const;
  std::string GetDefaultDeviceString() const;
  std::shared_ptr<Device> FindDevice(const DeviceQualifier& devq) const;

  std::unique_lock<std::mutex> GetDevicesOutputLock() const;

  bool HasConnectedDevice(const DeviceQualifier& qualifier) const;

  std::vector<InputDetection> DetectInput(const std::vector<std::string>& device_strings,
                                          std::chrono::milliseconds initial_wait,
                                          std::chrono::milliseconds confirmation_wait,
                                          std::chrono::milliseconds maximum_wait) const;

protected:
  // Exclusively needed when reading/writing "m_devices"
  mutable std::recursive_mutex m_devices_mutex;
  // Needed when setting values/state on outputs
  mutable std::mutex m_devices_output_mutex;
  std::vector<std::shared_ptr<Device>> m_devices;
};
}  // namespace Core
}  // namespace ciface
