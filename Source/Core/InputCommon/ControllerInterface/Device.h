// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
    virtual bool IsDetectable() { return true; }

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
  virtual void UpdateInput() {}

  // May be overridden to implement hotplug removal.
  // Currently handled on a per-backend basis but this could change.
  virtual bool IsValid() const { return true; }

  // (e.g. Xbox 360 controllers have controller number LEDs which should match the ID we use.)
  virtual std::optional<int> GetPreferredId() const;

  const std::vector<Input*>& Inputs() const { return m_inputs; }
  const std::vector<Output*>& Outputs() const { return m_outputs; }

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
  Device::Input* FindInput(std::string_view name, const Device* def_dev) const;
  Device::Output* FindOutput(std::string_view name, const Device* def_dev) const;

  std::vector<std::string> GetAllDeviceStrings() const;
  std::string GetDefaultDeviceString() const;
  std::shared_ptr<Device> FindDevice(const DeviceQualifier& devq) const;

  bool HasConnectedDevice(const DeviceQualifier& qualifier) const;

  std::pair<std::shared_ptr<Device>, Device::Input*>
  DetectInput(u32 wait_ms, const std::vector<std::string>& device_strings) const;

protected:
  mutable std::mutex m_devices_mutex;
  std::vector<std::shared_ptr<Device>> m_devices;
};
}  // namespace Core
}  // namespace ciface
