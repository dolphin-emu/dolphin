// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define DINPUT_SOURCE_NAME "DInput"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dinput.h>
#include <list>
#include <windows.h>

#include "InputCommon/ControllerInterface/Device.h"

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
