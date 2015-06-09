// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define DINPUT_SOURCE_NAME "DInput"

#include <list>
#include <windows.h>

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/DInput/DInput8.h"

namespace ciface
{
namespace DInput
{

//BOOL CALLBACK DIEnumEffectsCallback(LPCDIEFFECTINFO pdei, LPVOID pvRef);
BOOL CALLBACK DIEnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
std::string GetDeviceName(const LPDIRECTINPUTDEVICE8 device);

void Init(std::vector<Core::Device*>& devices, HWND hwnd);

}
}
