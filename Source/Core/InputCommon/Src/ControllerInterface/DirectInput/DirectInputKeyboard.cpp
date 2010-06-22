#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT_KEYBOARD

#include "DirectInputKeyboard.h"
#include "DirectInput.h"

// TODO: maybe add a ClearInputState function to this device

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

void InitKeyboard(IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices)
{
	LPDIRECTINPUTDEVICE8 kb_device;

	unsigned int kb_count = 0;

	std::list<DIDEVICEINSTANCE> keyboards;
	idi8->EnumDevices(DI8DEVCLASS_KEYBOARD, DIEnumDevicesCallback, (LPVOID)&keyboards, DIEDFL_ATTACHEDONLY);

	// add other keyboard devices
	std::list<DIDEVICEINSTANCE>::iterator
		i = keyboards.begin(),
		e = keyboards.end();
	for (; i!=e; ++i)
	{
		if (SUCCEEDED(idi8->CreateDevice(i->guidInstance, &kb_device, NULL)))
		{
			if (SUCCEEDED(kb_device->SetDataFormat(&c_dfDIKeyboard)))
			if (SUCCEEDED(kb_device->SetCooperativeLevel(NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
			{
				devices.push_back(new Keyboard(kb_device, kb_count++));
				return;
			}
		}
		kb_device->Release();
	}
}

Keyboard::~Keyboard()
{
	// kb
	m_kb_device->Unacquire();
	m_kb_device->Release();

}

Keyboard::Keyboard(const LPDIRECTINPUTDEVICE8 kb_device, const int index)
	: m_kb_device(kb_device)
	, m_index(index)
{
	m_kb_device->Acquire();

	ZeroMemory(&m_state_in, sizeof(m_state_in));
	ZeroMemory(m_state_out, sizeof(m_state_out));
	ZeroMemory(&m_current_state_out, sizeof(m_current_state_out));

	// KEYBOARD
	// add keys
	for (unsigned int i = 0; i < sizeof(named_keys)/sizeof(*named_keys); ++i)
		AddInput(new Key(i));
	// add lights
	for (unsigned int i = 0; i < sizeof(named_lights)/sizeof(*named_lights); ++i)
		AddOutput(new Light(i));
}

bool Keyboard::UpdateInput()
{
	HRESULT hr = m_kb_device->GetDeviceState(sizeof(m_state_in), m_state_in);

	if (DIERR_INPUTLOST == hr || DIERR_NOTACQUIRED == hr)
		m_kb_device->Acquire();

	return SUCCEEDED(hr);
}

bool Keyboard::UpdateOutput()
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

	if (kbinputs.size())
		return ( kbinputs.size() == SendInput( (UINT)kbinputs.size(), &kbinputs[0], sizeof( kbinputs[0] ) ) );
	else
		return true;
}

std::string Keyboard::GetName() const
{
	return "Keyboard";
}

int Keyboard::GetId() const
{
	return m_index;
}

std::string Keyboard::GetSource() const
{
	return DINPUT_SOURCE_NAME;
}

ControlState Keyboard::GetInputState(const ControllerInterface::Device::Input* const input) const
{
	return (((Input*)input)->GetState(m_state_in));
}

void Keyboard::SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state)
{
	((Output*)output)->SetState(state, m_state_out);
}

// names
std::string Keyboard::Key::GetName() const
{
	return named_keys[m_index].name;
}

std::string Keyboard::Light::GetName() const
{
	return named_lights[ m_index ].name;
}

// get/set state
ControlState Keyboard::Key::GetState(const BYTE keys[]) const
{
	return (keys[named_keys[m_index].code] != 0);
}

void Keyboard::Light::SetState( const ControlState state, unsigned char* const state_out )
{
	state_out[m_index] = state * 255;
}

}
}

#endif
