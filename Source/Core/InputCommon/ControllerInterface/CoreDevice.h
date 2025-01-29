// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

// idk in case I wanted to change it to double or something, idk what's best
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
enum class DeviceRemoval
{
  Remove,
  Keep,
};

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
  class Control  // input or output
  {
  public:
    virtual ~Control() = default;
    virtual std::string GetName() const = 0;
    virtual Input* ToInput() { return nullptr; }
    virtual Output* ToOutput() { return nullptr; }

    // May be overridden to allow multiple valid names.
    // Useful for backwards-compatible configurations when names change.
    virtual bool IsMatchingName(std::string_view name) const;

    // May be overridden to hide in UI.
    // Useful for backwards-compatible configurations when names change.
    virtual bool IsHidden() const;
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

    // Implementations should return a value from 0.0 to 1.0 across their normal range.
    // One input should be provided for each "direction". (e.g. 2 for each axis)
    // If possible, negative values may be returned in situations where an opposing input is
    // activated. (e.g. When an underlying axis, X, is currently negative, "Axis X-", will return a
    // positive value and "Axis X+" may return a negative value.)
    // Doing so is solely to allow our input detection logic to better detect false positives.
    // This is necessary when making use of "FullAnalogSurface" as multiple inputs will be seen
    // increasing from 0.0 to 1.0 as a user tries to map just one. The negative values provide a
    // view of the underlying axis. (Negative values are clamped off before they reach
    // expression-parser or controller-emu)
    virtual ControlState GetState() const = 0;

    Input* ToInput() override { return this; }

    // Overridden by CombinedInput,
    // so hotkey logic knows Ctrl, L_Ctrl, and R_Ctrl are the same,
    // and so input detection can return the parent name.
    virtual bool IsChild(const Input*) const { return false; }
  };

  class RelativeInput : public Input
  {
  public:
    bool IsDetectable() const override { return false; }
  };

  //
  // Output
  //
  // An output on a device
  //
  class Output : public Control
  {
  public:
    virtual ~Output() = default;
    virtual void SetState(ControlState state) = 0;
    Output* ToOutput() override { return this; }
  };

  virtual ~Device();

  int GetId() const { return m_id; }
  void SetId(int id) { m_id = id; }
  virtual std::string GetName() const = 0;
  virtual std::string GetSource() const = 0;
  std::string GetQualifiedName() const;
  virtual DeviceRemoval UpdateInput() { return DeviceRemoval::Keep; }

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
  // Every platform should have at least one device with priority >= 0.
  static constexpr int DEFAULT_DEVICE_SORT_PRIORITY = std::numeric_limits<int>::max();
  virtual int GetSortPriority() const { return 0; }

  const std::vector<Input*>& Inputs() const { return m_inputs; }
  const std::vector<Output*>& Outputs() const { return m_outputs; }

  Input* GetParentMostInput(Input* input) const;

  Input* FindInput(std::string_view name) const;
  Output* FindOutput(std::string_view name) const;

protected:
  void AddInput(Input* const i);
  void AddOutput(Output* const o);

  // Pass Inputs for center-neutral (- and +) directions of some axis.
  // This function adds those Inputs and also a FullAnalogSurface Input for each direction.
  // This is only needed when it's not known if the particular axis is neutral in the center
  //  or neutral on one of the extremes.
  // Some e.g. DInput devices expose a trigger across the full analog surface
  //  but we have no way of knowing this until the user actually maps the Input,
  //  so both center-neutral and full-surface Inputs need to be created in that case.
  void AddFullAnalogSurfaceInputs(Input* low, Input* high);

  void AddCombinedInput(std::string name, const std::pair<std::string, std::string>& inputs);

private:
  int m_id = 0;
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

  bool operator==(const Device* dev) const;

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
    Device::Input* input = nullptr;
    Clock::time_point press_time;
    std::optional<Clock::time_point> release_time;
    ControlState smoothness = 0;
  };

  Device::Input* FindInput(std::string_view name, const Device* def_dev) const;
  Device::Output* FindOutput(std::string_view name, const Device* def_dev) const;

  std::vector<std::shared_ptr<Device>> GetAllDevices() const;
  std::vector<std::string> GetAllDeviceStrings() const;
  bool HasDefaultDevice() const;
  std::string GetDefaultDeviceString() const;
  std::shared_ptr<Device> FindDevice(const DeviceQualifier& devq) const;

  bool HasConnectedDevice(const DeviceQualifier& qualifier) const;

  std::vector<InputDetection> DetectInput(const std::vector<std::string>& device_strings,
                                          std::chrono::milliseconds initial_wait,
                                          std::chrono::milliseconds confirmation_wait,
                                          std::chrono::milliseconds maximum_wait) const;

  std::recursive_mutex& GetDevicesMutex() const { return m_devices_mutex; }

protected:
  // Exclusively needed when reading/writing the "m_devices" array.
  // Not needed when individually readring/writing a single device ptr.
  mutable std::recursive_mutex m_devices_mutex;
  std::vector<std::shared_ptr<Device>> m_devices;
};

class InputDetector
{
public:
  using Detection = DeviceContainer::InputDetection;
  using Results = std::vector<Detection>;

  InputDetector();
  ~InputDetector();

  void Start(const DeviceContainer& container, const std::vector<std::string>& device_strings);
  void Update(std::chrono::milliseconds initial_wait, std::chrono::milliseconds confirmation_wait,
              std::chrono::milliseconds maximum_wait);
  bool IsComplete() const;

  const Results& GetResults() const;

  // move-return'd to prevent copying.
  Results TakeResults();

private:
  struct Impl;

  Clock::time_point m_start_time;
  Results m_detections;
  std::unique_ptr<Impl> m_state;
};

}  // namespace Core
}  // namespace ciface
