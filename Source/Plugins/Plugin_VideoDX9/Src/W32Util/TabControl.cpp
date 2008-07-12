#include <commctrl.h>

#include "TabControl.h"

namespace W32Util
{
	// __________________________________________________________________________________________________
	// constructor
	//
	TabControl::TabControl(HINSTANCE _hInstance, HWND _hTabCtrl,DLGPROC _lpDialogFunc) :
		m_hInstance(_hInstance),
		m_hTabCtrl(_hTabCtrl),
		m_numDialogs(0)
	{
		for (int i=0; i<MAX_WIN_DIALOGS; i++)
			m_WinDialogs[i] = NULL;
	}

	// __________________________________________________________________________________________________
	// destructor
	//
	TabControl::~TabControl(void)
	{}

	// __________________________________________________________________________________________________
	// AddItem
	//
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

	// __________________________________________________________________________________________________
	// SelectDialog
	//
	void TabControl::SelectDialog (int _nDialogId)
	{
		for (int i = 0 ; i < m_numDialogs ; i ++)
			if (m_WinDialogs[i] != NULL)
				ShowWindow(m_WinDialogs[i],i == _nDialogId ? SW_NORMAL : SW_HIDE);
	}

	// __________________________________________________________________________________________________
	// MessageHandler
	//
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

