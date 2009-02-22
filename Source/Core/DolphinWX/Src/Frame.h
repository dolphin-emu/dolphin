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


//////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯
#ifndef __FRAME_H_
#define __FRAME_H_

#include <wx/wx.h> // wxWidgets
#include <wx/busyinfo.h>
#include <wx/mstream.h>
////////////////////////////////
#include "DriveUtil.h"

//////////////////////////////////////////////////////////////////////////
// A shortcut to access the bitmaps
// ¯¯¯¯¯¯¯¯¯¯
#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}
////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// Class declarations
// ¯¯¯¯¯¯¯¯¯¯
class CGameListCtrl;
class CFrame : public wxFrame
{
	public:

		CFrame(wxFrame* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = wxT("Dolphin"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
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

		// ---------------------------------------
		// Wiimote leds
		// ---------------
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

		wxBoxSizer* sizerPanel;
		CGameListCtrl* m_GameListCtrl;
		wxPanel* m_Panel;
		wxToolBar* TheToolBar;
		wxToolBarToolBase* m_ToolPlay;

		std::vector<std::string> drives;

		//////////////////////////////////////////////////////////////////////////////////////
		// Music mod
		// ¯¯¯¯¯¯¯¯¯¯
		#ifdef MUSICMOD
			wxToolBarToolBase* mm_ToolMute, * mm_ToolPlay, * mm_ToolLog;
			wxSlider * mm_Slider;

			void MM_UpdateGUI(); void MM_PopulateGUI(); void MM_InitBitmaps(int Theme);
			void MM_OnPlay(); void MM_OnStop();
			void MM_OnMute(wxCommandEvent& event);
			void MM_OnPause(wxCommandEvent& event);
			void MM_OnVolume(wxScrollEvent& event);
			void MM_OnLog(wxCommandEvent& event);
		#endif
		///////////////////////////////////


		enum EToolbar
		{
			Toolbar_FileOpen,
			Toolbar_Refresh,
			Toolbar_Browse,
			Toolbar_Play,
			Toolbar_Stop,
			Toolbar_Pause,
			Toolbar_FullScreen,
			Toolbar_PluginOptions,
			Toolbar_PluginGFX,
			Toolbar_PluginDSP,
			Toolbar_PluginPAD,
			Toolbar_Wiimote,			
			Toolbar_Help,

			#ifdef MUSICMOD  // Music modification
				Toolbar_Log, Toolbar_PluginDSP_Dis, Toolbar_Log_Dis,
			#endif

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

		void PopulateToolbar(wxToolBar* toolBar);
		void RecreateToolbar();
		void CreateMenu();

#ifdef _WIN32
		// Override window proc for tricks like screensaver disabling
		WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);
#endif
		// Event functions
		void OnQuit(wxCommandEvent& event);
		void OnHelp(wxCommandEvent& event);

		void OnOpen(wxCommandEvent& event); void DoOpen(bool Boot); // File menu
		void OnRefresh(wxCommandEvent& event);
		void OnBrowse(wxCommandEvent& event);
		void OnBootDrive(wxCommandEvent& event);

		void OnPlay(wxCommandEvent& event); // Emulation
		void OnChangeDisc(wxCommandEvent& event);
		void OnStop(wxCommandEvent& event);
		void OnClose(wxCloseEvent &event);	
		void OnLoadState(wxCommandEvent& event);
		void OnSaveState(wxCommandEvent& event);
		
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
		void OnKeyDown(wxKeyEvent& event); void OnKeyUp(wxKeyEvent& event);
		void OnDoubleClick(wxMouseEvent& event); void OnMotion(wxMouseEvent& event);
		
		void OnHostMessage(wxCommandEvent& event);

		void OnMemcard(wxCommandEvent& event); // Misc
		void OnShow_CheatsWindow(wxCommandEvent& event);
		void OnShow_SDCardWindow(wxCommandEvent& event);

		void OnGameListCtrl_ItemActivated(wxListEvent& event);

		// Menu items
		wxMenuBar* m_pMenuBar;

		wxMenuItem* m_pMenuItemOpen; // File

		wxMenuItem* m_pMenuItemPlay; // Emulation
		wxMenuItem* m_pMenuItemStop;
		wxMenuItem* m_pMenuChangeDisc;
		wxMenuItem* m_pPluginOptions;
		wxMenuItem* m_pMenuItemLoad;
		wxMenuItem* m_pMenuItemSave;
		wxToolBarToolBase* m_pToolPlay;

		void BootGame();

		// Double click and mouse move options
		double m_fLastClickTime, m_iLastMotionTime; int LastMouseX, LastMouseY;

		#if wxUSE_TIMER
			void Update();
			void OnTimer(wxTimerEvent& WXUNUSED(event)) { Update(); }
			wxTimer m_timer;
		#endif

		// Event table
		DECLARE_EVENT_TABLE();
};
////////////////////////////////

#endif  // __FRAME_H_
