// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/bitmap.h>
#include <wx/panel.h>
#include <wx/aui/framemanager.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "DolphinWX/Globals.h"

class CFrame;
class CRegisterWindow;
class CWatchWindow;
class CBreakPointWindow;
class CMemoryWindow;
class CJitWindow;
class CCodeView;
class DSPDebuggerLLE;
class GFXDebuggerPanel;
struct SConfig;

class wxAuiToolBar;
class wxListBox;
class wxMenu;
class wxMenuBar;
class wxToolBar;

class CCodeWindow : public wxPanel
{
public:
	CCodeWindow(const SConfig& _LocalCoreStartupParameter,
	            CFrame * parent,
	            wxWindowID id = wxID_ANY,
	            const wxPoint& pos = wxDefaultPosition,
	            const wxSize& size = wxDefaultSize,
	            long style = wxTAB_TRAVERSAL | wxBORDER_NONE,
	            const wxString& name = _("Code"));
	~CCodeWindow();

	void Load();
	void Save();

	// Parent interaction
	CFrame *Parent;
	wxMenuBar * GetMenuBar();
	wxToolBar * GetToolBar();
	wxBitmap m_Bitmaps[Toolbar_Debug_Bitmap_Max];

	bool UseInterpreter();
	bool BootToPause();
	bool AutomaticStart();
	bool JITNoBlockCache();
	bool JITNoBlockLinking();
	bool JumpToAddress(u32 address);

	void Update() override;
	void NotifyMapLoaded();
	void CreateMenu(const SConfig& _LocalCoreStartupParameter, wxMenuBar *pMenuBar);
	void CreateMenuOptions(wxMenu *pMenu);
	void CreateMenuSymbols(wxMenuBar *pMenuBar);
	void PopulateToolbar(wxToolBar* toolBar);
	void UpdateButtonStates();
	void OpenPages();

	// Menu bar
	void ToggleCodeWindow(bool bShow);
	void ToggleRegisterWindow(bool bShow);
	void ToggleWatchWindow(bool bShow);
	void ToggleBreakPointWindow(bool bShow);
	void ToggleMemoryWindow(bool bShow);
	void ToggleJitWindow(bool bShow);
	void ToggleSoundWindow(bool bShow);
	void ToggleVideoWindow(bool bShow);

	// Sub dialogs
	CRegisterWindow* m_RegisterWindow;
	CWatchWindow* m_WatchWindow;
	CBreakPointWindow* m_BreakpointWindow;
	CMemoryWindow* m_MemoryWindow;
	CJitWindow* m_JitWindow;
	DSPDebuggerLLE* m_SoundWindow;
	GFXDebuggerPanel* m_VideoWindow;

	// Settings
	bool bAutomaticStart; bool bBootToPause;
	bool bShowOnStart[IDM_VIDEO_WINDOW - IDM_LOG_WINDOW + 1];
	int iNbAffiliation[IDM_CODE_WINDOW - IDM_LOG_WINDOW + 1];

private:
	void OnCPUMode(wxCommandEvent& event);

	void OnChangeFont(wxCommandEvent& event);

	void OnCodeStep(wxCommandEvent& event);
	void OnAddrBoxChange(wxCommandEvent& event);
	void OnSymbolsMenu(wxCommandEvent& event);
	void OnJitMenu(wxCommandEvent& event);
	void OnProfilerMenu(wxCommandEvent& event);

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
	void StepOut();
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

	wxAuiManager  m_aui_manager;
	wxAuiToolBar* m_aui_toolbar;
};
