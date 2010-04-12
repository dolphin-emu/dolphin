#include "Xlib.h"

namespace ciface
{
namespace Xlib
{

void Init(std::vector<ControllerInterface::Device*>& devices, void* const hwnd)
{
	// mouse will be added to this, Keyboard class will be turned into KeyboardMouse
	// single device for combined keyboard/mouse, this will allow combinations like shift+click more easily
	devices.push_back(new Keyboard((Display*)hwnd));
}

Keyboard::Keyboard(Display* display) : m_display(display)
{
	memset(&m_state, 0, sizeof(m_state));

	int min_keycode, max_keycode;
	XDisplayKeycodes(m_display, &min_keycode, &max_keycode);
	
	for (int i = min_keycode; i <= max_keycode; ++i)
	{
		Key *temp_key = new Key(m_display, i);
		if (temp_key->m_keyname.length())
			inputs.push_back(temp_key);
		else
			delete temp_key;
	}
}

Keyboard::~Keyboard()
{
}


ControlState Keyboard::GetInputState(const ControllerInterface::Device::Input* const input)
{
	return ((Input*)input)->GetState(&m_state);
}

void Keyboard::SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state)
{

}

bool Keyboard::UpdateInput()
{
	XQueryKeymap(m_display, m_state.keyboard);

	// mouse stuff in here too

	return true;
}

bool Keyboard::UpdateOutput()
{
	return true;
}


std::string Keyboard::GetName() const
{
	return "Keyboard";
	//return "Keyboard Mouse";	// change to this later
}

std::string Keyboard::GetSource() const
{
	return "Xlib";
}

int Keyboard::GetId() const
{
	return 0;
}


Keyboard::Key::Key(Display* const display, KeyCode keycode)
  : m_display(display), m_keycode(keycode)
{
	int i = 0;
	KeySym keysym = 0;
	do
	{
		keysym = XKeycodeToKeysym(m_display, keycode, i);
		i++;
	}
	while (keysym == NoSymbol && i < 8);

	// 0x0110ffff is the top of the unicode character range according to keysymdef.h
	// although it is probably more than we need.
	if (keysym == NoSymbol || keysym > 0x0110ffff)
		m_keyname = std::string();
	else
		m_keyname = std::string(XKeysymToString(keysym));
}

ControlState Keyboard::Key::GetState(const State* const state)
{
	return (state->keyboard[m_keycode/8] & (1 << (m_keycode%8))) != 0;
}

std::string Keyboard::Key::GetName() const
{
	return m_keyname;
}

}
}
