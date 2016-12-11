// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <functional>
#include "Common/CommonTypes.h"

struct GCPadStatus;

namespace GCAdapter {
    enum ControllerTypes {
      CONTROLLER_NONE = 0,
      CONTROLLER_WIRED = 1,
      CONTROLLER_WIRELESS = 2
    };

    void Init();
    void ResetRumble();
    void Shutdown();
    void SetAdapterCallback(std::function<void(void)> func);
    void StartScanThread();
    void StopScanThread();
    GCPadStatus Input(const int chan);
    void Output(const int chan, const u8 rumble_command);
    bool IsDetected();
    bool IsDriverDetected();
    bool DeviceConnected(const int chan);
    bool UseAdapter();
}
