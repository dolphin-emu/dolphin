// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <string>
#include <vector>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/thread.h>
#include <wx/toolbar.h>
#include <wx/aui/auibar.h>

#include "Common/BreakPoints.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PPCTables.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/JitWindow.h"
#include "DolphinWX/Debugger/RegisterWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"

CCodeWindow::CCodeWindow(const SConfig& _LocalCoreStartupParameter, CFrame *parent,
	wxWindowID id, const wxPoint& position, const wxSize& size, long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, Parent(parent)
	, m_RegisterWindow(nullptr)
	, m_WatchWindow(nullptr)
	, m_BreakpointWindow(nullptr)
	, m_MemoryWindow(nullptr)
	, m_JitWindow(nullptr)
	, m_SoundWindow(nullptr)
	, m_VideoWindow(nullptr)
	, codeview(nullptr)
{
	InitBitmaps();

	DebugInterface* di = &PowerPC::debug_interface;

	codeview = new CCodeView(di, &g_symbolDB, this, wxID_ANY);

	callstack = new wxListBox(this, wxID_ANY);
	callstack->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallstackListChange, this);

	symbols = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
	symbols->Bind(wxEVT_LISTBOX, &CCodeWindow::OnSymbolListChange, this);

	calls = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
	calls->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallsListChange, this);

	callers = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
	callers->Bind(wxEVT_LISTBOX, &CCodeWindow::OnCallersListChange, this);

	m_aui_toolbar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxAUI_TB_HORIZONTAL | wxAUI_TB_PLAIN_BACKGROUND);

	wxSearchCtrl* const address_searchctrl = new wxSearchCtrl(m_aui_toolbar, IDM_ADDRBOX);
	address_searchctrl->Bind(wxEVT_TEXT, &CCodeWindow::OnAddrBoxChange, this);
	address_searchctrl->SetDescriptiveText(_("Search Address"));

	m_aui_toolbar->AddControl(address_searchctrl);
	m_aui_toolbar->Realize();

	m_aui_manager.SetManagedWindow(this);
	m_aui_manager.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);
	m_aui_manager.AddPane(m_aui_toolbar, wxAuiPaneInfo().ToolbarPane().Top().Floatable(false));
	m_aui_manager.AddPane(callstack, wxAuiPaneInfo().MinSize(150, 100).Left().CloseButton(false).Floatable(false).Caption(_("Callstack")));
	m_aui_manager.AddPane(symbols, wxAuiPaneInfo().MinSize(150, 100).Left().CloseButton(false).Floatable(false).Caption(_("Symbols")));
	m_aui_manager.AddPane(calls, wxAuiPaneInfo().MinSize(150, 100).Left().CloseButton(false).Floatable(false).Caption(_("Function calls")));
	m_aui_manager.AddPane(callers, wxAuiPaneInfo().MinSize(150, 100).Left().CloseButton(false).Floatable(false).Caption(_("Function callers")));
	m_aui_manager.AddPane(codeview, wxAuiPaneInfo().CenterPane().CloseButton(false).Floatable(false));
	m_aui_manager.Update();

	// Menu
	Bind(wxEVT_MENU, &CCodeWindow::OnCPUMode, this, IDM_INTERPRETER, IDM_JIT_SR_OFF);
	Bind(wxEVT_MENU, &CCodeWindow::OnChangeFont, this, IDM_FONT_PICKER);
	Bind(wxEVT_MENU, &CCodeWindow::OnJitMenu, this, IDM_CLEAR_CODE_CACHE, IDM_SEARCH_INSTRUCTION);
	Bind(wxEVT_MENU, &CCodeWindow::OnSymbolsMenu, this, IDM_CLEAR_SYMBOLS, IDM_PATCH_HLE_FUNCTIONS);
	Bind(wxEVT_MENU, &CCodeWindow::OnProfilerMenu, this, IDM_PROFILE_BLOCKS, IDM_WRITE_PROFILE);

	// Toolbar
	Bind(wxEVT_MENU, &CCodeWindow::OnCodeStep, this, IDM_STEP, IDM_GOTOPC);

	// Other
	Bind(wxEVT_HOST_COMMAND, &CCodeWindow::OnHostMessage, this);
}

