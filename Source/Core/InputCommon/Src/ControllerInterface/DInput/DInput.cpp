
#include "DInput.h"

#include "StringUtil.h"

#include "DInputJoystick.h"
#include "DInputKeyboardMouse.h"

#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace ciface
{
namespace DInput
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

void Init(std::vector<Core::Device*>& devices, HWND hwnd)
{
	IDirectInput8* idi8;
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&idi8, NULL)))
		return;

	InitKeyboardMouse(idi8, devices, hwnd);
	InitJoystick(idi8, devices, hwnd);

	idi8->Release();

}

}
}
