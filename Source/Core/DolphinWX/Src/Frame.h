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


#ifndef __FRAME_H_
#define __FRAME_H_

#include <wx/wx.h> // wxWidgets
#include <wx/busyinfo.h>
#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/artprov.h>
#include <wx/aui/aui.h>

#include "CDUtils.h"
#include "CodeWindow.h"
#include "LogWindow.h"

// A shortcut to access the bitmaps
#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}

// Class declarations
class CGameListCtrl;
class CLogWindow;

class CFrame : public wxFrame
{
	public:

		CFrame(bool showLogWindow,
			wxFrame* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = wxT("Dolphin"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		bool _UseDebugger = false,
		long style = wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE);

		void* GetRenderHandle()
		{
			#ifdef _WIN32
				return(m_Panel->GetHandle());
			#else
				return this;
			#endif
		}

		virtual ~CFrame();

		// These have to be public
		wxStatusBar* m_pStatusBar;
		void InitBitmaps();
		void DoStop();
		bool bRenderToMain;
		void UpdateGUI();
		void ToggleLogWindow(bool check);
		void ToggleConsole(bool check);
		CCodeWindow* g_pCodeWindow;
		void PostEvent(wxCommandEvent& event);
		void PostMenuEvent(wxMenuEvent& event);
		void PostUpdateUIEvent(wxUpdateUIEvent& event);

		// ---------------------------------------
		// Wiimote leds
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		void ModifyStatusBar();
		void CreateDestroy(int Case);
		void CreateLeds(); void CreateSpeakers();
		void UpdateLeds(); void UpdateSpeakers();
		wxBitmap CreateBitmapForLeds(bool On);
		wxBitmap CreateBitmapForSpeakers(int BitmapType, bool On);
		void DoMoveIcons(); void MoveLeds();  void MoveSpeakers();
		bool HaveLeds; bool HaveSpeakers;

		wxStaticBitmap *m_StatBmp[7];
		u8 g_Leds[4]; u8 g_Leds_[4];
		u8 g_Speakers[3]; u8 g_Speakers_[3];
		// ---------------

	private:
		
		bool UseDebugger;
		wxBoxSizer* sizerPanel;
		wxBoxSizer* sizerFrame;
		CGameListCtrl* m_GameListCtrl;
		wxPanel* m_Panel;
		wxToolBarToolBase* m_ToolPlay;
		bool m_bLogWindow;
		CLogWindow* m_LogWindow;

		// AUI
		wxAuiManager *m_Mgr;
		wxAuiToolBar *m_ToolBar, *m_ToolBar2;
		wxAuiNotebook *m_NB0, *m_NB1;
		// Perspectives
		wxString AuiFullscreen;
		wxString AuiMode1;
		wxString AuiMode2;
		wxString AuiCurrent;
		void OnNotebookPageClose(wxAuiNotebookEvent& evt);

		char **drives;

		enum EToolbar
		{
			Toolbar_FileOpen,
			Toolbar_Refresh,
			Toolbar_Browse,
			Toolbar_Play,
			Toolbar_Stop,
			Toolbar_Pause,
			Toolbar_Screenshot,
			Toolbar_FullScreen,
			Toolbar_PluginOptions,
			Toolbar_PluginGFX,
			Toolbar_PluginDSP,
			Toolbar_PluginPAD,
			Toolbar_Wiimote,			
			Toolbar_Help,
			EToolbar_Max
		};

		enum EBitmapsThemes
		{
			BOOMY,
			VISTA,
			XPLASTIK,
			KDE,
			THEMES_MAX
		};

		enum WiimoteBitmaps  // Wiimote speaker bitmaps
		{
			CREATELEDS, 						
			DESTROYLEDS,
			CREATESPEAKERS,
			DESTROYSPEAKERS
		};

		wxBitmap m_Bitmaps[EToolbar_Max];
		wxBitmap m_BitmapsMenu[EToolbar_Max];

		void PopulateToolbar(wxAuiToolBar* toolBar);
		void RecreateToolbar();
		void CreateMenu();

#ifdef _WIN32
		// Override window proc for tricks like screensaver disabling
		WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif
		// Event functions
		void OnQuit(wxCommandEvent& event);
		void OnHelp(wxCommandEvent& event);

		void OnOpen(wxCommandEvent& event); // File menu
		void DoOpen(bool Boot);
		void OnRefresh(wxCommandEvent& event);
		void OnBrowse(wxCommandEvent& event);
		void OnBootDrive(wxCommandEvent& event);

		void OnPlay(wxCommandEvent& event); // Emulation
		void OnRecord(wxCommandEvent& event);
		void OnPlayRecording(wxCommandEvent& event);
		void OnChangeDisc(wxCommandEvent& event);
		void OnStop(wxCommandEvent& event);
		void OnScreenshot(wxCommandEvent& event);
		void OnClose(wxCloseEvent &event);	
		void OnLoadState(wxCommandEvent& event);
		void OnSaveState(wxCommandEvent& event);
		void OnLoadStateFromFile(wxCommandEvent& event);
		void OnSaveStateToFile(wxCommandEvent& event);
		void OnLoadLastState(wxCommandEvent& event);
		void OnUndoLoadState(wxCommandEvent& event);
		void OnUndoSaveState(wxCommandEvent& event);
		
		void OnFrameSkip(wxCommandEvent& event);
		void OnFrameStep(wxCommandEvent& event);
		
		void OnConfigMain(wxCommandEvent& event); // Options
		void OnPluginGFX(wxCommandEvent& event);
		void OnPluginDSP(wxCommandEvent& event);
		void OnPluginPAD(wxCommandEvent& event);
		void OnPluginWiimote(wxCommandEvent& event);

		void OnToggleFullscreen(wxCommandEvent& event);
		void OnToggleDualCore(wxCommandEvent& event);
		void OnToggleSkipIdle(wxCommandEvent& event);
		void OnToggleThrottle(wxCommandEvent& event);
		void OnResize(wxSizeEvent& event);
		void OnToggleToolbar(wxCommandEvent& event);
		void OnToggleStatusbar(wxCommandEvent& event);
		void OnToggleLogWindow(wxCommandEvent& event);
		void OnToggleConsole(wxCommandEvent& event);
		void OnKeyDown(wxKeyEvent& event);
		void OnKeyUp(wxKeyEvent& event);
		void OnDoubleClick(wxMouseEvent& event);
		void OnMotion(wxMouseEvent& event);
		
		void OnHostMessage(wxCommandEvent& event);

		void OnMemcard(wxCommandEvent& event); // Misc

		void OnNetPlay(wxCommandEvent& event);

		void OnShow_CheatsWindow(wxCommandEvent& event);
		void OnShow_InfoWindow(wxCommandEvent& event);
		void OnLoadWiiMenu(wxCommandEvent& event);
		void GameListChanged(wxCommandEvent& event);

		void OnGameListCtrl_ItemActivated(wxListEvent& event);
		void CFrame::DoFullscreen(bool _F);

		// MenuBar
		// File - Drive
		wxMenuItem* m_pSubMenuDrive;

		// Emulation
		wxMenuItem* m_pSubMenuLoad;
		wxMenuItem* m_pSubMenuSave;
		wxMenuItem* m_pSubMenuFrameSkipping;

		void BootGame();

		// Double click and mouse move options
		double m_fLastClickTime, m_iLastMotionTime;
		int LastMouseX, LastMouseY;

		#if wxUSE_TIMER
			void Update();
			void OnTimer(wxTimerEvent& WXUNUSED(event)) { Update(); }
			wxTimer m_timer;
		#endif

		// Event table
		DECLARE_EVENT_TABLE();
};

#endif  // __FRAME_H_
