#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT

#include "DirectInput.h"

#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace ciface
{
namespace DirectInput
{

void Init( std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ )
{
	IDirectInput8* idi8;
	if ( DI_OK != DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&idi8, NULL ) )
		return;

#ifdef CIFACE_USE_DIRECTINPUT_KBM
	InitKeyboardMouse( idi8, devices );
#endif
#ifdef CIFACE_USE_DIRECTINPUT_JOYSTICK
	InitJoystick( idi8, devices/*, hwnd*/ );
#endif

	idi8->Release();

}

}
}

#endif
