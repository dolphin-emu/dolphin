#ifndef _CIFACE_DIRECTINPUT_H_
#define _CIFACE_DIRECTINPUT_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#include <list>

namespace ciface
{
namespace DirectInput
{

//BOOL CALLBACK DIEnumEffectsCallback(LPCDIEFFECTINFO pdei, LPVOID pvRef);
BOOL CALLBACK DIEnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
std::string GetDeviceName(const LPDIRECTINPUTDEVICE8 device);

void Init( std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ );

}
}

#endif
