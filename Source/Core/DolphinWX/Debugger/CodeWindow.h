// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/bitmap.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "DolphinWX/Globals.h"

class CFrame;
class CRegisterWindow;
class CBreakPointWindow;
class CMemoryWindow;
class CJitWindow;
class CCodeView;
class DSPDebuggerLLE;
class GFXDebuggerPanel;
struct SCoreStartupParameter;

class wxToolBar;
class wxListBox;
class wxMenu;
class wxMenuBar;

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
		wxToolBar * GetToolBar();
		wxBitmap m_Bitmaps[ToolbarDebugBitmapMax];

		bool UseInterpreter();
		bool BootToPause();
		bool AutomaticStart();
		bool JITNoBlockCache();
		bool JITNoBlockLinking();
		bool JumpToAddress(u32 address);

		void Update() override;
		void NotifyMapLoaded();
		void CreateMenu(const SCoreStartupParameter& _LocalCoreStartupParameter, wxMenuBar *pMenuBar);
		void CreateMenuOptions(wxMenu *pMenu);
		void CreateMenuSymbols(wxMenuBar *pMenuBar);
		void RecreateToolbar(wxToolBar*);
		void PopulateToolbar(wxToolBar* toolBar);
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

		void InitBitmaps();

		CCodeView* codeview;
		wxListBox* callstack;
		wxListBox* symbols;
		wxListBox* callers;
		wxListBox* calls;
		Common::Event sync_event;

		DECLARE_EVENT_TABLE()
};
