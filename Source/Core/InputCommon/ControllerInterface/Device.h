// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

// idk in case I wanted to change it to double or something, idk what's best
typedef double ControlState;

namespace ciface
{
namespace Core
{
// Forward declarations
class DeviceQualifier;

//
// Device
//
// A device class
//
class Device
{
public:
  class Input;
  class Output;

  using InputVector = std::vector<std::unique_ptr<Input>>;
  using OutputVector = std::vector<std::unique_ptr<Output>>;

  //
  // Control
  //
  // Control includes inputs and outputs
  //
  class Control  // input or output
  {
  public:
    virtual std::string GetName() const = 0;
    virtual ~Control() {}
    bool InputGateOn();

    virtual Input* ToInput() { return nullptr; }
    virtual Output* ToOutput() { return nullptr; }
  };

  //
  // Input
  //
  // An input on a device
  //
  class Input : public Control
  {
  public:
    // things like absolute axes/ absolute mouse position will override this
    virtual bool IsDetectable() { return true; }
    virtual ControlState GetState() const = 0;

    ControlState GetGatedState()
    {
      if (InputGateOn())
        return GetState();
      else
        return 0.0;
    }

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
    virtual ~Output() {}
    virtual void SetState(ControlState state) = 0;

    void SetGatedState(ControlState state)
    {
      if (InputGateOn())
        SetState(state);
    }

    Output* ToOutput() override { return this; }
  };

  virtual ~Device();

  virtual std::string GetName() const = 0;
  virtual int GetId() const = 0;
  virtual std::string GetSource() const = 0;
  virtual void UpdateInput() {}
  const InputVector& Inputs() const { return m_inputs; }
  const OutputVector& Outputs() const { return m_outputs; }
  Input* FindInput(const std::string& name) const;
  Output* FindOutput(const std::string& name) const;

protected:
  void AddInput(std::unique_ptr<Input> input);
  void AddOutput(std::unique_ptr<Output> output);

  class FullAnalogSurface : public Input
  {
  public:
    FullAnalogSurface(Input* low, Input* high) : m_low(*low), m_high(*high) {}
    ControlState GetState() const override
    {
      return (1 + m_high.GetState() - m_low.GetState()) / 2;
    }

    std::string GetName() const override { return m_low.GetName() + *m_high.GetName().rbegin(); }
  private:
    Input& m_low;
    Input& m_high;
  };

  void AddAnalogInputs(std::unique_ptr<Input> low, std::unique_ptr<Input> high)
  {
    Input* const low_ptr = low.get();
    Input* const high_ptr = high.get();

    AddInput(std::move(low));
    AddInput(std::move(high));

    AddInput(std::make_unique<FullAnalogSurface>(low_ptr, high_ptr));
    AddInput(std::make_unique<FullAnalogSurface>(high_ptr, low_ptr));
  }

private:
  InputVector m_inputs;
  OutputVector m_outputs;
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
  DeviceQualifier(const std::string& _source, const int _id, const std::string& _name)
      : source(_source), cid(_id), name(_name)
  {
  }
  void FromDevice(const Device* const dev);
  void FromString(const std::string& str);
  std::string ToString() const;
  bool operator==(const DeviceQualifier& devq) const;
  bool operator==(const Device* const dev) const;

  std::string source;
  int cid;
  std::string name;
};

class DeviceContainer
{
public:
  Device::Input* FindInput(const std::string& name, const Device* def_dev) const;
  Device::Output* FindOutput(const std::string& name, const Device* def_dev) const;

  std::vector<std::string> GetAllDeviceStrings() const;
  std::string GetDefaultDeviceString() const;
  std::shared_ptr<Device> FindDevice(const DeviceQualifier& devq) const;

protected:
  mutable std::mutex m_devices_mutex;
  std::vector<std::shared_ptr<Device>> m_devices;
};
}
}
