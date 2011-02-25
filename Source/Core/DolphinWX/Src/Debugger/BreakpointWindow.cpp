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

#include <wx/wx.h>

#include "BreakpointView.h"
#include "CodeWindow.h"
#include "HW/Memmap.h"
#include "BreakpointDlg.h"
#include "MemoryCheckDlg.h"
#include "Host.h"
#include "PowerPC/PowerPC.h"
#include "FileUtil.h"

BEGIN_EVENT_TABLE(CBreakPointWindow, wxPanel)
	EVT_CLOSE(CBreakPointWindow::OnClose)
	EVT_LIST_ITEM_SELECTED(ID_BPS, CBreakPointWindow::OnSelectBP)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_BPS, CBreakPointWindow::OnRightClick)
	EVT_LIST_ITEM_SELECTED(ID_TOOLBAR, CBreakPointWindow::OnSelectToolbar)
END_EVENT_TABLE()

CBreakPointWindow::CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent,
	   	wxWindowID id, const wxString& title, const wxPoint& position,
	   	const wxSize& size, long style)
: wxPanel(parent, id, position, size, style, title)
	, m_BreakPointListView(NULL)
	, m_pCodeWindow(_pCodeWindow)
{
	CreateGUIControls();
}

void CBreakPointWindow::OnClose(wxCloseEvent& WXUNUSED(event))
{
	SaveAll();
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
	sizerH->Add(m_BreakPointListView, 1, wxEXPAND);

	SetSizer(sizerH);
}

void CBreakPointWindow::OnSelectToolbar(wxListEvent& event)
{
	switch(event.GetItem().GetId())
	{
	case IDM_DELETE:
		OnDelete();
		break;
	case IDM_CLEAR:
		OnClear();
		break;
	case IDM_ADD_BREAKPOINT:
		OnAddBreakPoint();
		break;
	case IDM_ADD_MEMORYCHECK:
		OnAddMemoryCheck();
		break;
	case IDM_LOADALL:
		LoadAll();
	case IDM_SAVEALL:
		SaveAll();
		break;
	}
}

void CBreakPointWindow::NotifyUpdate()
{
	if (m_BreakPointListView)
		m_BreakPointListView->Update();
}

void CBreakPointWindow::OnDelete()
{
	if (m_BreakPointListView)
		m_BreakPointListView->DeleteCurrentSelection();
}

// jump to begin addr
void CBreakPointWindow::OnSelectBP(wxListEvent& event)
{
	long Index = event.GetIndex();
	if (Index >= 0)
	{
		u32 Address = (u32)m_BreakPointListView->GetItemData(Index);
		if (m_pCodeWindow)
			m_pCodeWindow->JumpToAddress(Address);
	}
}

// modify
void CBreakPointWindow::OnRightClick(wxListEvent& event)
{
}

// Clear all breakpoints and memchecks
void CBreakPointWindow::OnClear()
{
	PowerPC::breakpoints.Clear();
	PowerPC::memchecks.Clear();
	NotifyUpdate();
}

void CBreakPointWindow::OnAddBreakPoint()
{
	BreakPointDlg bpDlg(this, this);
	bpDlg.ShowModal();
}

void CBreakPointWindow::OnAddMemoryCheck()
{
	MemoryCheckDlg memDlg(this);
	memDlg.ShowModal();
}

void CBreakPointWindow::SaveAll()
{
	// simply dump all to bp/mc files in a way we can read again
	IniFile ini;
	if (ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX)))
	{
		ini.SetLines("BreakPoints", PowerPC::breakpoints.GetStrings());
		ini.SetLines("MemoryChecks", PowerPC::memchecks.GetStrings());
		ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
	}
}

void CBreakPointWindow::LoadAll()
{
	IniFile ini;
	BreakPoints::TBreakPointsStr newbps;
	MemChecks::TMemChecksStr newmcs;
	
	if (!ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX)))
		return;
	
	if (ini.GetLines("BreakPoints", newbps))
		PowerPC::breakpoints.AddFromStrings(newbps);
	if (ini.GetLines("MemoryChecks", newmcs, false))
		PowerPC::memchecks.AddFromStrings(newmcs);

	NotifyUpdate();
}
