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

#include <wx/stattext.h>
#include "WXInputBase.h"
//#include <string.h>
//#include <stdio.h>
//#include <ctype.h>

namespace InputCommon
{

const wxChar *WXKeyToString(int keycode)
{
	switch (keycode)
	{
		case WXK_CANCEL:            return wxT("Cancel"); break;
		case WXK_BACK:              return wxT("Back"); break;
		case WXK_TAB:               return wxT("Tab"); break;
		case WXK_CLEAR:             return wxT("Clear"); break;
		case WXK_RETURN:            return wxT("Return"); break;
		case WXK_SHIFT:             return wxT("Shift"); break;
		case WXK_CONTROL:           return wxT("Control"); break;
		case WXK_ALT:               return wxT("Alt"); break;
		case WXK_CAPITAL:           return wxT("CapsLock"); break;
		case WXK_MENU :             return wxT("Menu"); break;
		case WXK_PAUSE:             return wxT("Pause"); break;
		case WXK_ESCAPE:            return wxT("Escape"); break;
		case WXK_SPACE:             return wxT("Space"); break;
		case WXK_PAGEUP:            return wxT("PgUp"); break;
		case WXK_PAGEDOWN:          return wxT("PgDn"); break;
		case WXK_END:               return wxT("End"); break;
		case WXK_HOME :             return wxT("Home"); break;
		case WXK_LEFT :             return wxT("Left"); break;
		case WXK_UP:                return wxT("Up"); break;
		case WXK_RIGHT:             return wxT("Right"); break;
		case WXK_DOWN :             return wxT("Down"); break;
		case WXK_SELECT:            return wxT("Select"); break;
		case WXK_PRINT:             return wxT("Print"); break;
		case WXK_EXECUTE:           return wxT("Execute"); break;
		case WXK_INSERT:            return wxT("Insert"); break;
		case WXK_DELETE:            return wxT("Delete"); break;
		case WXK_HELP :             return wxT("Help"); break;
		case WXK_NUMPAD0:           return wxT("NP 0"); break;
		case WXK_NUMPAD1:           return wxT("NP 1"); break;
		case WXK_NUMPAD2:           return wxT("NP 2"); break;
		case WXK_NUMPAD3:           return wxT("NP 3"); break;
		case WXK_NUMPAD4:           return wxT("NP 4"); break;
		case WXK_NUMPAD5:           return wxT("NP 5"); break;
		case WXK_NUMPAD6:           return wxT("NP 6"); break;
		case WXK_NUMPAD7:           return wxT("NP 7"); break;
		case WXK_NUMPAD8:           return wxT("NP 8"); break;
		case WXK_NUMPAD9:           return wxT("NP 9"); break;
		case WXK_NUMPAD_DECIMAL:    return wxT("NP ."); break;
		case WXK_NUMPAD_DELETE:     return wxT("NP Delete"); break;
		case WXK_NUMPAD_INSERT:     return wxT("NP Insert"); break;
		case WXK_NUMPAD_END:        return wxT("NP End"); break;
		case WXK_NUMPAD_DOWN:       return wxT("NP Down"); break;
		case WXK_NUMPAD_PAGEDOWN:   return wxT("NP Pagedown"); break;
		case WXK_NUMPAD_LEFT:       return wxT("NP Left"); break;
		case WXK_NUMPAD_RIGHT:      return wxT("NP Right"); break;
		case WXK_NUMPAD_HOME:       return wxT("NP Home"); break;
		case WXK_NUMPAD_UP:         return wxT("NP Up"); break;
		case WXK_NUMPAD_PAGEUP:     return wxT("NP Pageup"); break;
		case WXK_NUMPAD_MULTIPLY:   return wxT("NP *"); break;
		case WXK_NUMPAD_ADD:        return wxT("NP +"); break;
		case WXK_NUMPAD_SUBTRACT:   return wxT("NP -"); break;
		case WXK_NUMPAD_DIVIDE:     return wxT("NP /"); break;
		case WXK_NUMPAD_ENTER:      return wxT("NP Enter"); break;
		case WXK_NUMPAD_SEPARATOR:  return wxT("NP Separator"); break;
		case WXK_F1:                return wxT("F1"); break;
		case WXK_F2:                return wxT("F2"); break;
		case WXK_F3:                return wxT("F3"); break;
		case WXK_F4:                return wxT("F4"); break;
		case WXK_F5:                return wxT("F5"); break;
		case WXK_F6:                return wxT("F6"); break;
		case WXK_F7:                return wxT("F7"); break;
		case WXK_F8:                return wxT("F8"); break;
		case WXK_F9:                return wxT("F9"); break;
		case WXK_F10:               return wxT("F10"); break;
		case WXK_F11:               return wxT("F11"); break;
		case WXK_F12:               return wxT("F12"); break;
		case WXK_F13:               return wxT("F13"); break;
		case WXK_F14:               return wxT("F14"); break;
		case WXK_F15:               return wxT("F15"); break;
		case WXK_F16:               return wxT("F16"); break;
		case WXK_F17:               return wxT("F17"); break;
		case WXK_F18:               return wxT("F19"); break;
		case WXK_F19:               return wxT("F20"); break;
		case WXK_F20:               return wxT("F21"); break;
		case WXK_F21:               return wxT("F22"); break;
		case WXK_F22:               return wxT("F23"); break;
		case WXK_F23:               return wxT("F24"); break;
		case WXK_F24:               return wxT("F25"); break;
		case WXK_NUMLOCK:           return wxT("Numlock"); break;
		case WXK_SCROLL:            return wxT("Scrolllock"); break;
		default:                    return wxString::FromAscii(keycode);
	}
}

const wxChar *WXKeymodToString(int modifier)
{
	switch (modifier)
	{
		case wxMOD_ALT:         return wxT("Alt"); break;
		case wxMOD_CMD:         return wxT("Ctrl"); break;
		case wxMOD_ALTGR:       return wxT("Ctrl+Alt"); break;
		case wxMOD_SHIFT:       return wxT("Shift"); break;
		default:                return wxT(""); break;
	}
}

}
