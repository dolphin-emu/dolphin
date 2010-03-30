#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT_KBM

#include "DirectInputKeyboardMouse.h"

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

struct
{
	const BYTE			code;
	const char* const	name;
} named_keys[] =
{
#include "NamedKeys.h"
};

struct
{
	const BYTE			code;
	const char* const	name;
} named_lights[] =
{
	{ VK_NUMLOCK, "NUM LOCK" },
	{ VK_CAPITAL, "CAPS LOCK" },
	{ VK_SCROLL, "SCROLL LOCK" }
};

void InitKeyboardMouse( IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices )
{
	// mouse and keyboard are a combined device, to allow shift+click and stuff
	// if thats dumb, i will make a VirtualDevice class that just uses ranges of inputs/outputs from other devices
	// so there can be a separated Keyboard and mouse, as well as combined KeyboardMouse

	// TODO: this has potential to not release devices if set datafmt or cooplevel fails

	LPDIRECTINPUTDEVICE8 kb_device;
	LPDIRECTINPUTDEVICE8 mo_device;

	if ( DI_OK == idi8->CreateDevice( GUID_SysKeyboard, &kb_device, NULL ) )
	if ( DI_OK == kb_device->SetDataFormat( &c_dfDIKeyboard ) )
	if ( DI_OK == kb_device->SetCooperativeLevel( NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE) )
	if ( DI_OK == kb_device->Acquire() )
	{

		if ( DI_OK == idi8->CreateDevice( GUID_SysMouse, &mo_device, NULL ) )
		if ( DI_OK == mo_device->SetDataFormat( &c_dfDIMouse2 ) )
		if ( DI_OK == mo_device->SetCooperativeLevel( NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE) )
		if ( DI_OK == mo_device->Acquire() )
		{
			devices.push_back( new KeyboardMouse( kb_device, mo_device ) );
			return;
		}
		else
			goto release_mouse;

		goto unacquire_kb;
	}
	else
		goto release_kb;

release_mouse:
	mo_device->Release();
unacquire_kb:
	kb_device->Unacquire();
release_kb:
	kb_device->Release();
}

KeyboardMouse::~KeyboardMouse()
{
	// kb
	m_kb_device->Unacquire();
	m_kb_device->Release();
	// mouse
	m_mo_device->Unacquire();
	m_mo_device->Release();
}

KeyboardMouse::KeyboardMouse( const LPDIRECTINPUTDEVICE8 kb_device, const LPDIRECTINPUTDEVICE8 mo_device )
	: m_kb_device(kb_device)
	, m_mo_device(mo_device)
{
	m_last_update = wxGetLocalTimeMillis();

	ZeroMemory( &m_state_in, sizeof(m_state_in) );
	ZeroMemory( m_state_out, sizeof(m_state_out) );
	ZeroMemory( &m_current_state_out, sizeof(m_current_state_out) );

	// KEYBOARD
	// add keys
	for ( unsigned int i = 0; i < sizeof(named_keys)/sizeof(*named_keys); ++i )
		inputs.push_back( new Key( i ) );
	// add lights
	for ( unsigned int i = 0; i < sizeof(named_lights)/sizeof(*named_lights); ++i )
		outputs.push_back( new Light( i ) );

	// MOUSE
	// get caps
	DIDEVCAPS mouse_caps;
	ZeroMemory( &mouse_caps, sizeof(mouse_caps) );
	mouse_caps.dwSize = sizeof(mouse_caps);
	m_mo_device->GetCapabilities(&mouse_caps);
	// mouse buttons
	for ( unsigned int i = 0; i < mouse_caps.dwButtons; ++i )
		inputs.push_back( new Button( i ) );
	// mouse axes
	for ( unsigned int i = 0; i < mouse_caps.dwAxes; ++i )
	{
		// each axis gets a negative and a positive input instance associated with it
		inputs.push_back( new Axis( i, (2==i) ? -1 : -MOUSE_AXIS_SENSITIVITY ) );
		inputs.push_back( new Axis( i, -(2==i) ? 1 : MOUSE_AXIS_SENSITIVITY ) );
	}

}

