// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define DINPUT_SOURCE_NAME "DInput"

#include <windows.h>
#include <list>

// Disable warning C4265 in wrl/client.h:
//   'Microsoft::WRL::Details::RemoveIUnknownBase<T>': class has virtual functions,
//   but destructor is not virtual
#pragma warning(push)
#pragma warning(disable : 4265)
#include <wrl/client.h>
#pragma warning(pop)

#include "InputCommon/ControllerInterface/DInput/DInput8.h"

namespace ciface
{
namespace DInput
{
using Microsoft::WRL::ComPtr;

// BOOL CALLBACK DIEnumEffectsCallback(LPCDIEFFECTINFO pdei, LPVOID pvRef);
BOOL CALLBACK DIEnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
std::string GetDeviceName(const LPDIRECTINPUTDEVICE8 device);

void PopulateDevices(HWND hwnd);
}
}
