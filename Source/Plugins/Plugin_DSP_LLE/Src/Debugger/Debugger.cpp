// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h" // Common
#include <iostream> // System
#include <fstream>
#include <sstream>

#include "Debugger.h"
#include "DSPRegisterView.h"

// Event table and class
BEGIN_EVENT_TABLE(DSPDebuggerLLE, wxFrame)	
	EVT_CLOSE(DSPDebuggerLLE::OnClose)

	EVT_MENU_RANGE(ID_RUNTOOL, ID_STEPTOOL, DSPDebuggerLLE::OnChangeState)
	EVT_MENU(ID_SHOWPCTOOL, DSPDebuggerLLE::OnShowPC)

	EVT_LIST_ITEM_RIGHT_CLICK(ID_DISASM, DSPDebuggerLLE::OnRightClick)
	EVT_LIST_ITEM_ACTIVATED(ID_DISASM, DSPDebuggerLLE::OnDoubleClick)
END_EVENT_TABLE()

DSPDebuggerLLE::DSPDebuggerLLE(wxWindow *parent, wxWindowID id, const wxString &title,
							   const wxPoint &position, const wxSize& size, long style)
							   : wxFrame(parent, id, title, position, size, style)
							   , m_CachedStepCounter(-1)
							   , m_CachedCR(-1)
							   , m_State(RUN)
							   , m_CachedUCodeCRC(-1)
{
	CreateGUIControls();
}

DSPDebuggerLLE::~DSPDebuggerLLE()
{
}

void DSPDebuggerLLE::CreateGUIControls()
{
	// Basic settings
	SetSize(700, 500);
	this->SetSizeHints(700, 500);
	this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

	m_Toolbar = CreateToolBar(wxTB_NODIVIDER|wxTB_NOICONS|wxTB_HORZ_TEXT|wxTB_DOCKABLE, ID_TOOLBAR); 
	m_Toolbar->AddTool(ID_RUNTOOL, wxT("Run"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
	m_Toolbar->AddTool(ID_STEPTOOL, wxT("Step"), wxNullBitmap, wxT("Step Code "), wxITEM_NORMAL);
	m_Toolbar->AddTool(ID_SHOWPCTOOL, wxT("Show Pc"), wxNullBitmap, wxT("Reset To PC counter"), wxITEM_NORMAL);
	m_Toolbar->AddTool(ID_JUMPTOTOOL, wxT("Jump"), wxNullBitmap, wxT("Jump to a specific Address"), wxITEM_NORMAL);
	m_Toolbar->AddSeparator();
	m_Toolbar->AddCheckTool(ID_CHECK_ASSERTINT, wxT("AssertInt"), wxNullBitmap, wxNullBitmap, wxEmptyString);
	m_Toolbar->AddCheckTool(ID_CHECK_HALT, wxT("Halt"), wxNullBitmap, wxNullBitmap, wxEmptyString);
	m_Toolbar->AddCheckTool(ID_CHECK_INIT, wxT("Init"), wxNullBitmap, wxNullBitmap, wxEmptyString);
	m_Toolbar->Realize();

	wxBoxSizer* sMain = new wxBoxSizer(wxHORIZONTAL);

	m_Disasm = new wxListCtrl(this, ID_DISASM, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
	sMain->Add(m_Disasm, 4, wxALL|wxEXPAND, 5);

	wxStaticLine* m_staticline = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL);
	sMain->Add(m_staticline, 0, wxEXPAND|wxALL, 5);

	m_Regs = new DSPRegisterView(this, ID_DSP_REGS);
	sMain->Add(m_Regs, 1, wxEXPAND|wxALL, 5);

	this->SetSizer(sMain);
	this->Layout();

	// Add the disasm columns
	m_Disasm->InsertColumn(COLUMN_BP, wxT("BP"), wxLIST_FORMAT_LEFT, 25);
	m_Disasm->InsertColumn(COLUMN_FUNCTION, wxT("Function"), wxLIST_FORMAT_LEFT, 160);
	m_Disasm->InsertColumn(COLUMN_ADDRESS, wxT("Address"), wxLIST_FORMAT_LEFT, 55);
	m_Disasm->InsertColumn(COLUMN_MNEMONIC, wxT("Mnemonic"), wxLIST_FORMAT_LEFT, 55);
	m_Disasm->InsertColumn(COLUMN_OPCODE, wxT("Opcode"), wxLIST_FORMAT_LEFT, 60);
	m_Disasm->InsertColumn(COLUMN_EXT, wxT("Ext"), wxLIST_FORMAT_LEFT, 40);
	m_Disasm->InsertColumn(COLUMN_PARAM, wxT("Param"), wxLIST_FORMAT_LEFT, 500);
}

void DSPDebuggerLLE::OnClose(wxCloseEvent& event)
{
	wxWindow::Destroy();
	event.Skip();
}

void DSPDebuggerLLE::OnChangeState(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_RUNTOOL:
		if ((m_State == RUN) || (m_State == RUN_START))
			m_State = PAUSE;
		else
			m_State = RUN_START;
		break;

	case ID_STEPTOOL:
		m_State = STEP;
		break;
	}

	UpdateState();
}

void DSPDebuggerLLE::OnShowPC(wxCommandEvent& event)
{
	Refresh();
	FocusOnPC();
}

void DSPDebuggerLLE::OnRightClick(wxListEvent& event)
{
	long item = m_Disasm->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); 
	u16 SelectedPC = static_cast<u16>(m_Disasm->GetItemData(item));
	g_dsp.pc = SelectedPC;

	Refresh();
}

