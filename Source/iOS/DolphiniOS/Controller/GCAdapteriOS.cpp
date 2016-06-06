// Copyright 2014 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI.h"
#include "Core/HW/SystemTimers.h"

#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

// Global java_vm class
//extern JavaVM* g_java_vm;

namespace GCAdapter
{
static void Setup();
static void Reset();

static void ThreadFunc()
{
        
}
    
static void ScanThreadFunc()
{

}

static void Write()
{

}

static void Read()
{

}

void Init()
{

}

static void Setup()
{
    
}

static void Reset()
{
    
}

void Shutdown()
{

}

void StartScanThread()
{

}

void StopScanThread()
{
    
}

void Input(int chan, GCPadStatus* pad)
{
    printf("Input!\n");
}

void Output(int chan, u8 rumble_command)
{
    printf("Output!\n");
}

bool IsDetected() {
    return 1;
}
bool IsDriverDetected() { return true; }
bool DeviceConnected(int chan)
{
    return 1;
}

bool UseAdapter()
{
    return 1;

}

void ResetRumble()
{

}

void SetAdapterCallback(std::function<void(void)> func) { }

} // end of namespace GCAdapter
