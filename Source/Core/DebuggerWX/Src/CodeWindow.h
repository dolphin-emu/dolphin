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

#include "Thread.h"
#include "CoreParameter.h"

// GUI global
#include "../../DolphinWX/Src/Globals.h"
#include "../../DolphinWX/Src/Frame.h"

class CFrame;
class CRegisterWindow;
class CBreakPointWindow;
class CMemoryWindow;
class CJitWindow;
class CCodeView;
class DSPDebuggerLLE;
class GFXDebuggerPanel;

class CCodeWindow
	: public wxPanel
{
	public:

		CCodeWindow(const SCoreStartupParameter& _LocalCoreStartupParameter,
			CFrame * parent,
			wxWindowID id = wxID_ANY,
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
			const wxString& name = _("Code")
			);

		void Load();
		void Save();

		// Parent interaction
		CFrame *Parent;
		wxMenuBar * GetMenuBar();
		wxAuiToolBar * GetToolBar();
		wxBitmap m_Bitmaps[ToolbarDebugBitmapMax];

		bool UseInterpreter();
		bool BootToPause();
		bool AutomaticStart();
		bool JITNoBlockCache();
		bool JITBlockLinking();
		void JumpToAddress(u32 _Address);

		void Update();
		void NotifyMapLoaded();
		void CreateMenu(const SCoreStartupParameter& _LocalCoreStartupParameter,
			   	wxMenuBar *pMenuBar);
		void CreateMenuOptions(wxMenu *pMenu);
		void CreateMenuSymbols(wxMenuBar *pMenuBar);
		void RecreateToolbar(wxAuiToolBar*);
		void PopulateToolbar(wxAuiToolBar* toolBar);
		void UpdateButtonStates();
		void OpenPages();
		void UpdateManager();

		// Menu bar
		// -------------------
		void OnCPUMode(wxCommandEvent& event); // CPU Mode menu
		void OnJITOff(wxCommandEvent& event);

		void ToggleCodeWindow(bool bShow);
		void ToggleRegisterWindow(bool bShow);
		void ToggleBreakPointWindow(bool bShow);
		void ToggleMemoryWindow(bool bShow);
		void ToggleJitWindow(bool bShow);
		void ToggleSoundWindow(bool bShow);
		void ToggleVideoWindow(bool bShow);

		void OnChangeFont(wxCommandEvent& event);

		void OnCodeStep(wxCommandEvent& event);
		void OnAddrBoxChange(wxCommandEvent& event);
		void OnSymbolsMenu(wxCommandEvent& event);
		void OnJitMenu(wxCommandEvent& event);
		void OnProfilerMenu(wxCommandEvent& event);

		// Sub dialogs
		CRegisterWindow* m_RegisterWindow;
		CBreakPointWindow* m_BreakpointWindow;
		CMemoryWindow* m_MemoryWindow;
		CJitWindow* m_JitWindow;
		DSPDebuggerLLE* m_SoundWindow;
		GFXDebuggerPanel* m_VideoWindow;

		// Settings
		bool bAutomaticStart; bool bBootToPause;
		bool bShowOnStart[IDM_VIDEOWINDOW - IDM_LOGWINDOW + 1];
		int iNbAffiliation[IDM_CODEWINDOW - IDM_LOGWINDOW + 1];

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

		void OnSymbolListChange(wxCommandEvent& event);
		void OnSymbolListContextMenu(wxContextMenuEvent& event);
		void OnCallstackListChange(wxCommandEvent& event);
		void OnCallersListChange(wxCommandEvent& event);
		void OnCallsListChange(wxCommandEvent& event);
		void OnCodeViewChange(wxCommandEvent &event);
		void OnHostMessage(wxCommandEvent& event);

		// Debugger functions
		void SingleStep();
		void StepOver();
		void ToggleBreakpoint();

		void UpdateLists();
		void UpdateCallstack();
		void OnKeyDown(wxKeyEvent& event);

		void InitBitmaps();
		void CreateGUIControls(const SCoreStartupParameter& _LocalCoreStartupParameter);

		CCodeView* codeview;
		wxListBox* callstack;
		wxListBox* symbols;
		wxListBox* callers;
		wxListBox* calls;
		Common::Event sync_event;

		DECLARE_EVENT_TABLE()
};

#endif // CODEWINDOW_H_