void DSPDebuggerLLE::OnDoubleClick(wxListEvent& event)
{
	long item = m_Disasm->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); 
	u16 SelectedPC = static_cast<u16>(m_Disasm->GetItemData(item));
	ToggleBreakPoint(SelectedPC);

	if (IsBreakPoint(SelectedPC))
		m_Disasm->SetItem(item, COLUMN_BP, wxT("*"));
	else
		m_Disasm->SetItem(item, COLUMN_BP, wxT(""));
}

void DSPDebuggerLLE::Refresh()
{
	UpdateSymbolMap();
	UpdateDisAsmListView();
	UpdateRegisterFlags();
	UpdateState();
}

void DSPDebuggerLLE::FocusOnPC()
{
	UnselectAll();

	for (int i = 0; i < m_Disasm->GetItemCount(); i++)
	{
		if (m_Disasm->GetItemData(i) == g_dsp.pc)
		{
			m_Disasm->EnsureVisible(i - 5);
			m_Disasm->EnsureVisible(i + 5);
			m_Disasm->SetItemState(i, wxLIST_STATE_FOCUSED|wxLIST_STATE_SELECTED, wxLIST_STATE_FOCUSED|wxLIST_STATE_SELECTED);
			break;
		}
	}
}

void DSPDebuggerLLE::UnselectAll()
{
	for (int i = 0; i < m_Disasm->GetItemCount(); i++)
	{
		m_Disasm->SetItemState(i, 0, wxLIST_STATE_SELECTED);
	}
}

void DSPDebuggerLLE::UpdateState()
{
	if ((m_State == RUN) || (m_State == RUN_START))
		m_Toolbar->FindById(ID_RUNTOOL)->SetLabel(wxT("Pause"));
	else
		m_Toolbar->FindById(ID_RUNTOOL)->SetLabel(wxT("Run"));
	m_Toolbar->Realize();
}

void DSPDebuggerLLE::RebuildDisAsmListView()
{
	m_Disasm->Freeze();
	m_Disasm->DeleteAllItems();

	char Buffer[256];
	gd_globals_t gdg;

	if (g_dsp.pc & 0x8000)
		gdg.binbuf = g_dsp.irom;
	else
		gdg.binbuf = g_dsp.iram;

	gdg.buffer = Buffer;
	gdg.buffer_size   = 256;
	gdg.ext_separator = (char)0xff;

	gdg.show_pc      = false;
	gdg.show_hex     = false;
	gdg.print_tabs   = true;
	gdg.decode_names = true;
	gdg.decode_registers = true;

	for (gdg.pc = 0; gdg.pc < DSP_IROM_SIZE;)
	{
		u16 CurrentPC = gdg.pc;

		if (g_dsp.pc & 0x8000)
			CurrentPC |= 0x8000;

		char Temp[256];
		sprintf(Temp, "0x%04x", CurrentPC);

		char Temp2[256];
		sprintf(Temp2, "0x%04x", dsp_imem_read(CurrentPC));

		char* pOpcode = gd_dis_opcode(&gdg);
		const char* pParameter = NULL;
		const char* pExtension = NULL;

		size_t WholeString = strlen(pOpcode);

		for (size_t i = 0; i < WholeString; i++)
		{
			if (pOpcode[i] == (char)0xff)
			{
				pOpcode[i] = 0x00;
				pExtension = &pOpcode[i + 1];
			}

			if (pOpcode[i] == 0x09)
			{
				pOpcode[i] = 0x00;
				pParameter = &pOpcode[i + 1];
			}
		}


		const char* pFunctionName = NULL;

		if (m_SymbolMap.find(CurrentPC) != m_SymbolMap.end())
		{
			pFunctionName = m_SymbolMap[CurrentPC].Name.c_str();
		}

		int Item = m_Disasm->InsertItem(gdg.pc, wxEmptyString);
		m_Disasm->SetItem(Item, COLUMN_BP, wxEmptyString);
		m_Disasm->SetItem(Item, COLUMN_FUNCTION, wxString::FromAscii(pFunctionName));
		m_Disasm->SetItem(Item, COLUMN_ADDRESS, wxString::FromAscii(Temp));
		m_Disasm->SetItem(Item, COLUMN_MNEMONIC, wxString::FromAscii(Temp2));
		m_Disasm->SetItem(Item, COLUMN_OPCODE, wxString::FromAscii(pOpcode));
		m_Disasm->SetItem(Item, COLUMN_EXT, wxString::FromAscii(pExtension));

		if (!strcasecmp(pOpcode, "CALL"))
		{
			u32 FunctionAddress = -1;
			sscanf(pParameter, "0x%04x", &FunctionAddress);

			if (m_SymbolMap.find(FunctionAddress) != m_SymbolMap.end())
			{
				pParameter = m_SymbolMap[FunctionAddress].Name.c_str();
			}
		}

		m_Disasm->SetItem(Item, COLUMN_PARAM, wxString::FromAscii(pParameter));

		m_Disasm->SetItemData(Item, CurrentPC);
	}

//	m_Disasm->SortItems(CompareFunc, this); // TODO verify

	m_Disasm->Thaw();
}

