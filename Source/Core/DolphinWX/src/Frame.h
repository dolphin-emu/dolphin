#ifndef __FRAME_H_
#define __FRAME_H_

class CGameListCtrl;
class CFrame
	: public wxFrame
{
	public:

		CFrame(wxFrame* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _T("Dolphin"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);

		void* GetRenderHandle() {return(m_Panel->GetHandle());}


	private:

		CGameListCtrl* m_GameListCtrl;
		wxPanel* m_Panel;

		enum EBitmaps
		{
			Toolbar_FileOpen,
			Toolbar_Refresh,
			Toolbar_Browse,
			Toolbar_Play,
			Toolbar_Play_Dis,
			Toolbar_Stop,
			Toolbar_Stop_Dis,
			Toolbar_Pause,
			Toolbar_PluginOptions,
			Toolbar_PluginOptions_Dis,
			Toolbar_PluginGFX,
			Toolbar_PluginDSP,
			Toolbar_PluginPAD,
			Toolbar_FullScreen,
			Toolbar_Help,
			Bitmaps_Max
		};

		wxBitmap m_Bitmaps[Bitmaps_Max];
		wxBitmap m_BitmapsMenu[Bitmaps_Max];

		void InitBitmaps();
		void PopulateToolbar(wxToolBar* toolBar);
		void RecreateToolbar();
		void CreateMenu();

		// event handler
		void OnQuit(wxCommandEvent& event);
		void OnHelp(wxCommandEvent& event);
		void OnRefresh(wxCommandEvent& event);
		void OnPluginOptions(wxCommandEvent& event);
		void OnPluginGFX(wxCommandEvent& event);
		void OnPluginDSP(wxCommandEvent& event);
		void OnPluginPAD(wxCommandEvent& event);
		void OnOpen(wxCommandEvent& event);
		void OnPlay(wxCommandEvent& event);
		void OnStop(wxCommandEvent& event);
		void OnBrowse(wxCommandEvent& event);
		void OnMemcard(wxCommandEvent& event);
		void OnToggleFullscreen(wxCommandEvent& event);
		void OnToggleDualCore(wxCommandEvent& event);
		void OnToggleThrottle(wxCommandEvent& event);
		void OnToggleToolbar(wxCommandEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void OnHostMessage(wxCommandEvent& event);


		wxStatusBar* m_pStatusBar;
		wxMenuBar* m_pMenuBar;

		wxMenuItem* m_pMenuItemPlay;
		wxMenuItem* m_pMenuItemStop;
		wxMenuItem* m_pPluginOptions;

		wxBusyInfo* m_pBootProcessDialog;

		void UpdateGUI();


		// old function that could be cool

		DECLARE_EVENT_TABLE();
};

#endif  // __FRAME_H_

