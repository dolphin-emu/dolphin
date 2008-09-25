// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _DIRECTINPUTBASE_H
#define _DIRECTINPUTBASE_H

class DInput
{
	public:

		DInput();
		~DInput();

		static void DInput::DIKToString(unsigned int keycode, char *keyStr);

		HRESULT Init(HWND hWnd);
		void Free();
		HRESULT Read();


		BYTE diks[256]; // DirectInput keyboard state buffer

	private:

		LPDIRECTINPUT8 g_pDI; // The DirectInput object
		LPDIRECTINPUTDEVICE8 g_pKeyboard; // The keyboard device
};

#endif

