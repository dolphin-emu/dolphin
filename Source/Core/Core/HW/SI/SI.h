// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace MMIO
{
class Mapping;
}

namespace SerialInterface
{
class SerialInterfaceState
{
public:
  SerialInterfaceState();
  SerialInterfaceState(const SerialInterfaceState&) = delete;
  SerialInterfaceState(SerialInterfaceState&&) = delete;
  SerialInterfaceState& operator=(const SerialInterfaceState&) = delete;
  SerialInterfaceState& operator=(SerialInterfaceState&&) = delete;
  ~SerialInterfaceState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

class ISIDevice;
enum SIDevices : int;

// SI number of channels
enum
{
  MAX_SI_CHANNELS = 0x04
};

void Init();
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void ScheduleEvent(int device_number, s64 cycles_into_future, u64 userdata = 0);
void RemoveEvent(int device_number);

void UpdateDevices();

void RemoveDevice(int device_number);
void AddDevice(SIDevices device, int device_number);
void AddDevice(std::unique_ptr<ISIDevice> device);

void ChangeDevice(SIDevices device, int channel);

SIDevices GetDeviceType(int channel);

u32 GetPollXLines();

}  // namespace SerialInterface
