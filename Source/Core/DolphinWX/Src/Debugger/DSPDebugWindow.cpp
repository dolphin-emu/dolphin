// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include <iostream>
#include <fstream>
#include <sstream>

#include <wx/artprov.h>

#include "../WxUtils.h"
#include "StringUtil.h"
#include "DSPDebugWindow.h"
#include "DSPRegisterView.h"
#include "CodeView.h"
#include "MemoryView.h"
#include "HW/DSPLLE/DSPSymbols.h"

DSPDebuggerLLE* m_DebuggerFrame = NULL;

BEGIN_EVENT_TABLE(DSPDebuggerLLE, wxPanel)	
	EVT_CLOSE(DSPDebuggerLLE::OnClose)
	EVT_MENU_RANGE(ID_RUNTOOL, ID_SHOWPCTOOL, DSPDebuggerLLE::OnChangeState)
	EVT_TEXT_ENTER(ID_ADDRBOX, DSPDebuggerLLE::OnAddrBoxChange)
	EVT_LISTBOX(ID_SYMBOLLIST, DSPDebuggerLLE::OnSymbolListChange)
END_EVENT_TABLE()


DSPDebuggerLLE::DSPDebuggerLLE(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id, wxDefaultPosition, wxDefaultSize,
			wxTAB_TRAVERSAL, _("DSP LLE Debugger"))
	, m_CachedStepCounter(-1)
{
	m_DebuggerFrame = this;

	// notify wxAUI which frame to use
	m_mgr.SetManagedWindow(this);
	m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

	m_Toolbar = new wxAuiToolBar(this, ID_TOOLBAR,
		wxDefaultPosition, wxDefaultSize, wxAUI_TB_HORZ_TEXT);
	m_Toolbar->AddTool(ID_RUNTOOL, wxT("Pause"),
		wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_OTHER, wxSize(10,10)));
	m_Toolbar->AddTool(ID_STEPTOOL, wxT("Step"),
		wxArtProvider::GetBitmap(wxART_GO_DOWN, wxART_OTHER, wxSize(10,10)));
	m_Toolbar->AddTool(ID_SHOWPCTOOL, wxT("Show PC"),
		wxArtProvider::GetBitmap(wxART_GO_TO_PARENT, wxART_OTHER, wxSize(10,10)));
	m_Toolbar->AddSeparator();
	m_Toolbar->AddControl(new wxTextCtrl(m_Toolbar, ID_ADDRBOX, wxEmptyString,
		wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER));
	m_Toolbar->Realize();

	m_SymbolList = new wxListBox(this, ID_SYMBOLLIST, wxDefaultPosition,
		wxSize(140, 100), 0, NULL, wxLB_SORT);

	m_MainNotebook = new wxAuiNotebook(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize,
		wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE);

	wxPanel *code_panel = new wxPanel(m_MainNotebook, wxID_ANY);
	wxBoxSizer *code_sizer = new wxBoxSizer(wxVERTICAL);
	m_CodeView = new CCodeView(&debug_interface, &DSPSymbols::g_dsp_symbol_db, code_panel);
	m_CodeView->SetPlain();
	code_sizer->Add(m_CodeView, 1, wxALL | wxEXPAND);
	code_panel->SetSizer(code_sizer);
	m_MainNotebook->AddPage(code_panel, wxT("Disasm"), true);

	wxPanel *mem_panel = new wxPanel(m_MainNotebook, wxID_ANY);
	wxBoxSizer *mem_sizer = new wxBoxSizer(wxVERTICAL);
	// TODO insert memViewer class
	m_MemView = new CMemoryView(&debug_interface, mem_panel);
	mem_sizer->Add(m_MemView, 1, wxALL | wxEXPAND);
	mem_panel->SetSizer(mem_sizer);
	m_MainNotebook->AddPage(mem_panel, wxT("Mem"));

	m_Regs = new DSPRegisterView(this, ID_DSP_REGS);

	// add the panes to the manager
	m_mgr.AddPane(m_Toolbar, wxAuiPaneInfo().
		ToolbarPane().Top().
		LeftDockable(false).RightDockable(false));

	m_mgr.AddPane(m_SymbolList, wxAuiPaneInfo().
		Left().CloseButton(false).
		Caption(wxT("Symbols")).Dockable(true));

	m_mgr.AddPane(m_MainNotebook, wxAuiPaneInfo().
		Name(wxT("m_MainNotebook")).Center().
		CloseButton(false).MaximizeButton(true));

	m_mgr.AddPane(m_Regs, wxAuiPaneInfo().Right().
		CloseButton(false).Caption(wxT("Registers")).
		Dockable(true));

	UpdateState();

	m_mgr.Update();
}

DSPDebuggerLLE::~DSPDebuggerLLE()
{
	m_mgr.UnInit();
	m_DebuggerFrame = NULL;
}

void DSPDebuggerLLE::OnClose(wxCloseEvent& event)
{
	event.Skip();
}

