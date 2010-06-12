// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "WXInputBase.h"

namespace InputCommon
{

const wxString WXKeyToString(int keycode)
{
	switch (keycode)
	{
		case WXK_BACK:			return wxT("Back");
		case WXK_TAB:			return wxT("Tab");
		case WXK_RETURN:		return wxT("Return");
		case WXK_ESCAPE:		return wxT("Escape");
		case WXK_SPACE:			return wxT("Space");
		case WXK_DELETE:		return wxT("Delete");

		// Undocumented wx keycodes
		case 167:			return wxT("Paragraph");
		case 177:			return wxT("Plus-Minus");

		case WXK_START:			return wxT("Start");
		case WXK_LBUTTON:		return wxT("L Button");
		case WXK_RBUTTON:		return wxT("R Button");
		case WXK_CANCEL:		return wxT("Cancel");
		case WXK_MBUTTON:		return wxT("M Button");
		case WXK_CLEAR:			return wxT("Clear");
		case WXK_SHIFT:			return wxT("Shift");
		case WXK_ALT:			return wxT("Alt");
		case WXK_CONTROL:		return wxT("Control");
		case WXK_MENU:			return wxT("Menu");
		case WXK_PAUSE:			return wxT("Pause");
		case WXK_CAPITAL:		return wxT("Caps Lock");
		case WXK_END:			return wxT("End");
		case WXK_HOME:			return wxT("Home");
		case WXK_LEFT:			return wxT("Left");
		case WXK_UP:			return wxT("Up");
		case WXK_RIGHT:			return wxT("Right");
		case WXK_DOWN:			return wxT("Down");
		case WXK_SELECT:		return wxT("Select");
		case WXK_PRINT:			return wxT("Print");
		case WXK_EXECUTE:		return wxT("Execute");
		case WXK_SNAPSHOT:		return wxT("Snapshot");
		case WXK_INSERT:		return wxT("Insert");
		case WXK_HELP:			return wxT("Help");
		case WXK_NUMPAD0:		return wxT("NP 0");
		case WXK_NUMPAD1:		return wxT("NP 1");
		case WXK_NUMPAD2:		return wxT("NP 2");
		case WXK_NUMPAD3:		return wxT("NP 3");
		case WXK_NUMPAD4:		return wxT("NP 4");
		case WXK_NUMPAD5:		return wxT("NP 5");
		case WXK_NUMPAD6:		return wxT("NP 6");
		case WXK_NUMPAD7:		return wxT("NP 7");
		case WXK_NUMPAD8:		return wxT("NP 8");
		case WXK_NUMPAD9:		return wxT("NP 9");
		case WXK_MULTIPLY:		return wxT("Multiply");
		case WXK_ADD:			return wxT("Add");
		case WXK_SEPARATOR:		return wxT("Separator");
		case WXK_SUBTRACT:		return wxT("Subtract");
		case WXK_DECIMAL:		return wxT("Decimal");
		case WXK_DIVIDE:		return wxT("Divide");
		case WXK_F1:			return wxT("F1");
		case WXK_F2:			return wxT("F2");
		case WXK_F3:			return wxT("F3");
		case WXK_F4:			return wxT("F4");
		case WXK_F5:			return wxT("F5");
		case WXK_F6:			return wxT("F6");
		case WXK_F7:			return wxT("F7");
		case WXK_F8:			return wxT("F8");
		case WXK_F9:			return wxT("F9");
		case WXK_F10:			return wxT("F10");
		case WXK_F11:			return wxT("F11");
		case WXK_F12:			return wxT("F12");
		case WXK_F13:			return wxT("F13");
		case WXK_F14:			return wxT("F14");
		case WXK_F15:			return wxT("F15");
		case WXK_F16:			return wxT("F16");
		case WXK_F17:			return wxT("F17");
		case WXK_F18:			return wxT("F19");
		case WXK_F19:			return wxT("F20");
		case WXK_F20:			return wxT("F21");
		case WXK_F21:			return wxT("F22");
		case WXK_F22:			return wxT("F23");
		case WXK_F23:			return wxT("F24");
		case WXK_F24:			return wxT("F25");
		case WXK_NUMLOCK:		return wxT("Num Lock");
		case WXK_SCROLL:		return wxT("Scroll Lock");
		case WXK_PAGEUP:		return wxT("Page Up");
		case WXK_PAGEDOWN:		return wxT("Page Down");
		case WXK_NUMPAD_SPACE:		return wxT("NP Space");
		case WXK_NUMPAD_TAB:		return wxT("NP Tab");
		case WXK_NUMPAD_ENTER:		return wxT("NP Enter");
		case WXK_NUMPAD_F1:		return wxT("NP F1");
		case WXK_NUMPAD_F2:		return wxT("NP F2");
		case WXK_NUMPAD_F3:		return wxT("NP F3");
		case WXK_NUMPAD_F4:		return wxT("NP F4");
		case WXK_NUMPAD_HOME:		return wxT("NP Home");
		case WXK_NUMPAD_LEFT:		return wxT("NP Left");
		case WXK_NUMPAD_UP:		return wxT("NP Up");
		case WXK_NUMPAD_RIGHT:		return wxT("NP Right");
		case WXK_NUMPAD_DOWN:		return wxT("NP Down");
		case WXK_NUMPAD_PAGEUP:		return wxT("NP Page Up");
		case WXK_NUMPAD_PAGEDOWN:	return wxT("NP Page Down");
		case WXK_NUMPAD_END:		return wxT("NP End");
		case WXK_NUMPAD_BEGIN:		return wxT("NP Begin");
		case WXK_NUMPAD_INSERT:		return wxT("NP Insert");
		case WXK_NUMPAD_DELETE:		return wxT("NP Delete");
		case WXK_NUMPAD_EQUAL:		return wxT("NP Equal");
		case WXK_NUMPAD_MULTIPLY:	return wxT("NP Multiply");
		case WXK_NUMPAD_ADD:		return wxT("NP Add");
		case WXK_NUMPAD_SEPARATOR:	return wxT("NP Separator");
		case WXK_NUMPAD_SUBTRACT:	return wxT("NP Subtract");
		case WXK_NUMPAD_DECIMAL:	return wxT("NP Decimal");
		case WXK_NUMPAD_DIVIDE:		return wxT("NP Divide");
		case WXK_WINDOWS_LEFT:		return wxT("Windows Left");
		case WXK_WINDOWS_RIGHT:		return wxT("Windows Right");
		case WXK_WINDOWS_MENU:		return wxT("Windows Menu");
		case WXK_COMMAND:		return wxT("Command");
	}

	if (keycode > WXK_SPACE && keycode < WXK_DELETE) {
		return wxString((wxChar)keycode, 1);
	}

	return wxT("");
}

const wxString WXKeymodToString(int modifier)
{
	switch (modifier)
	{
		case wxMOD_ALT:			return wxT("Alt");
		case wxMOD_CONTROL:		return wxT("Ctrl");
		case wxMOD_ALTGR:		return wxT("Ctrl+Alt");
		case wxMOD_SHIFT:		return wxT("Shift");
		// wxWidgets can only use Alt/Ctrl/Shift as menu accelerators,
		// so Meta (Command on OS X) is simply made equivalent to Ctrl.
		case wxMOD_META:		return wxT("Ctrl");
		default:			return wxT("");
	}
}

}
