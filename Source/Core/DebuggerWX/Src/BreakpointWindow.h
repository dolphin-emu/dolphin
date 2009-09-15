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

#ifndef __BREAKPOINTWINDOW_h__
#define __BREAKPOINTWINDOW_h__

class CBreakPointView;
class CCodeWindow;
class wxListEvent;
class IniFile;

class CBreakPointWindow
	: public wxPanel
{
	private:

		DECLARE_EVENT_TABLE();

	public:

		CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Breakpoints"), 
						  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(400, 250), 
						  long style = wxNO_BORDER);

		void NotifyUpdate();

	private:

		enum
		{
			ID_TOOLBAR = 501,
			ID_BPS = 1002,
			IDM_DELETE,
			IDM_CLEAR,
			IDM_ADD_BREAKPOINT,
			IDM_ADD_BREAKPOINTMANY,
			IDM_ADD_MEMORYCHECK,
			IDM_ADD_MEMORYCHECKMANY
		};

		enum
		{
			Toolbar_Delete,
			Toolbar_Add_BreakPoint,
			Toolbar_Add_Memcheck,
			Bitmaps_max
		};

		CBreakPointView* m_BreakPointListView;
        CCodeWindow* m_pCodeWindow;

		wxBitmap m_Bitmaps[Bitmaps_max];

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();

		void RecreateToolbar();
		void PopulateToolbar(wxToolBar* toolBar);
		void InitBitmaps();

		void OnDelete(wxCommandEvent& event);
		void OnClear(wxCommandEvent& event);
		void OnAddBreakPoint(wxCommandEvent& event);
		void OnAddBreakPointMany(wxCommandEvent& event);		
		void OnAddMemoryCheck(wxCommandEvent& event);
		void OnAddMemoryCheckMany(wxCommandEvent& event);
        void OnActivated(wxListEvent& event);
};

#endif
