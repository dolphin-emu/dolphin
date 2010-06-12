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

	m_window = DefaultRootWindow(m_display);

	int min_keycode, max_keycode;
	XDisplayKeycodes(m_display, &min_keycode, &max_keycode);
	
	// Keyboard Keys
	for (int i = min_keycode; i <= max_keycode; ++i)
	{
		Key *temp_key = new Key(m_display, i);
		if (temp_key->m_keyname.length())
			inputs.push_back(temp_key);
		else
			delete temp_key;
	}

	// Mouse Buttons
	inputs.push_back(new Button(Button1Mask));
	inputs.push_back(new Button(Button2Mask));
	inputs.push_back(new Button(Button3Mask));
	inputs.push_back(new Button(Button4Mask));
	inputs.push_back(new Button(Button5Mask));
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

	int root_x, root_y, win_x, win_y;
	Window root, child;
	XQueryPointer(m_display, m_window, &root, &child, &root_x, &root_y, &win_x, &win_y, &m_state.buttons);

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
	
	// Convert to upper case for the keyname
	if (keysym >= 97 && keysym <= 122)
		keysym = keysym - 32;

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

ControlState Keyboard::Button::GetState(const State* const state)
{
	return ((state->buttons & m_index) > 0);
}

std::string Keyboard::Key::GetName() const
{
	return m_keyname;
}

std::string Keyboard::Button::GetName() const
{
	char button = '0';
	switch (m_index)
	{
		case Button1Mask: button = '1'; break;
		case Button2Mask: button = '2'; break;
		case Button3Mask: button = '3'; break;
		case Button4Mask: button = '4'; break;
		case Button5Mask: button = '5'; break;
	}

	return std::string("Button ") + button;
}

}
}
