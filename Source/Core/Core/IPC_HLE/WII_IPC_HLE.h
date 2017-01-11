// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"

class IWII_IPC_HLE_Device;
class PointerWrap;

struct IPCCommandResult
{
  bool send_reply;
  u64 reply_delay_ticks;
};

enum IPCCommandType : u32
{
  IPC_CMD_OPEN = 1,
  IPC_CMD_CLOSE = 2,
  IPC_CMD_READ = 3,
  IPC_CMD_WRITE = 4,
  IPC_CMD_SEEK = 5,
  IPC_CMD_IOCTL = 6,
  IPC_CMD_IOCTLV = 7,
  // This is used for replies to commands.
  IPC_REPLY = 8,
};

namespace WII_IPC_HLE_Interface
{
// Init
void Init();

// Needs to be called after Reset(true) to recreate the device tree
void Reinit();

// Shutdown
void Shutdown();

// Reset
void Reset(bool hard = false);

// Do State
void DoState(PointerWrap& p);

// Set default content file
void SetDefaultContentFile(const std::string& file_name);
void ES_DIVerify(const std::vector<u8>& tmd);

void SDIO_EventNotify();

std::shared_ptr<IWII_IPC_HLE_Device> GetDeviceByName(const std::string& device_name);
std::shared_ptr<IWII_IPC_HLE_Device> AccessDeviceByID(u32 id);

// Update
void Update();

// Update Devices
void UpdateDevices();

void ExecuteCommand(u32 address);

void EnqueueRequest(u32 address);
void EnqueueReply(u32 address, int cycles_in_future = 0,
                  CoreTiming::FromThread from = CoreTiming::FromThread::CPU);
void EnqueueCommandAcknowledgement(u32 address, int cycles_in_future = 0);

}  // end of namespace WII_IPC_HLE_Interface