CCodeWindow::~CCodeWindow()
{
	m_aui_manager.UnInit();
}

wxMenuBar *CCodeWindow::GetMenuBar()
{
	return Parent->GetMenuBar();
}

wxToolBar *CCodeWindow::GetToolBar()
{
	return Parent->m_ToolBar;
}

// ----------
// Events

void CCodeWindow::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_NOTIFY_MAP_LOADED:
			NotifyMapLoaded();
			if (m_BreakpointWindow) m_BreakpointWindow->NotifyUpdate();
			break;

		case IDM_UPDATE_DISASM_DIALOG:
			Update();
			if (codeview) codeview->Center(PC);
			if (m_RegisterWindow) m_RegisterWindow->NotifyUpdate();
			if (m_WatchWindow) m_WatchWindow->NotifyUpdate();
			break;

		case IDM_UPDATE_BREAKPOINTS:
			Update();
			if (m_BreakpointWindow) m_BreakpointWindow->NotifyUpdate();
			break;

		case IDM_UPDATE_JIT_PANE:
			// Check if the JIT pane is in the AUI notebook. If not, add it and switch to it.
			if (!m_JitWindow)
				ToggleJitWindow(true);
			m_JitWindow->ViewAddr(codeview->GetSelection());
			break;
	}
}

// The Play, Stop, Step, Skip, Go to PC and Show PC buttons go here
void CCodeWindow::OnCodeStep(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_STEP:
			SingleStep();
			break;

		case IDM_STEPOVER:
			StepOver();
			break;

		case IDM_STEPOUT:
			StepOut();
			break;

		case IDM_TOGGLE_BREAKPOINT:
			ToggleBreakpoint();
			break;

		case IDM_SKIP:
			PC += 4;
			Update();
			break;

		case IDM_SETPC:
			PC = codeview->GetSelection();
			Update();
			break;

		case IDM_GOTOPC:
			JumpToAddress(PC);
			break;
	}

	UpdateButtonStates();
	// Update all toolbars in the aui manager
	Parent->UpdateGUI();
}

bool CCodeWindow::JumpToAddress(u32 address)
{
	// Jump to anywhere in memory
	if (address <= 0xFFFFFFFF)
	{
		codeview->Center(address);
		UpdateLists();

		return true;
	}

	return false;
}

void CCodeWindow::OnCodeViewChange(wxCommandEvent &event)
{
	UpdateLists();
}

void CCodeWindow::OnAddrBoxChange(wxCommandEvent& event)
{
	wxSearchCtrl* pAddrCtrl = (wxSearchCtrl*)m_aui_toolbar->FindControl(IDM_ADDRBOX);

	// Trim leading and trailing whitespace.
	wxString txt = pAddrCtrl->GetValue().Trim().Trim(false);

	bool success = false;
	unsigned long addr;
	if (txt.ToULong(&addr, 16))
	{
		if (JumpToAddress(addr))
			success = true;
	}

	if (success)
		pAddrCtrl->SetBackgroundColour(wxNullColour);
	else if (!txt.empty())
		pAddrCtrl->SetBackgroundColour(*wxRED);

	pAddrCtrl->Refresh();

	event.Skip();
}

void CCodeWindow::OnCallstackListChange(wxCommandEvent& event)
{
	int index   = callstack->GetSelection();
	if (index >= 0)
	{
		u32 address = (u32)(u64)(callstack->GetClientData(index));
		if (address)
			JumpToAddress(address);
	}
}

void CCodeWindow::OnCallersListChange(wxCommandEvent& event)
{
	int index = callers->GetSelection();
	if (index >= 0)
	{
		u32 address = (u32)(u64)(callers->GetClientData(index));
		if (address)
			JumpToAddress(address);
	}
}

void CCodeWindow::OnCallsListChange(wxCommandEvent& event)
{
	int index = calls->GetSelection();
	if (index >= 0)
	{
		u32 address = (u32)(u64)(calls->GetClientData(index));
		if (address)
			JumpToAddress(address);
	}
}

