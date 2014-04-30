// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/StringUtil.h"

#include "InputCommon/ControllerInterface/DInput/DInput.h"

#include "InputCommon/ControllerInterface/DInput/DInputJoystick.h"
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace ciface
{
namespace DInput
{

BOOL CALLBACK DIEnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	((std::list<DIDEVICEOBJECTINSTANCE>*)pvRef)->push_back(*lpddoi);
	return DIENUM_CONTINUE;
}

BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	((std::list<DIDEVICEINSTANCE>*)pvRef)->push_back(*lpddi);
	return DIENUM_CONTINUE;
}

std::string GetDeviceName(const LPDIRECTINPUTDEVICE8 device)
{
	DIPROPSTRING str = {};
	str.diph.dwSize = sizeof(str);
	str.diph.dwHeaderSize = sizeof(str.diph);
	str.diph.dwHow = DIPH_DEVICE;

	std::string result;
	if (SUCCEEDED(device->GetProperty(DIPROP_PRODUCTNAME, &str.diph)))
	{
		result = StripSpaces(UTF16ToUTF8(str.wsz));
	}

	return result;
}

HRESULT SetDeviceProperty(const LPDIRECTINPUTDEVICE8 device, const GUID& property_guid, DWORD data)
{
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof(dipdw);
	dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = data;
	return device->SetProperty(property_guid, &dipdw.diph);
}

std::string DIJOYSTATE_AxisName(size_t index)
{
	std::ostringstream ss;
	// axis
	if (index < 6)
	{
		ss << "Axis " << (char)('X' + (index % 3));
		if (index > 2)
			ss << 'r';
	}
	// slider
	else
	{
		ss << "Slider " << (int)(index - 6);
	}

	return ss.str();
}

void Init(std::vector<Core::Device*>& devices, HWND hwnd)
{
	IDirectInput8* idi8;
	if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&idi8, nullptr)))
		return;

	InitKeyboardMouse(idi8, devices, hwnd);
	InitJoystick(idi8, devices, hwnd);

	idi8->Release();

}

}
}
