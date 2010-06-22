#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT_MOUSE

#include "DirectInputMouse.h"
#include "DirectInput.h"

// TODO: maybe add a ClearInputState function to this device

	// (lower would be more sensitive) user can lower sensitivity by setting range
	// seems decent here ( at 8 ), I dont think anyone would need more sensitive than this
	// and user can lower it much farther than they would want to with the range
#define MOUSE_AXIS_SENSITIVITY		8

	// if input hasn't been received for this many ms, mouse input will be skipped
	// otherwise it is just some crazy value
#define DROP_INPUT_TIME				250

namespace ciface
{
namespace DirectInput
{

void InitMouse( IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices )
{
	LPDIRECTINPUTDEVICE8 mo_device;

	unsigned int mo_count = 0;

	std::list<DIDEVICEINSTANCE> mice;
	idi8->EnumDevices(DI8DEVCLASS_POINTER, DIEnumDevicesCallback, (LPVOID)&mice, DIEDFL_ATTACHEDONLY);

	// add other keyboard devices
	std::list<DIDEVICEINSTANCE>::iterator
		i = mice.begin(),
		e = mice.end();
	for (; i!=e; ++i)
	{
		if (SUCCEEDED(idi8->CreateDevice(i->guidInstance, &mo_device, NULL)))
		{
			if (SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)))
			if (SUCCEEDED(mo_device->SetCooperativeLevel(NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
			{
				devices.push_back(new Mouse(mo_device, mo_count++));
				return;
			}
		}
		mo_device->Release();
	}
}

Mouse::~Mouse()
{
	// mouse
	m_mo_device->Unacquire();
	m_mo_device->Release();
}

Mouse::Mouse(const LPDIRECTINPUTDEVICE8 mo_device, const int index)
	: m_mo_device(mo_device)
	, m_index(index)
{
	m_mo_device->Acquire();

	m_last_update = GetTickCount();
	ZeroMemory(&m_state_in, sizeof(m_state_in));

	// MOUSE
	// get caps
	DIDEVCAPS mouse_caps;
	ZeroMemory( &mouse_caps, sizeof(mouse_caps) );
	mouse_caps.dwSize = sizeof(mouse_caps);
	m_mo_device->GetCapabilities(&mouse_caps);
	// mouse buttons
	for (unsigned int i = 0; i < mouse_caps.dwButtons; ++i)
		AddInput(new Button(i));
	// mouse axes
	for (unsigned int i = 0; i < mouse_caps.dwAxes; ++i)
	{
		// each axis gets a negative and a positive input instance associated with it
		AddInput(new Axis(i, (2==i) ? -1 : -MOUSE_AXIS_SENSITIVITY));
		AddInput(new Axis(i, -(2==i) ? 1 : MOUSE_AXIS_SENSITIVITY));
	}

}

bool Mouse::UpdateInput()
{
	DIMOUSESTATE2 tmp_mouse;
	HRESULT hr = m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);

	// if mouse position hasn't been updated in a short while, skip a dev state
	const DWORD cur_time = GetTickCount();
	if (cur_time - m_last_update > DROP_INPUT_TIME)
	{
		// set buttons/axes to zero
		// skip this input state
		ZeroMemory(&m_state_in, sizeof(m_state_in));
	}
	else if (SUCCEEDED(hr))
	{
		// need to smooth out the axes, otherwise it doesnt work for shit
		for (unsigned int i = 0; i < 3; ++i)
			((&m_state_in.lX)[i] += (&tmp_mouse.lX)[i]) /= 2;

		// copy over the buttons
		memcpy(m_state_in.rgbButtons, tmp_mouse.rgbButtons, sizeof(m_state_in.rgbButtons));
	}

	m_last_update = cur_time;

	if (DIERR_INPUTLOST == hr || DIERR_NOTACQUIRED == hr)
		m_mo_device->Acquire();

	return SUCCEEDED(hr);
}

bool Mouse::UpdateOutput()
{
	return true;
}

std::string Mouse::GetName() const
{
	return "Mouse";
}

int Mouse::GetId() const
{
	return m_index;
}

std::string Mouse::GetSource() const
{
	return DINPUT_SOURCE_NAME;
}

ControlState Mouse::GetInputState( const ControllerInterface::Device::Input* const input ) const
{
	return (((Input*)input)->GetState(&m_state_in));
}

void Mouse::SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state)
{
	return;
}

// names
std::string Mouse::Button::GetName() const
{
	return std::string("Button ") + char('0'+m_index);
}

std::string Mouse::Axis::GetName() const
{
	std::string tmpstr("Axis ");
	tmpstr += "XYZ"[m_index]; tmpstr += ( m_range>0 ? '+' : '-' );
	return tmpstr;
}

// get/set state
ControlState Mouse::Button::GetState(const DIMOUSESTATE2* const state) const
{
	return (state->rgbButtons[m_index] != 0);
}

ControlState Mouse::Axis::GetState(const DIMOUSESTATE2* const state) const
{
	return std::max(0.0f, ControlState((&state->lX)[m_index]) / m_range);
}

}
}

#endif
