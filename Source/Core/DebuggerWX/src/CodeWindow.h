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

#ifndef CODEWINDOW_H_
#define CODEWINDOW_H_

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include "Debugger.h"
#include "CodeView.h"
#include "Thread.h"

#include "CoreParameter.h"

class CRegisterWindow;
class CLogWindow;
class CBreakPointWindow;
class CMemoryWindow;
class CJitWindow;
class CCodeView;
class IniFile;

class CCodeWindow
	: public wxFrame
{
	public:

		CCodeWindow(const SCoreStartupParameter& _LocalCoreStartupParameter, wxWindow* parent,
			wxWindowID id = wxID_ANY,
			const wxString& title = _T("Dolphin-Debugger"),
		const wxPoint& pos = wxPoint(950, 100),
		const wxSize& size = wxSize(400, 500),
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);

		~CCodeWindow();

		void Load(IniFile &file);
		void Save(IniFile &file) const;

		void Update();
		void NotifyMapLoaded();


		bool UseInterpreter();
		bool UseDualCore();
        void JumpToAddress(u32 _Address);

	private:

		enum
		{
			ID_TOOLBAR = 500,
			IDM_CODEVIEW,
			IDM_DEBUG_GO,
			IDM_STEP,
			IDM_STEPOVER,
			IDM_SKIP,
			IDM_SETPC,
			IDM_GOTOPC,
			IDM_ADDRBOX,
			IDM_CALLSTACKLIST,
			IDM_CALLERSLIST,
			IDM_CALLSLIST,
			IDM_SYMBOLLIST,
			IDM_INTERPRETER,
			IDM_DUALCORE,
			IDM_LOGWINDOW,
			IDM_REGISTERWINDOW,
			IDM_BREAKPOINTWINDOW,
			IDM_MEMORYWINDOW,
			IDM_SCANFUNCTIONS,
			IDM_LOGINSTRUCTIONS,
			IDM_LOADMAPFILE,
			IDM_SAVEMAPFILE,
			IDM_CLEARSYMBOLS,
			IDM_CLEANSYMBOLS,
			IDM_CREATESIGNATUREFILE,
			IDM_USESIGNATUREFILE,
			IDM_USESYMBOLFILE,
			IDM_CLEARCODECACHE,
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

		// sub dialogs
		CLogWindow* m_LogWindow;
		CRegisterWindow* m_RegisterWindow;
		CBreakPointWindow* m_BreakpointWindow;
		CMemoryWindow* m_MemoryWindow;
		CJitWindow* m_JitWindow;

		CCodeView* codeview;
		wxListBox* callstack;
		wxListBox* symbols;
		wxListBox* callers;
		wxListBox* calls;
		Common::Event sync_event;

		wxBitmap m_Bitmaps[Bitmaps_max];

		DECLARE_EVENT_TABLE()

		void OnSymbolListChange(wxCommandEvent& event);
		void OnSymbolListContextMenu(wxContextMenuEvent& event);
		void OnCallstackListChange(wxCommandEvent& event);
		void OnCallersListChange(wxCommandEvent& event);
		void OnCallsListChange(wxCommandEvent& event);
		void OnCodeStep(wxCommandEvent& event);
		void OnCodeViewChange(wxCommandEvent &event);

		void SingleCPUStep();

		void OnAddrBoxChange(wxCommandEvent& event);

		void OnToggleRegisterWindow(wxCommandEvent& event);
		void OnToggleBreakPointWindow(wxCommandEvent& event);
		void OnToggleLogWindow(wxCommandEvent& event);
		void OnToggleMemoryWindow(wxCommandEvent& event);
		void OnHostMessage(wxCommandEvent& event);
		void OnSymbolsMenu(wxCommandEvent& event);
		void OnJitMenu(wxCommandEvent& event);
		void OnInterpreter(wxCommandEvent& event);

		void CreateMenu(const SCoreStartupParameter& _LocalCoreStartupParameter);

		void UpdateButtonStates();
		void UpdateLists();

		void RecreateToolbar();
		void PopulateToolbar(wxToolBar* toolBar);
		void InitBitmaps();
		void CreateGUIControls(const SCoreStartupParameter& _LocalCoreStartupParameter);
		void OnKeyDown(wxKeyEvent& event);
};

#endif /*CODEWINDOW_*/