void CCodeWindow::SingleStep()
{
	if (CPU::IsStepping())
	{
		PowerPC::breakpoints.ClearAllTemporary();
		JitInterface::InvalidateICache(PC, 4, true);
		CPU::StepOpcode(&sync_event);
		wxThread::Sleep(20);
		// need a short wait here
		JumpToAddress(PC);
		Update();
	}
}

void CCodeWindow::StepOver()
{
	if (CPU::IsStepping())
	{
		UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
		if (inst.LK)
		{
			PowerPC::breakpoints.ClearAllTemporary();
			PowerPC::breakpoints.Add(PC + 4, true);
			CPU::EnableStepping(false);
			JumpToAddress(PC);
			Update();
		}
		else
		{
			SingleStep();
		}

		UpdateButtonStates();
		// Update all toolbars in the aui manager
		Parent->UpdateGUI();
	}
}

void CCodeWindow::StepOut()
{
	if (CPU::IsStepping())
	{
		PowerPC::breakpoints.ClearAllTemporary();

		// Keep stepping until the next blr or timeout after one second
		u64 timeout = SystemTimers::GetTicksPerSecond();
		u64 steps = 0;
		PowerPC::CoreMode oldMode = PowerPC::GetMode();
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
		UGeckoInstruction inst = PowerPC::HostRead_Instruction(PC);
		while (inst.hex != 0x4e800020 && steps < timeout) // check for blr
		{
			if (inst.LK)
			{
				// Step over branches
				u32 next_pc = PC + 4;
				while (PC != next_pc && steps < timeout)
				{
					PowerPC::SingleStep();
					++steps;
				}
			}
			else
			{
				PowerPC::SingleStep();
				++steps;
			}
			inst = PowerPC::HostRead_Instruction(PC);
		}

		PowerPC::SingleStep();
		PowerPC::SetMode(oldMode);

		JumpToAddress(PC);
		Update();

		UpdateButtonStates();
		// Update all toolbars in the aui manager
		Parent->UpdateGUI();
	}
}

void CCodeWindow::ToggleBreakpoint()
{
	if (CPU::IsStepping())
	{
		if (codeview) codeview->ToggleBreakpoint(codeview->GetSelection());
		Update();
	}
}

void CCodeWindow::UpdateLists()
{
	callers->Clear();
	u32 addr = codeview->GetSelection();
	Symbol *symbol = g_symbolDB.GetSymbolFromAddr(addr);
	if (!symbol)
		return;

	for (auto& call : symbol->callers)
	{
		u32 caller_addr = call.callAddress;
		Symbol *caller_symbol = g_symbolDB.GetSymbolFromAddr(caller_addr);
		if (caller_symbol)
		{
			int idx = callers->Append(StrToWxStr(StringFromFormat
						("< %s (%08x)", caller_symbol->name.c_str(), caller_addr).c_str()));
			callers->SetClientData(idx, (void*)(u64)caller_addr);
		}
	}

	calls->Clear();
	for (auto& call : symbol->calls)
	{
		u32 call_addr = call.function;
		Symbol *call_symbol = g_symbolDB.GetSymbolFromAddr(call_addr);
		if (call_symbol)
		{
			int idx = calls->Append(StrToWxStr(StringFromFormat
						("> %s (%08x)", call_symbol->name.c_str(), call_addr).c_str()));
			calls->SetClientData(idx, (void*)(u64)call_addr);
		}
	}
}

void CCodeWindow::UpdateCallstack()
{
	if (Core::GetState() == Core::CORE_STOPPING) return;

	callstack->Clear();

	std::vector<Dolphin_Debugger::CallstackEntry> stack;

	bool ret = Dolphin_Debugger::GetCallstack(stack);

	for (auto& frame : stack)
	{
		int idx = callstack->Append(StrToWxStr(frame.Name));
		callstack->SetClientData(idx, (void*)(u64)frame.vAddress);
	}

	if (!ret)
		callstack->Append(StrToWxStr("invalid callstack"));
}

