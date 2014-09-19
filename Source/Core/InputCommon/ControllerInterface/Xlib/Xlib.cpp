#include <cstring>
#include <X11/XKBlib.h>

#include "InputCommon/ControllerInterface/Xlib/Xlib.h"

namespace ciface
{
namespace Xlib
{

void Init(std::vector<Core::Device*>& devices, void* const hwnd)
{
	devices.push_back(new KeyboardMouse((Window)hwnd));
}

KeyboardMouse::KeyboardMouse(Window window) : m_window(window)
{
	memset(&m_state, 0, sizeof(m_state));

	m_display = XOpenDisplay(nullptr);

	int min_keycode, max_keycode;
	XDisplayKeycodes(m_display, &min_keycode, &max_keycode);

	// Keyboard Keys
	for (int i = min_keycode; i <= max_keycode; ++i)
	{
		Key *temp_key = new Key(m_display, i, m_state.keyboard);
		if (temp_key->m_keyname.length())
			AddInput(temp_key);
		else
			delete temp_key;
	}

	// Mouse Buttons
	AddInput(new Button(Button1Mask, m_state.buttons));
	AddInput(new Button(Button2Mask, m_state.buttons));
	AddInput(new Button(Button3Mask, m_state.buttons));
	AddInput(new Button(Button4Mask, m_state.buttons));
	AddInput(new Button(Button5Mask, m_state.buttons));

	// Mouse Cursor, X-/+ and Y-/+
	for (int i = 0; i != 4; ++i)
		AddInput(new Cursor(!!(i & 2), !!(i & 1), (&m_state.cursor.x)[!!(i & 2)]));
}

KeyboardMouse::~KeyboardMouse()
{
	XCloseDisplay(m_display);
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

KeyboardMouse::Key::Key(Display* const display, KeyCode keycode, const char* keyboard)
	: m_display(display), m_keyboard(keyboard), m_keycode(keycode)
{
	int i = 0;
	KeySym keysym = 0;
	do
	{
		keysym = XkbKeycodeToKeysym(m_display, keycode, i, 0);
		i++;
	}
	while (keysym == NoSymbol && i < 8);

	// Convert to upper case for the keyname
	if (keysym >= 97 && keysym <= 122)
		keysym -= 32;

	// 0x0110ffff is the top of the unicode character range according
	// to keysymdef.h although it is probably more than we need.
	if (keysym == NoSymbol || keysym > 0x0110ffff ||
		XKeysymToString(keysym) == nullptr)
		m_keyname = std::string();
	else
		m_keyname = std::string(XKeysymToString(keysym));
}

ControlState KeyboardMouse::Key::GetState() const
{
	return (m_keyboard[m_keycode / 8] & (1 << (m_keycode % 8))) != 0;
}

ControlState KeyboardMouse::Button::GetState() const
{
	return ((m_buttons & m_index) != 0);
}

ControlState KeyboardMouse::Cursor::GetState() const
{
	return std::max(0.0f, m_cursor / (m_positive ? 1.0f : -1.0f));
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
