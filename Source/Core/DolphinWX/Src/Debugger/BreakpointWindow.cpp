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

extern "C" {
#include "../../resources/toolbar_add_breakpoint.c"
#include "../../resources/toolbar_add_memorycheck.c"
#include "../../resources/toolbar_debugger_delete.c"
}

#include <map>
typedef void (CBreakPointWindow::*toolbar_func)();
typedef std::map<long, toolbar_func> toolbar_m;
typedef std::pair<long, toolbar_func> toolbar_p;
toolbar_m toolbar_map;

class CBreakPointBar : public wxListCtrl
{
	enum
	{
		Toolbar_Delete,
		Toolbar_Add_BP,
		Toolbar_Add_MC,
		Bitmaps_max
	};
	wxBitmap m_Bitmaps[Bitmaps_max];

public:
	CBreakPointBar::CBreakPointBar(CBreakPointWindow* parent, const wxWindowID id,
		const wxPoint& pos, const wxSize& size, long style)
		: wxListCtrl((wxWindow*)parent, id, pos, size, style)
	{
		SetBackgroundColour(wxColour(0x555555));
		SetForegroundColour(wxColour(0xffffff));

		// load original size 48x48
		wxMemoryInputStream st1(toolbar_delete_png, sizeof(toolbar_delete_png));
		wxMemoryInputStream st2(toolbar_add_breakpoint_png, sizeof(toolbar_add_breakpoint_png));
		wxMemoryInputStream st3(toolbar_add_memcheck_png, sizeof(toolbar_add_memcheck_png));
		m_Bitmaps[Toolbar_Delete] = wxBitmap(wxImage(st1, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);
		m_Bitmaps[Toolbar_Add_BP] = wxBitmap(wxImage(st2, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);
		m_Bitmaps[Toolbar_Add_MC] = wxBitmap(wxImage(st3, wxBITMAP_TYPE_ANY, -1).Rescale(24,24), -1);

		m_imageListNormal = new wxImageList(24, 24);
		m_imageListNormal->Add(m_Bitmaps[Toolbar_Delete]);
		m_imageListNormal->Add(m_Bitmaps[Toolbar_Add_BP]);
		m_imageListNormal->Add(m_Bitmaps[Toolbar_Add_MC]);
		SetImageList(m_imageListNormal, wxIMAGE_LIST_NORMAL);

		toolbar_map.insert(toolbar_p(InsertItem(0, _("Delete"), 0), &CBreakPointWindow::OnDelete));
		toolbar_map.insert(toolbar_p(InsertItem(1, _("Clear"), 0), &CBreakPointWindow::OnClear));
		toolbar_map.insert(toolbar_p(InsertItem(2, _("+BP"), 1), &CBreakPointWindow::OnAddBreakPoint));

		// just add memory breakpoints if you can use them
		if (Memory::AreMemoryBreakpointsActivated())
		{
			toolbar_map.insert(toolbar_p(InsertItem(3, _("+MC"), 2), &CBreakPointWindow::OnAddMemoryCheck));
			toolbar_map.insert(toolbar_p(InsertItem(4, _("Load"), 0), &CBreakPointWindow::LoadAll));
			toolbar_map.insert(toolbar_p(InsertItem(5, _("Save"), 0), &CBreakPointWindow::SaveAll));
		}
		else
		{
			toolbar_map.insert(toolbar_p(InsertItem(3, _("Load"), 0), &CBreakPointWindow::LoadAll));
			toolbar_map.insert(toolbar_p(InsertItem(4, _("Save"), 0), &CBreakPointWindow::SaveAll));
		}
	}
};

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
	(this->*toolbar_map[event.GetItem().GetId()])();
	m_BreakPointBar->SetItemState(event.GetItem().GetId(), 0, wxLIST_STATE_SELECTED);
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
	
	if (ini.GetLines("BreakPoints", newbps, false))
		PowerPC::breakpoints.AddFromStrings(newbps);
	if (ini.GetLines("MemoryChecks", newmcs, false))
		PowerPC::memchecks.AddFromStrings(newmcs);

	NotifyUpdate();
}
