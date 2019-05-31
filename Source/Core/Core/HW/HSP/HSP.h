// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace HSP
{
class IHSPDevice;
enum HSPDevices : int;

void Init();
void Shutdown();

u64 Read(u32 address);
void Write(u32 address, u64 value);

void DoState(PointerWrap& p);

void RemoveDevice();
void AddDevice(std::unique_ptr<IHSPDevice> device);
void AddDevice(HSPDevices device);
}  // namespace HSP
