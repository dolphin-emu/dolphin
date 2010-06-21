#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT

#include "DirectInput.h"

#include <StringUtil.h>

#ifdef CIFACE_USE_DIRECTINPUT_JOYSTICK
	#include "DirectInputJoystick.h"
#endif
#ifdef CIFACE_USE_DIRECTINPUT_KEYBOARD
	#include "DirectInputKeyboard.h"
#endif
#ifdef CIFACE_USE_DIRECTINPUT_MOUSE
	#include "DirectInputMouse.h"
#endif

#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace ciface
{
namespace DirectInput
{

//BOOL CALLBACK DIEnumEffectsCallback(LPCDIEFFECTINFO pdei, LPVOID pvRef)
//{
//	((std::list<DIEFFECTINFO>*)pvRef)->push_back(*pdei);
//	return DIENUM_CONTINUE;
//}

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
	std::string out;

	DIPROPSTRING str;
	ZeroMemory(&str, sizeof(str));
	str.diph.dwSize = sizeof(str);
	str.diph.dwHeaderSize = sizeof(str.diph);
	str.diph.dwHow = DIPH_DEVICE;

	if (SUCCEEDED(device->GetProperty(DIPROP_PRODUCTNAME, &str.diph)))
	{
		const int size = WideCharToMultiByte(CP_ACP, 0, str.wsz, -1, NULL, 0, NULL, NULL);
		char* const data = new char[size];
		if (size == WideCharToMultiByte(CP_ACP, 0, str.wsz, -1, data, size, NULL, NULL))
			out.assign(data);
		delete[] data;
	}

	return StripSpaces(out);
}

void Init( std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ )
{
	IDirectInput8* idi8;
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&idi8, NULL)))
		return;

#ifdef CIFACE_USE_DIRECTINPUT_KEYBOARD
	InitKeyboard(idi8, devices);
#endif
#ifdef CIFACE_USE_DIRECTINPUT_MOUSE
	InitMouse(idi8, devices);
#endif
#ifdef CIFACE_USE_DIRECTINPUT_JOYSTICK
	InitJoystick(idi8, devices/*, hwnd*/);
#endif

	idi8->Release();

}

}
}

#endif