void DSPDebuggerLLE::UpdateDisAsmListView()
{
	if (g_dsp.dram == NULL)
		return;

	// check if we have to rebuild the list view
	if (m_Disasm->GetItemCount() == 0)
	{
		RebuildDisAsmListView();
	}
	else
	{
		u16 FirstPC = static_cast<u16>(m_Disasm->GetItemData(0)); // TODO verify

		if ((FirstPC & 0x8000) != (g_dsp.pc & 0x8000))
		{
			RebuildDisAsmListView();
		}
	}

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

	if (m_CachedUCodeCRC != g_dsp.iram_crc)
	{
		// load symbol map (if there is one)
		m_CachedUCodeCRC = g_dsp.iram_crc;
		char FileName[256];
		sprintf(FileName, "%sDSP_%08x.map", FULL_MAPS_DIR, m_CachedUCodeCRC);
		LoadSymbolMap(FileName);

		// rebuild the disasm
		RebuildDisAsmListView();
	}
}

void DSPDebuggerLLE::UpdateRegisterFlags()
{
	if (m_CachedCR == g_dsp.cr)
		return;

	m_Toolbar->ToggleTool(ID_CHECK_ASSERTINT, g_dsp.cr & 0x02 ? true : false);
	m_Toolbar->ToggleTool(ID_CHECK_HALT, g_dsp.cr & 0x04 ? true : false);
	m_Toolbar->ToggleTool(ID_CHECK_INIT, g_dsp.cr & 0x800 ? true : false);

	m_CachedCR = g_dsp.cr;
}

bool DSPDebuggerLLE::CanDoStep()
{
	// update the symbols all the time because they're script cmds like bps
	UpdateSymbolMap();

	switch (m_State)
	{
	case RUN_START:
		m_State = RUN;
		return true;

	case RUN:

		if (IsBreakPoint(g_dsp.pc))
		{
			Refresh();
			m_State = PAUSE;
			return false;
		}

		return true;

	case PAUSE:
		Refresh();
		return false;

	case STEP:
		Refresh();
		m_State = PAUSE;
		return true;
	}

	return false;
}

void DSPDebuggerLLE::DebugBreak()
{
	m_State = PAUSE;
}

bool DSPDebuggerLLE::IsBreakPoint(u16 _Address)
{
	return(std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address) != m_BreakPoints.end());
}

void DSPDebuggerLLE::ToggleBreakPoint(u16 _Address)
{
	if (IsBreakPoint(_Address))
		RemoveBreakPoint(_Address);
	else
		AddBreakPoint(_Address);
}

void DSPDebuggerLLE::RemoveBreakPoint(u16 _Address)
{
	CBreakPointList::iterator itr = std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address);

	if (itr != m_BreakPoints.end())
		m_BreakPoints.erase(itr);
}

void DSPDebuggerLLE::AddBreakPoint(u16 _Address)
{
	CBreakPointList::iterator itr = std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address);

	if (itr == m_BreakPoints.end())
		m_BreakPoints.push_back(_Address);
}

void DSPDebuggerLLE::ClearBreakPoints()
{
	m_BreakPoints.clear();
}

bool DSPDebuggerLLE::LoadSymbolMap(const char* _pFileName)
{
	m_SymbolMap.clear();

	FILE* pFile = fopen(_pFileName, "r");

	if (!pFile)
		return false;

	char Name[1024];
	u32 AddressStart, AddressEnd;

	while (!feof(pFile))
	{
		char line[512];
		fgets(line, 511, pFile);

		if (strlen(line) < 2)
			continue;

		// check for comment
		if (line[0] == '.')
			continue;

		// clear all breakpoints
		if (line[0] == 'C')
		{
			ClearBreakPoints();
			continue;
		}

		// add breakpoint
		if (line[0] == 'B')
		{
			sscanf(line, "B %04x", &AddressStart);
			AddBreakPoint(static_cast<u16>(AddressStart));
			continue;
		}

		// default add new symbol
		sscanf(line, "%04x %04x %s", &AddressStart, &AddressEnd, Name);

		if (m_SymbolMap.find(AddressStart) == m_SymbolMap.end())
			m_SymbolMap.insert(std::pair<u16, SSymbol>(AddressStart, SSymbol(AddressStart, AddressEnd, Name)));
		else
			m_SymbolMap[AddressStart] = SSymbol(AddressStart, AddressEnd, Name);
	}

	fclose(pFile);

	return true;
}
