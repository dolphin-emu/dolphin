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

#include "Debugger.h"
#include "BreakpointView.h"
#include "CodeWindow.h"
#include "HW/Memmap.h"
#include "BreakpointDlg.h"
#include "MemoryCheckDlg.h"
#include "Host.h"
#include "PowerPC/PowerPC.h"


BEGIN_EVENT_TABLE(CBreakPointWindow, wxPanel)
	EVT_CLOSE(CBreakPointWindow::OnClose)
	EVT_LIST_ITEM_ACTIVATED(ID_BPS, CBreakPointWindow::OnActivated)
	EVT_LIST_ITEM_SELECTED(ID_TOOLBAR, CBreakPointWindow::OnSelectItem)
END_EVENT_TABLE()

CBreakPointWindow::CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
: wxPanel(parent, id, position, size, style, title)
	, m_BreakPointListView(NULL)
    , m_pCodeWindow(_pCodeWindow)
{
	CreateGUIControls();
}

void CBreakPointWindow::CreateGUIControls()
{
	SetSize(8, 8, 400, 370);
	Center();

	m_BreakPointBar = new CBreakPointBar(this, ID_TOOLBAR, wxDefaultPosition, wxSize(0, 55),
			wxLC_ICON | wxSUNKEN_BORDER | wxLC_SINGLE_SEL);
	m_BreakPointListView = new CBreakPointView(this, ID_BPS, wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	wxBoxSizer* sizerH = new wxBoxSizer(wxVERTICAL);
	sizerH->Add(m_BreakPointBar, 0, wxALL | wxEXPAND);
	sizerH->Add((wxListCtrl*)m_BreakPointListView, 1, wxEXPAND);

	NotifyUpdate();
	SetSizer(sizerH);
}

void CBreakPointWindow::OnSelectItem(wxListEvent& event)
{
	switch(event.GetItem().GetId())
	{
	case IDM_DELETE:
		OnDelete();
		m_BreakPointBar->SetItemState(event.GetItem().GetId(), 0, wxLIST_STATE_FOCUSED);
		break;
	case IDM_CLEAR:
		OnClear();
		m_BreakPointBar->SetItemState(event.GetItem().GetId(), 0, wxLIST_STATE_FOCUSED);
		break;
	case IDM_ADD_BREAKPOINT:
		OnAddBreakPoint();
		break;
	case IDM_ADD_BREAKPOINTMANY:
		OnAddBreakPointMany();
		break;
	case IDM_ADD_MEMORYCHECK:
		OnAddMemoryCheck();
		break;
	case IDM_ADD_MEMORYCHECKMANY:
		OnAddMemoryCheckMany();
		break;
	}
}

void CBreakPointWindow::OnClose(wxCloseEvent& /*event*/)
{
	Hide();
}

void CBreakPointWindow::NotifyUpdate()
{
	if (m_BreakPointListView != NULL) m_BreakPointListView->Update();
}

void CBreakPointWindow::OnDelete()
{
	if (m_BreakPointListView)
	{
		m_BreakPointListView->DeleteCurrentSelection();
	}
}

void CBreakPointWindow::OnActivated(wxListEvent& event)
{
    long Index = event.GetIndex();
    if (Index >= 0)
    {
        u32 Address = (u32)m_BreakPointListView->GetItemData(Index);
        if (m_pCodeWindow != NULL)
        {
            m_pCodeWindow->JumpToAddress(Address);
        }
    }
}


// Breakpoint actions
// ---------------------

// Clear all breakpoints
void CBreakPointWindow::OnClear()
{
	PowerPC::breakpoints.Clear();
	PowerPC::memchecks.Clear();
	NotifyUpdate();
}
// Add one breakpoint
void CBreakPointWindow::OnAddBreakPoint()
{
	BreakPointDlg bpDlg(this, this);
	bpDlg.ShowModal();
}
// Load breakpoints from file
void CBreakPointWindow::OnAddBreakPointMany()
{
	// load ini
	IniFile ini;
	std::string filename = std::string(FULL_GAMECONFIG_DIR "BreakPoints.ini");

	if (ini.Load(filename.c_str())) // check if there is any file there
	{
		// get lines from a certain section
		std::vector<std::string> lines;
		if (!ini.GetLines("BreakPoints", lines))
		{
			wxMessageBox(_T("You have no [BreakPoints] line in your file"));
			return;
		}

		for (std::vector<std::string>::const_iterator iter = lines.begin(); iter != lines.end(); ++iter)
		{
			std::string line = StripSpaces(*iter);
			u32 Address = 0;
			if (AsciiToHex(line.c_str(), Address))
			{
				PowerPC::breakpoints.Add(Address);
			}
		}
		// Only update after we are done with the loop
		NotifyUpdate();
	}
	else
	{
		wxMessageBox(_T("Couldn't find User/GameConfig/BreakPoints.ini file"));
	}

}


// Memory check actions
// ---------------------
void 
CBreakPointWindow::OnAddMemoryCheck()
{
	MemoryCheckDlg memDlg(this);
	memDlg.ShowModal();
}

// Load memory checks from file
void CBreakPointWindow::OnAddMemoryCheckMany()
{
	// load ini
	IniFile ini;
	std::string filename = std::string(FULL_GAMECONFIG_DIR "MemoryChecks.ini");

	if (ini.Load(filename.c_str()))
	{
		// get lines from a certain section
		std::vector<std::string> lines;
		if (!ini.GetLines("MemoryChecks", lines))
		{
			wxMessageBox(_T("You have no [MemoryChecks] line in your file"));
			return;
		}

		for (std::vector<std::string>::const_iterator iter = lines.begin(); iter != lines.end(); ++iter)
		{
			std::string line = StripSpaces(*iter);
			std::vector<std::string> pieces;
			SplitString(line, " ", pieces); // split string

			TMemCheck MemCheck;
			u32 sAddress = 0;
			u32 eAddress = 0;
			bool doCommon = false;

			// ------------------------------------------------------------------------------------------
			// Decide if we have a range or just one address, and if we should break or not
			// --------------
			if (
				pieces.size() == 1
				&& AsciiToHex(pieces[0].c_str(), sAddress)
				&& pieces[0].size() == 8
				)
			{
				// address range	
				MemCheck.StartAddress = sAddress;
				MemCheck.EndAddress = sAddress;
				doCommon = true;
				MemCheck.Break = false;
			}
			else if(
				pieces.size() == 2
				&& AsciiToHex(pieces[0].c_str(), sAddress) && AsciiToHex(pieces[1].c_str(), eAddress)
				&& pieces[0].size() == 8 && pieces[1].size() == 8
				)
			{
				// address range	
				MemCheck.StartAddress = sAddress;
				MemCheck.EndAddress = eAddress;
				doCommon = true;
				MemCheck.Break = false;
			}
			else if(
				pieces.size() == 3
				&& AsciiToHex(pieces[0].c_str(), sAddress) && AsciiToHex(pieces[1].c_str(), eAddress)
				&& pieces[0].size() == 8 && pieces[1].size() == 8 && pieces[2].size() == 1
				)
			{
				// address range	
				MemCheck.StartAddress = sAddress;
				MemCheck.EndAddress = eAddress;
				doCommon = true;
				MemCheck.Break = true;
			}

			if (doCommon)
			{
				// settings for the memory check	
				MemCheck.OnRead = true;
				MemCheck.OnWrite = true;
				MemCheck.Log = true;
				//MemCheck.Break = false; // this is also what sets Active "on" in the breakpoint window
				// so don't think it's off because we are only writing this to the log
				PowerPC::memchecks.Add(MemCheck);	
			}
		}
		// Update after we are done with the loop
		NotifyUpdate();
	}
	else
	{
		wxMessageBox(_T("You have no ") T_FULL_GAMECONFIG_DIR _T("MemoryChecks.ini file"));
	}
}

