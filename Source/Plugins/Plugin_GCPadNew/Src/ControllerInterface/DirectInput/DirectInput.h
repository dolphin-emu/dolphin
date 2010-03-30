#ifndef _CIFACE_DIRECTINPUT_H_
#define _CIFACE_DIRECTINPUT_H_

#include "../ControllerInterface.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#ifdef CIFACE_USE_DIRECTINPUT_JOYSTICK
	#include "DirectInputJoystick.h"
#endif
#ifdef CIFACE_USE_DIRECTINPUT_KBM
	#include "DirectInputKeyboardMouse.h"
#endif

namespace ciface
{
namespace DirectInput
{

void Init( std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ );

}
}

#endif
