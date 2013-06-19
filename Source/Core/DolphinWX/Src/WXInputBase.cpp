// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "WXInputBase.h"

namespace InputCommon
{

const wxString WXKeyToString(int keycode)
{
	switch (keycode)
	{
		case WXK_BACK:				return _("Back");
		case WXK_TAB:				return _("Tab");
		case WXK_RETURN:			return _("Return");
		case WXK_ESCAPE:			return _("Escape");
		case WXK_SPACE:				return _("Space");
		case WXK_DELETE:			return _("Delete");

		// Undocumented wx keycodes
		case 167:				return _("Paragraph");
		case 177:				return _("Plus-Minus");

		case WXK_START:				return _("Start");
		case WXK_LBUTTON:			return _("L Button");
		case WXK_RBUTTON:			return _("R Button");
		case WXK_CANCEL:			return _("Cancel");
		case WXK_MBUTTON:			return _("M Button");
		case WXK_CLEAR:				return _("Clear");
		case WXK_SHIFT:				return wxT("Shift");
		case WXK_ALT:				return wxT("Alt");
		case WXK_RAW_CONTROL:		return _("Control");
#ifdef __WXOSX__
		case WXK_COMMAND:			return _("Command");
#endif
		case WXK_MENU:				return _("Menu");
		case WXK_PAUSE:				return _("Pause");
		case WXK_CAPITAL:			return _("Caps Lock");
		case WXK_END:				return _("End");
		case WXK_HOME:				return _("Home");
		case WXK_LEFT:				return _("Left");
		case WXK_UP:				return _("Up");
		case WXK_RIGHT:				return _("Right");
		case WXK_DOWN:				return _("Down");
		case WXK_SELECT:			return _("Select");
		case WXK_PRINT:				return _("Print");
		case WXK_EXECUTE:			return _("Execute");
		case WXK_SNAPSHOT:			return _("Snapshot");
		case WXK_INSERT:			return _("Insert");
		case WXK_HELP:				return _("Help");
		case WXK_NUMPAD0:			return wxT("NP 0");
		case WXK_NUMPAD1:			return wxT("NP 1");
		case WXK_NUMPAD2:			return wxT("NP 2");
		case WXK_NUMPAD3:			return wxT("NP 3");
		case WXK_NUMPAD4:			return wxT("NP 4");
		case WXK_NUMPAD5:			return wxT("NP 5");
		case WXK_NUMPAD6:			return wxT("NP 6");
		case WXK_NUMPAD7:			return wxT("NP 7");
		case WXK_NUMPAD8:			return wxT("NP 8");
		case WXK_NUMPAD9:			return wxT("NP 9");
		case WXK_MULTIPLY:			return _("Multiply");
		case WXK_ADD:				return _("Add");
		case WXK_SEPARATOR:			return _("Separator");
		case WXK_SUBTRACT:			return _("Subtract");
		case WXK_DECIMAL:			return _("Decimal");
		case WXK_DIVIDE:			return _("Divide");
		case WXK_F1:				return wxT("F1");
		case WXK_F2:				return wxT("F2");
		case WXK_F3:				return wxT("F3");
		case WXK_F4:				return wxT("F4");
		case WXK_F5:				return wxT("F5");
		case WXK_F6:				return wxT("F6");
		case WXK_F7:				return wxT("F7");
		case WXK_F8:				return wxT("F8");
		case WXK_F9:				return wxT("F9");
		case WXK_F10:				return wxT("F10");
		case WXK_F11:				return wxT("F11");
		case WXK_F12:				return wxT("F12");
		case WXK_F13:				return wxT("F13");
		case WXK_F14:				return wxT("F14");
		case WXK_F15:				return wxT("F15");
		case WXK_F16:				return wxT("F16");
		case WXK_F17:				return wxT("F17");
		case WXK_F18:				return wxT("F19");
		case WXK_F19:				return wxT("F20");
		case WXK_F20:				return wxT("F21");
		case WXK_F21:				return wxT("F22");
		case WXK_F22:				return wxT("F23");
		case WXK_F23:				return wxT("F24");
		case WXK_F24:				return wxT("F25");
		case WXK_NUMLOCK:			return _("Num Lock");
		case WXK_SCROLL:			return _("Scroll Lock");
		case WXK_PAGEUP:			return _("Page Up");
		case WXK_PAGEDOWN:			return _("Page Down");
		case WXK_NUMPAD_SPACE:			return _("NP Space");
		case WXK_NUMPAD_TAB:			return _("NP Tab");
		case WXK_NUMPAD_ENTER:			return _("NP Enter");
		case WXK_NUMPAD_F1:			return wxT("NP F1");
		case WXK_NUMPAD_F2:			return wxT("NP F2");
		case WXK_NUMPAD_F3:			return wxT("NP F3");
		case WXK_NUMPAD_F4:			return wxT("NP F4");
		case WXK_NUMPAD_HOME:			return _("NP Home");
		case WXK_NUMPAD_LEFT:			return _("NP Left");
		case WXK_NUMPAD_UP:			return _("NP Up");
		case WXK_NUMPAD_RIGHT:			return _("NP Right");
		case WXK_NUMPAD_DOWN:			return _("NP Down");
		case WXK_NUMPAD_PAGEUP:			return _("NP Page Up");
		case WXK_NUMPAD_PAGEDOWN:		return _("NP Page Down");
		case WXK_NUMPAD_END:			return _("NP End");
		case WXK_NUMPAD_BEGIN:			return _("NP Begin");
		case WXK_NUMPAD_INSERT:			return _("NP Insert");
		case WXK_NUMPAD_DELETE:			return _("NP Delete");
		case WXK_NUMPAD_EQUAL:			return _("NP Equal");
		case WXK_NUMPAD_MULTIPLY:		return _("NP Multiply");
		case WXK_NUMPAD_ADD:			return _("NP Add");
		case WXK_NUMPAD_SEPARATOR:		return _("NP Separator");
		case WXK_NUMPAD_SUBTRACT:		return _("NP Subtract");
		case WXK_NUMPAD_DECIMAL:		return _("NP Decimal");
		case WXK_NUMPAD_DIVIDE:			return _("NP Divide");
		case WXK_WINDOWS_LEFT:			return _("Windows Left");
		case WXK_WINDOWS_RIGHT:			return _("Windows Right");
		case WXK_WINDOWS_MENU:			return _("Windows Menu");
	}

	if (keycode > WXK_SPACE && keycode < WXK_DELETE) {
		return wxString((wxChar)keycode, 1);
	}

	return wxT("");
}

const wxString WXKeymodToString(int modifier)
{
	wxString mods;

	if (modifier & wxMOD_META)
#ifdef __APPLE__
		mods += wxT("Cmd+");
#elif defined _WIN32
		mods += wxT("Win+");
#else
		mods += wxT("Meta+");
#endif
	if (modifier & wxMOD_CONTROL)
		mods += wxT("Ctrl+");
	if (modifier & wxMOD_ALT)
		mods += wxT("Alt+");
	if (modifier & wxMOD_SHIFT)
		mods += wxT("Shift+");

	return mods;
}

}
