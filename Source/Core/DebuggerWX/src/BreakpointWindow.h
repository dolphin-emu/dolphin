//__________________________________________________________________________________________________
// F|RES and ector 2003-2008
//

#ifndef __BREAKPOINTWINDOW_h__
#define __BREAKPOINTWINDOW_h__

class CBreakPointView;

#undef BREAKPOINT_WINDOW_STYLE
#define BREAKPOINT_WINDOW_STYLE wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER

class CBreakPointWindow
	: public wxFrame
{
	private:

		DECLARE_EVENT_TABLE();

	public:

		CBreakPointWindow(wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Breakpoints"), 
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(400, 250), 
			long style = BREAKPOINT_WINDOW_STYLE);

		virtual ~CBreakPointWindow();

		void NotifyUpdate();


	private:

		enum
		{
			ID_TOOLBAR = 500,
			ID_BPS = 1002,
			IDM_DELETE,
			IDM_ADD_BREAKPOINT,
			IDM_ADD_MEMORYCHECK
		};

		enum
		{
			Toolbar_Delete,
			Toolbar_Add_BreakPoint,
			Toolbar_Add_Memcheck,
			Bitmaps_max
		};

		CBreakPointView* m_BreakPointListView;
		wxBitmap m_Bitmaps[Bitmaps_max];

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();

		void RecreateToolbar();
		void PopulateToolbar(wxToolBar* toolBar);
		void InitBitmaps();

		void OnDelete(wxCommandEvent& event);
		void OnAddBreakPoint(wxCommandEvent& event);
		void OnAddMemoryCheck(wxCommandEvent& event);
};

#endif