bool KeyboardMouse::UpdateInput()
{
	DIMOUSESTATE2 tmp_mouse;

	// if mouse position hasn't been updated in a short while, skip a dev state
	wxLongLong cur_time = wxGetLocalTimeMillis();
	if ( cur_time - m_last_update > DROP_INPUT_TIME )
	{
		// set axes to zero
		ZeroMemory( &m_state_in.mouse, sizeof(m_state_in.mouse) );
		// skip this input state
		m_mo_device->GetDeviceState( sizeof(tmp_mouse), &tmp_mouse );
	}

	m_last_update = cur_time;

	if ( DI_OK == m_kb_device->GetDeviceState( sizeof(m_state_in.keyboard), &m_state_in.keyboard )
		&& DI_OK == m_mo_device->GetDeviceState( sizeof(tmp_mouse), &tmp_mouse ) )
	{
		// need to smooth out the axes, otherwise it doesnt work for shit
		for ( unsigned int i = 0; i < 3; ++i )
			((&m_state_in.mouse.lX)[i] += (&tmp_mouse.lX)[i]) /= 2;

		// copy over the buttons
		memcpy( m_state_in.mouse.rgbButtons, tmp_mouse.rgbButtons, sizeof(m_state_in.mouse.rgbButtons) );
		return true;
	}
	else
		return false;
}

bool KeyboardMouse::UpdateOutput()
{
	class KInput : public INPUT
	{
	public:
		KInput( const unsigned char key, const bool up = false )
		{
			memset( this, 0, sizeof(*this) );
			type = INPUT_KEYBOARD;
			ki.wVk = key;
			if (up) ki.dwFlags = KEYEVENTF_KEYUP;
		}
	};

	std::vector< KInput > kbinputs;
	for ( unsigned int i = 0; i < sizeof(m_state_out)/sizeof(*m_state_out); ++i )
	{
		bool want_on = false;
		if ( m_state_out[i] )
			want_on = m_state_out[i] > wxGetLocalTimeMillis() % 255 ;	// light should flash when output is 0.5

		// lights are set to their original state when output is zero
		if ( want_on ^ m_current_state_out[i] )
		{
			kbinputs.push_back( KInput( named_lights[i].code ) );		// press
			kbinputs.push_back( KInput( named_lights[i].code, true ) ); // release

			m_current_state_out[i] ^= 1;
		}
	}

	return ( kbinputs.size() == SendInput( (UINT)kbinputs.size(), &kbinputs[0], sizeof( kbinputs[0] ) ) );
}

std::string KeyboardMouse::GetName() const
{
	return "Keyboard Mouse";
}

int KeyboardMouse::GetId() const
{
	// should this be -1, idk
	return 0;
}

std::string KeyboardMouse::GetSource() const
{
	return "DirectInput";
}

ControlState KeyboardMouse::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ( ((Input*)input)->GetState( &m_state_in ) );
}

void KeyboardMouse::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
{
	((Output*)output)->SetState( state, m_state_out );
}

// names
std::string KeyboardMouse::Key::GetName() const
{
	return named_keys[m_index].name;
}

std::string KeyboardMouse::Button::GetName() const
{
	return std::string("Button ") + char('0'+m_index);
}

std::string KeyboardMouse::Axis::GetName() const
{
	std::string tmpstr("Mouse ");
	tmpstr += "XYZ"[m_index]; tmpstr += ( m_range>0 ? '+' : '-' );
	return tmpstr;
}

std::string KeyboardMouse::Light::GetName() const
{
	return named_lights[ m_index ].name;
}

// get/set state
ControlState KeyboardMouse::Key::GetState( const State* const state )
{
	return ( state->keyboard[named_keys[m_index].code] > 0 );
}

ControlState KeyboardMouse::Button::GetState( const State* const state )
{
	return ( state->mouse.rgbButtons[m_index] > 0 );
}

ControlState KeyboardMouse::Axis::GetState( const State* const state )
{
	return std::max( 0.0f, ControlState((&state->mouse.lX)[m_index]) / m_range );
}

void KeyboardMouse::Light::SetState( const ControlState state, unsigned char* const state_out )
{
	state_out[ m_index ] = state * 255;
}

}
}

#endif
