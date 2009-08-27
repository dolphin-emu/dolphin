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


#ifndef CODEWINDOW_H_
#define CODEWINDOW_H_

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include <wx/artprov.h>
#include <wx/aui/aui.h>

#include "Thread.h"
#include "CoreParameter.h"

class CRegisterWindow;
class CBreakPointWindow;
class CMemoryWindow;
class CJitWindow;
class CCodeView;

class CCodeWindow
	: public wxPanel
{
	public:

		CCodeWindow(const SCoreStartupParameter& _LocalCoreStartupParameter, wxWindow* parent,
			wxWindowID id = wxID_ANY);
		/*
		CCodeWindow(const SCoreStartupParameter& _LocalCoreStartupParameter, wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _T("Dolphin-Debugger"),
		const wxPoint& pos = wxPoint(950, 100),
		const wxSize& size = wxSize(400, 500),
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);
		*/

		~CCodeWindow();

		// Function redirection
		wxFrame *GetParentFrame();
		wxMenuBar * GetMenuBar();
		wxAuiToolBar * GetToolBar(), * m_ToolBar2;
		wxAuiNotebook *m_NB0, *m_NB1;
		bool IsActive();
		void UpdateToolbar(wxAuiToolBar *);
		void UpdateNotebook(int, wxAuiNotebook *);
		wxBitmap page_bmp;
		#ifdef _WIN32
		wxWindow * GetWxWindow(wxString);
		#endif
		wxWindow * GetNootebookPage(wxString);
		void DoToggleWindow(int,bool);

		void Load_(IniFile &file);
		void Load(IniFile &file);
		void Save(IniFile &file);

		bool UseInterpreter();
		bool BootToPause();
		bool AutomaticStart();
		bool UnlimitedJITCache();
		bool JITBlockLinking();
		//bool UseDualCore(); // not used
        void JumpToAddress(u32 _Address);

		void Update();
		void NotifyMapLoaded();
		void CreateMenu(const SCoreStartupParameter& _LocalCoreStartupParameter, wxMenuBar * pMenuBar);
		void RecreateToolbar(wxAuiToolBar*);
		void PopulateToolbar(wxAuiToolBar* toolBar);
		void CreateSymbolsMenu();
		void UpdateButtonStates();
		void CCodeWindow::UpdateManager();

		// Sub dialogs
		wxMenuBar* pMenuBar;
		CRegisterWindow* m_RegisterWindow;
		CBreakPointWindow* m_BreakpointWindow;
		CMemoryWindow* m_MemoryWindow;
		CJitWindow* m_JitWindow;

	private:

		enum
		{			
			// Debugger GUI Objects
			ID_CODEVIEW,
			ID_CALLSTACKLIST,
			ID_CALLERSLIST,
			ID_CALLSLIST,
			ID_SYMBOLLIST
		};

		enum
		{
			Toolbar_DebugGo,
			Toolbar_Pause,
			Toolbar_Step,
			Toolbar_StepOver,
			Toolbar_Skip,
			Toolbar_GotoPC,
			Toolbar_SetPC,
			Bitmaps_max
		};

		// Settings
		bool bAutomaticStart; bool bBootToPause;
		bool bRegisterWindow;
		bool bBreakpointWindow;
		bool bMemoryWindow;
		bool bJitWindow;
		bool bSoundWindow;
		bool bVideoWindow;

		void OnSymbolListChange(wxCommandEvent& event);
		void OnSymbolListContextMenu(wxContextMenuEvent& event);
		void OnCallstackListChange(wxCommandEvent& event);
		void OnCallersListChange(wxCommandEvent& event);
		void OnCallsListChange(wxCommandEvent& event);
		void OnCodeStep(wxCommandEvent& event);
		void OnCodeViewChange(wxCommandEvent &event);
		void SingleCPUStep();

		void OnAddrBoxChange(wxCommandEvent& event);		
		
		void OnToggleWindow(wxCommandEvent& event);
		void OnToggleRegisterWindow(bool,wxAuiNotebook*);
		void OnToggleBreakPointWindow(bool,wxAuiNotebook*);
		void OnToggleMemoryWindow(bool,wxAuiNotebook*);
		void OnToggleJitWindow(bool,wxAuiNotebook*);
		void OnToggleSoundWindow(bool,wxAuiNotebook*);
		void OnToggleVideoWindow(bool,wxAuiNotebook*);
		void OnChangeFont(wxCommandEvent& event);
	
		void OnHostMessage(wxCommandEvent& event);
		void OnSymbolsMenu(wxCommandEvent& event);
		void OnJitMenu(wxCommandEvent& event);
		void OnProfilerMenu(wxCommandEvent& event);

		void OnCPUMode(wxCommandEvent& event); // CPU Mode menu	
		void OnJITOff(wxCommandEvent& event);

		void UpdateLists();
		void UpdateCallstack();
		void OnStatusBar(wxMenuEvent& event); void OnStatusBar_(wxUpdateUIEvent& event);
		void DoTip(wxString text);
		void OnKeyDown(wxKeyEvent& event);

		wxMenuItem* jitblocklinking, *jitunlimited, *jitoff;
		wxMenuItem* jitlsoff, *jitlslxzoff, *jitlslwzoff, *jitlslbzxoff;
		wxMenuItem* jitlspoff;
		wxMenuItem* jitlsfoff;
		wxMenuItem* jitfpoff;
		wxMenuItem* jitioff;
		wxMenuItem* jitpoff;
		wxMenuItem* jitsroff;

		CCodeView* codeview;
		wxListBox* callstack;
		wxListBox* symbols;
		wxListBox* callers;
		wxListBox* calls;
		Common::Event sync_event;

		wxBitmap m_Bitmaps[Bitmaps_max];

		DECLARE_EVENT_TABLE()

		void InitBitmaps();
		void CreateGUIControls(const SCoreStartupParameter& _LocalCoreStartupParameter);		
};

#endif /*CODEWINDOW_*/
