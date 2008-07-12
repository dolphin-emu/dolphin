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

#pragma once

#include "stdafx.h"

namespace W32Util
{
#define MAX_WIN_DIALOGS 32

	class TabControl
	{
	private:

		HINSTANCE	m_hInstance;
		HWND		m_hWndParent;
		HWND		m_hTabCtrl;

		HWND m_WinDialogs[MAX_WIN_DIALOGS];
		int			m_numDialogs;

	public:

		TabControl(HINSTANCE _hInstance, HWND _hTabCtrl,DLGPROC _lpDialogFunc);

		~TabControl(void);

		HWND AddItem (char* _szText,int _iResource,DLGPROC _lpDialogFunc);

		void SelectDialog (int _nDialogId);

		void MessageHandler(UINT message, WPARAM wParam, LPARAM lParam);
	};
}
