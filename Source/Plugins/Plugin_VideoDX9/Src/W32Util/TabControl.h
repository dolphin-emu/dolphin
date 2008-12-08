#pragma once

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

		//
		// --- tools ---
		//

		HWND AddItem (char* _szText,int _iResource,DLGPROC _lpDialogFunc);

		void SelectDialog (int _nDialogId);

		void MessageHandler(UINT message, WPARAM wParam, LPARAM lParam);
	};

}
