#include "Xlib.h"

namespace ciface
{
namespace Xlib
{

void Init(std::vector<ControllerInterface::Device*>& devices, void* const hwnd)
{
	devices.push_back(new KeyboardMouse((Window)hwnd));
}

KeyboardMouse::KeyboardMouse(Window window) : m_window(window)
{
	memset(&m_state, 0, sizeof(m_state));

	m_display = XOpenDisplay(NULL);

	int min_keycode, max_keycode;
	XDisplayKeycodes(m_display, &min_keycode, &max_keycode);
	
	// Keyboard Keys
	for (int i = min_keycode; i <= max_keycode; ++i)
	{
		Key *temp_key = new Key(m_display, i);
		if (temp_key->m_keyname.length())
			AddInput(temp_key);
		else
			delete temp_key;
	}

	// Mouse Buttons
	AddInput(new Button(Button1Mask));
	AddInput(new Button(Button2Mask));
	AddInput(new Button(Button3Mask));
	AddInput(new Button(Button4Mask));
	AddInput(new Button(Button5Mask));

	// Mouse Cursor, X-/+ and Y-/+
	for (unsigned int i=0; i<4; ++i)
		AddInput(new Cursor(!!(i&2), !!(i&1)));
}

KeyboardMouse::~KeyboardMouse()
{
	XCloseDisplay(m_display);
}

ControlState KeyboardMouse::GetInputState(const ControllerInterface::Device::Input* const input) const
{
	return ((Input*)input)->GetState(&m_state);
}

void KeyboardMouse::SetOutputState(const ControllerInterface::Device::Output* const output, const ControlState state)
{

}

bool KeyboardMouse::UpdateInput()
{
	XQueryKeymap(m_display, m_state.keyboard);

	int root_x, root_y, win_x, win_y;
	Window root, child;
	XQueryPointer(m_display, m_window, &root, &child, &root_x, &root_y, &win_x, &win_y, &m_state.buttons);

	// update mouse cursor
	XWindowAttributes win_attribs;
	XGetWindowAttributes(m_display, m_window, &win_attribs);

	// the mouse position as a range from -1 to 1
	m_state.cursor.x = (float)win_x / (float)win_attribs.width * 2 - 1;
	m_state.cursor.y = (float)win_y / (float)win_attribs.height * 2 - 1;

	return true;
}

bool KeyboardMouse::UpdateOutput()
{
	return true;
}


std::string KeyboardMouse::GetName() const
{
	return "Keyboard Mouse";
}

std::string KeyboardMouse::GetSource() const
{
	return "Xlib";
}

int KeyboardMouse::GetId() const
{
	return 0;
}


KeyboardMouse::Key::Key(Display* const display, KeyCode keycode)
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
		keysym -= 32;

	// 0x0110ffff is the top of the unicode character range according
	// to keysymdef.h although it is probably more than we need.
	if (keysym == NoSymbol || keysym > 0x0110ffff ||
		XKeysymToString(keysym) == NULL)
		m_keyname = std::string();
	else
		m_keyname = std::string(XKeysymToString(keysym));
}

ControlState KeyboardMouse::Key::GetState(const State* const state) const
{
	KeyCode shift = XKeysymToKeycode(m_display, XK_Shift_L);
	return (state->keyboard[m_keycode/8] & (1 << (m_keycode%8))) != 0
			&& (state->keyboard[shift/8] & (1 << (shift%8))) == 0;
}

ControlState KeyboardMouse::Button::GetState(const State* const state) const
{
	return ((state->buttons & m_index) > 0);
}

ControlState KeyboardMouse::Cursor::GetState(const State* const state) const
{
	return std::max(0.0f, ControlState((&state->cursor.x)[m_index]) / (m_positive ? 1.0f : -1.0f));
}

std::string KeyboardMouse::Key::GetName() const
{
	return m_keyname;
}

std::string KeyboardMouse::Cursor::GetName() const
{
	static char tmpstr[] = "Cursor ..";
	tmpstr[7] = (char)('X' + m_index);
	tmpstr[8] = (m_positive ? '+' : '-');
	return tmpstr;
}

std::string KeyboardMouse::Button::GetName() const
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

	static char tmpstr[] = "Click .";
	tmpstr[6] = button;
	return tmpstr;
}

}
}
