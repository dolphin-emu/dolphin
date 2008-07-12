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

#include "TabControl.h"
#include <commctrl.h>

namespace W32Util
{
	TabControl::TabControl(HINSTANCE _hInstance, HWND _hTabCtrl,DLGPROC _lpDialogFunc) :
		m_hInstance(_hInstance),
		m_hTabCtrl(_hTabCtrl),
		m_numDialogs(0)
	{
		for (int i=0; i<MAX_WIN_DIALOGS; i++)
			m_WinDialogs[i] = NULL;
	}

	TabControl::~TabControl(void)
	{}

	HWND TabControl::AddItem (char* _szText,int _iResource,DLGPROC _lpDialogFunc)
	{
		TCITEM tcItem;

		ZeroMemory (&tcItem,sizeof (tcItem));

		tcItem.mask			= TCIF_TEXT | TCIF_IMAGE;
		tcItem.dwState		= 0;
		tcItem.pszText		= _szText;
		tcItem.cchTextMax	= sizeof (_szText);
		tcItem.iImage		= -1;

		int nResult = TabCtrl_InsertItem (m_hTabCtrl,TabCtrl_GetItemCount (m_hTabCtrl),&tcItem);


		HWND hDialog = CreateDialog(m_hInstance,(LPCSTR)_iResource,m_hTabCtrl,_lpDialogFunc);
		RECT rectInnerWindow = {0,0,0,0};

		GetWindowRect (m_hTabCtrl,&rectInnerWindow);

		TabCtrl_AdjustRect (m_hTabCtrl,FALSE,&rectInnerWindow);

		POINT pntPosition = {rectInnerWindow.left,rectInnerWindow.top};
		ScreenToClient(m_hTabCtrl, &pntPosition);

		SetWindowPos(hDialog, 0,
			pntPosition.x, pntPosition.y,
			rectInnerWindow.right - rectInnerWindow.left,rectInnerWindow.bottom - rectInnerWindow.top,0);
		ShowWindow(hDialog,SW_NORMAL);

		m_WinDialogs[m_numDialogs] = hDialog;
		m_numDialogs++; 

		SelectDialog (0);

		return hDialog;
	}

	void TabControl::SelectDialog (int _nDialogId)
	{
		for (int i = 0 ; i < m_numDialogs ; i ++)
			if (m_WinDialogs[i] != NULL)
				ShowWindow(m_WinDialogs[i],i == _nDialogId ? SW_NORMAL : SW_HIDE);
	}

	void TabControl::MessageHandler(UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_NOTIFY)
		{
			NMHDR* pNotifyMessage = NULL;
			pNotifyMessage = (LPNMHDR)lParam; 		
			if (pNotifyMessage->hwndFrom == m_hTabCtrl)
			{
				int iPage = TabCtrl_GetCurSel (m_hTabCtrl); 			
				SelectDialog (iPage);
			}
		}   
	}

}