void DSPDebuggerLLE::OnChangeState(wxCommandEvent& event)
{
	if (DSPCore_GetState() == DSPCORE_STOP)
		return;

	switch (event.GetId())
	{
		case ID_RUNTOOL:
			if (DSPCore_GetState() == DSPCORE_RUNNING)
				DSPCore_SetState(DSPCORE_STEPPING);
			else
				DSPCore_SetState(DSPCORE_RUNNING);
			break;

		case ID_STEPTOOL:
			if (DSPCore_GetState() == DSPCORE_STEPPING)
			{
				DSPCore_Step();
				Update();
			}
			break;

		case ID_SHOWPCTOOL:
			FocusOnPC();
			break;
	}

	UpdateState();
	m_mgr.Update();
}

void Host_RefreshDSPDebuggerWindow()
{
	if (m_DebuggerFrame)
		m_DebuggerFrame->Update();
}

void DSPDebuggerLLE::Update()
{
#if defined __WXGTK__
	if (!wxIsMainThread())
		wxMutexGuiEnter();
#endif
	UpdateSymbolMap();
	UpdateDisAsmListView();
	UpdateRegisterFlags();
	UpdateState();
	m_mgr.Update();
#if defined __WXGTK__
	if (!wxIsMainThread())
		wxMutexGuiLeave();
#endif
}

void DSPDebuggerLLE::FocusOnPC()
{
	JumpToAddress(g_dsp.pc);
}

void DSPDebuggerLLE::UpdateState()
{
	if (DSPCore_GetState() == DSPCORE_RUNNING)
	{
		m_Toolbar->SetToolLabel(ID_RUNTOOL, wxT("Pause"));
		m_Toolbar->SetToolBitmap(ID_RUNTOOL,
			wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_OTHER, wxSize(10,10)));
		m_Toolbar->EnableTool(ID_STEPTOOL, false);
	}
	else
	{
		m_Toolbar->SetToolLabel(ID_RUNTOOL, wxT("Run"));
		m_Toolbar->SetToolBitmap(ID_RUNTOOL,
			wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_OTHER, wxSize(10,10)));
		m_Toolbar->EnableTool(ID_STEPTOOL, true);
	}
	m_Toolbar->Realize();
}

void DSPDebuggerLLE::UpdateDisAsmListView()
{
	if (m_CachedStepCounter == g_dsp.step_counter)
		return;

	// show PC
	FocusOnPC();
	m_CachedStepCounter = g_dsp.step_counter;
	m_Regs->Update();
}

void DSPDebuggerLLE::UpdateSymbolMap()
{
	if (g_dsp.dram == NULL)
		return;

	m_SymbolList->Freeze();	// HyperIris: wx style fast filling
	m_SymbolList->Clear();
	for (SymbolDB::XFuncMap::iterator iter = DSPSymbols::g_dsp_symbol_db.GetIterator();
		 iter != DSPSymbols::g_dsp_symbol_db.End(); ++iter)
	{
		int idx = m_SymbolList->Append(StrToWxStr(iter->second.name));
		m_SymbolList->SetClientData(idx, (void*)&iter->second);
	}
	m_SymbolList->Thaw();
}

void DSPDebuggerLLE::OnSymbolListChange(wxCommandEvent& event)
{
	int index = m_SymbolList->GetSelection();
	if (index >= 0) {
		Symbol* pSymbol = static_cast<Symbol *>(m_SymbolList->GetClientData(index));
		if (pSymbol != NULL)
		{
			if (pSymbol->type == Symbol::SYMBOL_FUNCTION)
			{
				JumpToAddress(pSymbol->address);
			}
		}
	}
}

void DSPDebuggerLLE::UpdateRegisterFlags()
{
}

void DSPDebuggerLLE::OnAddrBoxChange(wxCommandEvent& event)
{
	wxTextCtrl* pAddrCtrl = (wxTextCtrl*)m_Toolbar->FindControl(ID_ADDRBOX);
	wxString txt = pAddrCtrl->GetValue();

	auto text = StripSpaces(WxStrToStr(txt));
	if (text.size())
	{
		u32 addr;
		sscanf(text.c_str(), "%04x", &addr);
		if (JumpToAddress(addr))
			pAddrCtrl->SetBackgroundColour(*wxWHITE);
		else
			pAddrCtrl->SetBackgroundColour(*wxRED);
	}
	event.Skip();
}

bool DSPDebuggerLLE::JumpToAddress(u16 addr)
{
	int page = m_MainNotebook->GetSelection();
	if (page == 0)
	{
		// Center on valid instruction in IRAM/IROM
		int new_line = DSPSymbols::Addr2Line(addr);
		if (new_line >= 0) {
			m_CodeView->Center(new_line);
			return true;
		}
	}
	else if (page == 1)
	{
		// Center on any location in any valid ROM/RAM
		int seg = addr >> 12;
		if (seg == 0 || seg == 1 ||
			seg == 8 || seg == 0xf)
		{
			m_MemView->Center(addr);
			return true;
		}
	}

	return false;
}