// Create CPU Mode menus
void CCodeWindow::CreateMenu(const SConfig& core_startup_parameter, wxMenuBar *pMenuBar)
{
	// CPU Mode
	wxMenu* pCoreMenu = new wxMenu;

	wxMenuItem* interpreter = pCoreMenu->Append(IDM_INTERPRETER, _("&Interpreter core"),
		_("This is necessary to get break points"
		" and stepping to work as explained in the Developer Documentation. But it can be very"
		" slow, perhaps slower than 1 fps."),
		wxITEM_CHECK);
	interpreter->Check(core_startup_parameter.iCPUCore == PowerPC::CORE_INTERPRETER);
	pCoreMenu->AppendSeparator();

	pCoreMenu->Append(IDM_JIT_NO_BLOCK_LINKING, _("&JIT Block Linking off"),
		_("Provide safer execution by not linking the JIT blocks."),
		wxITEM_CHECK);

	pCoreMenu->Append(IDM_JIT_NO_BLOCK_CACHE, _("&Disable JIT Cache"),
		_("Avoid any involuntary JIT cache clearing, this may prevent Zelda TP from crashing.\n[This option must be selected before a game is started.]"),
		wxITEM_CHECK);
	pCoreMenu->Append(IDM_CLEAR_CODE_CACHE, _("&Clear JIT cache"));

	pCoreMenu->AppendSeparator();
	pCoreMenu->Append(IDM_LOG_INSTRUCTIONS, _("&Log JIT instruction coverage"));
	pCoreMenu->Append(IDM_SEARCH_INSTRUCTION, _("&Search for an op"));

	pCoreMenu->AppendSeparator();
	pCoreMenu->Append(IDM_JIT_OFF, _("&JIT off (JIT core)"),
		_("Turn off all JIT functions, but still use the JIT core from Jit.cpp"),
		wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LS_OFF, _("&JIT LoadStore off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LSLBZX_OFF, _("    &JIT LoadStore lbzx off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LSLXZ_OFF, _("    &JIT LoadStore lXz off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LSLWZ_OFF, _("&JIT LoadStore lwz off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LSF_OFF, _("&JIT LoadStore Floating off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_LSP_OFF, _("&JIT LoadStore Paired off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_FP_OFF, _("&JIT FloatingPoint off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_I_OFF, _("&JIT Integer off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_P_OFF, _("&JIT Paired off"),
			wxEmptyString, wxITEM_CHECK);
	pCoreMenu->Append(IDM_JIT_SR_OFF, _("&JIT SystemRegisters off"),
			wxEmptyString, wxITEM_CHECK);

	pMenuBar->Append(pCoreMenu, _("&JIT"));


	// Debug Menu
	wxMenu* pDebugMenu = new wxMenu;

	pDebugMenu->Append(IDM_STEP, _("Step &Into\tF11"));
	pDebugMenu->Append(IDM_STEPOVER, _("Step &Over\tF10"));
	pDebugMenu->Append(IDM_STEPOUT, _("Step O&ut\tSHIFT+F11"));
	pDebugMenu->Append(IDM_TOGGLE_BREAKPOINT, _("Toggle &Breakpoint\tF9"));
	pDebugMenu->AppendSeparator();

	wxMenu* pPerspectives = new wxMenu;
	Parent->m_SavedPerspectives = new wxMenu;
	pDebugMenu->AppendSubMenu(pPerspectives, _("Perspectives"), _("Edit Perspectives"));
	pPerspectives->Append(IDM_SAVE_PERSPECTIVE, _("Save perspectives"), _("Save currently-toggled perspectives"));
	pPerspectives->Append(IDM_EDIT_PERSPECTIVES, _("Edit perspectives"), _("Toggle editing of perspectives"), wxITEM_CHECK);
	pPerspectives->AppendSeparator();
	pPerspectives->Append(IDM_ADD_PERSPECTIVE, _("Create new perspective"));
	pPerspectives->AppendSubMenu(Parent->m_SavedPerspectives, _("Saved perspectives"));
	Parent->PopulateSavedPerspectives();
	pPerspectives->AppendSeparator();
	wxMenu* pAddPane = new wxMenu;
	pPerspectives->AppendSubMenu(pAddPane, _("Add new pane to"));
	pAddPane->Append(IDM_PERSPECTIVES_ADD_PANE_TOP, _("Top"));
	pAddPane->Append(IDM_PERSPECTIVES_ADD_PANE_BOTTOM, _("Bottom"));
	pAddPane->Append(IDM_PERSPECTIVES_ADD_PANE_LEFT, _("Left"));
	pAddPane->Append(IDM_PERSPECTIVES_ADD_PANE_RIGHT, _("Right"));
	pAddPane->Append(IDM_PERSPECTIVES_ADD_PANE_CENTER, _("Center"));
	pPerspectives->Append(IDM_TAB_SPLIT, _("Tab split"), "", wxITEM_CHECK);
	pPerspectives->Append(IDM_NO_DOCKING, _("Disable docking"), "Disable docking of perspective panes to main window", wxITEM_CHECK);


	pMenuBar->Append(pDebugMenu, _("&Debug"));

	CreateMenuSymbols(pMenuBar);
}

void CCodeWindow::CreateMenuOptions(wxMenu* pMenu)
{
	wxMenuItem* boottopause = pMenu->Append(IDM_BOOT_TO_PAUSE, _("Boot to pause"),
		_("Start the game directly instead of booting to pause"),
		wxITEM_CHECK);
	boottopause->Check(bBootToPause);

	wxMenuItem* automaticstart = pMenu->Append(IDM_AUTOMATIC_START, _("&Automatic start"),
		_(
		"Automatically load the Default ISO when Dolphin starts, or the last game you loaded,"
		" if you have not given it an elf file with the --elf command line. [This can be"
		" convenient if you are bug-testing with a certain game and want to rebuild"
		" and retry it several times, either with changes to Dolphin or if you are"
		" developing a homebrew game.]"),
		wxITEM_CHECK);
	automaticstart->Check(bAutomaticStart);

	pMenu->Append(IDM_FONT_PICKER, _("&Font..."));
}

// CPU Mode and JIT Menu
void CCodeWindow::OnCPUMode(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_INTERPRETER:
			PowerPC::SetMode(UseInterpreter() ? PowerPC::MODE_INTERPRETER : PowerPC::MODE_JIT);
			break;
		case IDM_BOOT_TO_PAUSE:
			bBootToPause = !bBootToPause;
			return;
		case IDM_AUTOMATIC_START:
			bAutomaticStart = !bAutomaticStart;
			return;
		case IDM_JIT_OFF:
			SConfig::GetInstance().bJITOff = event.IsChecked();
			break;
		case IDM_JIT_LS_OFF:
			SConfig::GetInstance().bJITLoadStoreOff = event.IsChecked();
			break;
		case IDM_JIT_LSLXZ_OFF:
			SConfig::GetInstance().bJITLoadStorelXzOff = event.IsChecked();
			break;
		case IDM_JIT_LSLWZ_OFF:
			SConfig::GetInstance().bJITLoadStorelwzOff = event.IsChecked();
			break;
		case IDM_JIT_LSLBZX_OFF:
			SConfig::GetInstance().bJITLoadStorelbzxOff = event.IsChecked();
			break;
		case IDM_JIT_LSF_OFF:
			SConfig::GetInstance().bJITLoadStoreFloatingOff = event.IsChecked();
			break;
		case IDM_JIT_LSP_OFF:
			SConfig::GetInstance().bJITLoadStorePairedOff = event.IsChecked();
			break;
		case IDM_JIT_FP_OFF:
			SConfig::GetInstance().bJITFloatingPointOff = event.IsChecked();
			break;
		case IDM_JIT_I_OFF:
			SConfig::GetInstance().bJITIntegerOff = event.IsChecked();
			break;
		case IDM_JIT_P_OFF:
			SConfig::GetInstance().bJITPairedOff = event.IsChecked();
			break;
		case IDM_JIT_SR_OFF:
			SConfig::GetInstance().bJITSystemRegistersOff = event.IsChecked();
			break;
	}

	// Clear the JIT cache to enable these changes
	JitInterface::ClearCache();

	// Update
	UpdateButtonStates();
}

void CCodeWindow::OnJitMenu(wxCommandEvent& event)
{
	switch (event.GetId())
	{
		case IDM_LOG_INSTRUCTIONS:
			PPCTables::LogCompiledInstructions();
			break;

		case IDM_CLEAR_CODE_CACHE:
			JitInterface::ClearCache();
			break;

		case IDM_SEARCH_INSTRUCTION:
		{
			wxString str = wxGetTextFromUser("", _("Op?"), wxEmptyString, this);
			auto const wx_name = WxStrToStr(str);
			bool found = false;
			for (u32 addr = 0x80000000; addr < 0x80180000; addr += 4)
			{
				const char *name = PPCTables::GetInstructionName(PowerPC::HostRead_U32(addr));
				if (name && (wx_name == name))
				{
					NOTICE_LOG(POWERPC, "Found %s at %08x", wx_name.c_str(), addr);
					found = true;
				}
			}
			if (!found)
				NOTICE_LOG(POWERPC, "Opcode %s not found", wx_name.c_str());
			break;
		}
	}
}

// Shortcuts
bool CCodeWindow::UseInterpreter()
{
	return GetMenuBar()->IsChecked(IDM_INTERPRETER);
}

bool CCodeWindow::BootToPause()
{
	return GetMenuBar()->IsChecked(IDM_BOOT_TO_PAUSE);
}

bool CCodeWindow::AutomaticStart()
{
	return GetMenuBar()->IsChecked(IDM_AUTOMATIC_START);
}

bool CCodeWindow::JITNoBlockCache()
{
	return GetMenuBar()->IsChecked(IDM_JIT_NO_BLOCK_CACHE);
}

bool CCodeWindow::JITNoBlockLinking()
{
	return GetMenuBar()->IsChecked(IDM_JIT_NO_BLOCK_LINKING);
}

// Toolbar
void CCodeWindow::InitBitmaps()
{
	m_Bitmaps[Toolbar_Step] = WxUtils::LoadResourceBitmap("toolbar_debugger_step");
	m_Bitmaps[Toolbar_StepOver] = WxUtils::LoadResourceBitmap("toolbar_debugger_step_over");
	m_Bitmaps[Toolbar_StepOut] = WxUtils::LoadResourceBitmap("toolbar_debugger_step_out");
	m_Bitmaps[Toolbar_Skip] = WxUtils::LoadResourceBitmap("toolbar_debugger_skip");
	m_Bitmaps[Toolbar_GotoPC] = WxUtils::LoadResourceBitmap("toolbar_debugger_goto_pc");
	m_Bitmaps[Toolbar_SetPC] = WxUtils::LoadResourceBitmap("toolbar_debugger_set_pc");
}

void CCodeWindow::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[0].GetWidth(),
		h = m_Bitmaps[0].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	WxUtils::AddToolbarButton(toolBar, IDM_STEP,     _("Step"),      m_Bitmaps[Toolbar_Step],     _("Step into the next instruction"));
	WxUtils::AddToolbarButton(toolBar, IDM_STEPOVER, _("Step Over"), m_Bitmaps[Toolbar_StepOver], _("Step over the next instruction"));
	WxUtils::AddToolbarButton(toolBar, IDM_STEPOUT,  _("Step Out"),  m_Bitmaps[Toolbar_StepOut],  _("Step out of the current function"));
	WxUtils::AddToolbarButton(toolBar, IDM_SKIP,     _("Skip"),      m_Bitmaps[Toolbar_Skip],     _("Skips the next instruction completely"));
	toolBar->AddSeparator();
	WxUtils::AddToolbarButton(toolBar, IDM_GOTOPC,   _("Show PC"),   m_Bitmaps[Toolbar_GotoPC],   _("Go to the current instruction"));
	WxUtils::AddToolbarButton(toolBar, IDM_SETPC,    _("Set PC"),    m_Bitmaps[Toolbar_SetPC],    _("Set the current instruction"));
}

// Update GUI
void CCodeWindow::Update()
{
	if (!codeview) return;

	codeview->Refresh();
	UpdateCallstack();
	UpdateButtonStates();

	// Do not automatically show the current PC position when a breakpoint is hit or
	// when we pause since this can be called at other times too.
	//codeview->Center(PC);
}

void CCodeWindow::UpdateButtonStates()
{
	bool Initialized = (Core::GetState() != Core::CORE_UNINITIALIZED);
	bool Pause = (Core::GetState() == Core::CORE_PAUSE);
	bool Stepping = CPU::IsStepping();
	wxToolBar* ToolBar = GetToolBar();

	// Toolbar
	if (!ToolBar)
		return;

	if (!Initialized)
	{
		ToolBar->EnableTool(IDM_STEPOVER, false);
		ToolBar->EnableTool(IDM_STEPOUT, false);
		ToolBar->EnableTool(IDM_SKIP, false);
	}
	else
	{
		if (!Stepping)
		{
			ToolBar->EnableTool(IDM_STEPOVER, false);
			ToolBar->EnableTool(IDM_STEPOUT, false);
			ToolBar->EnableTool(IDM_SKIP, false);
		}
		else
		{
			ToolBar->EnableTool(IDM_STEPOVER, true);
			ToolBar->EnableTool(IDM_STEPOUT, true);
			ToolBar->EnableTool(IDM_SKIP, true);
		}
	}

	ToolBar->EnableTool(IDM_STEP, Initialized && Stepping);
	ToolBar->Realize();

	// Menu bar
	// ------------------
	GetMenuBar()->Enable(IDM_INTERPRETER, Pause); // CPU Mode

	GetMenuBar()->Enable(IDM_JIT_NO_BLOCK_CACHE, !Initialized);

	GetMenuBar()->Enable(IDM_JIT_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LS_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LSLXZ_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LSLWZ_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LSLBZX_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LSF_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_LSP_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_FP_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_I_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_P_OFF, Pause);
	GetMenuBar()->Enable(IDM_JIT_SR_OFF, Pause);

	GetMenuBar()->Enable(IDM_CLEAR_CODE_CACHE, Pause); // JIT Menu
	GetMenuBar()->Enable(IDM_SEARCH_INSTRUCTION, Initialized);

	GetMenuBar()->Enable(IDM_CLEAR_SYMBOLS, Initialized); // Symbols menu
	GetMenuBar()->Enable(IDM_SCAN_FUNCTIONS, Initialized);
	GetMenuBar()->Enable(IDM_LOAD_MAP_FILE, Initialized);
	GetMenuBar()->Enable(IDM_SAVEMAPFILE, Initialized);
	GetMenuBar()->Enable(IDM_LOAD_MAP_FILE_AS, Initialized);
	GetMenuBar()->Enable(IDM_SAVE_MAP_FILE_AS, Initialized);
	GetMenuBar()->Enable(IDM_LOAD_BAD_MAP_FILE, Initialized);
	GetMenuBar()->Enable(IDM_SAVE_MAP_FILE_WITH_CODES, Initialized);
	GetMenuBar()->Enable(IDM_CREATE_SIGNATURE_FILE, Initialized);
	GetMenuBar()->Enable(IDM_APPEND_SIGNATURE_FILE, Initialized);
	GetMenuBar()->Enable(IDM_COMBINE_SIGNATURE_FILES, Initialized);
	GetMenuBar()->Enable(IDM_RENAME_SYMBOLS, Initialized);
	GetMenuBar()->Enable(IDM_USE_SIGNATURE_FILE, Initialized);
	GetMenuBar()->Enable(IDM_PATCH_HLE_FUNCTIONS, Initialized);

	// Update Fonts
	callstack->SetFont(DebuggerFont);
	symbols->SetFont(DebuggerFont);
	callers->SetFont(DebuggerFont);
	calls->SetFont(DebuggerFont);
}
