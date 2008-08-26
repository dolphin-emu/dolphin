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

#include "Debugger.h"
#include "BreakpointWindow.h"
#include "BreakpointView.h"
#include "CodeWindow.h"
#include "HW/Memmap.h"
#include "BreakPointDlg.h"
#include "MemoryCheckDlg.h"
#include "IniFile.h"

#include <wx/mstream.h>

extern "C" {
#include "../resources/toolbar_add_breakpoint.c"
#include "../resources/toolbar_add_memorycheck.c"
#include "../resources/toolbar_delete.c"
}

static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

BEGIN_EVENT_TABLE(CBreakPointWindow, wxFrame)
	EVT_CLOSE(CBreakPointWindow::OnClose)
	EVT_MENU(IDM_DELETE, CBreakPointWindow::OnDelete)
	EVT_MENU(IDM_ADD_BREAKPOINT, CBreakPointWindow::OnAddBreakPoint)
	EVT_MENU(IDM_ADD_MEMORYCHECK, CBreakPointWindow::OnAddMemoryCheck)
    EVT_LIST_ITEM_ACTIVATED(ID_BPS, CBreakPointWindow::OnActivated)
END_EVENT_TABLE()


#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}


CBreakPointWindow::CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxFrame(parent, id, title, position, size, style)
	, m_BreakPointListView(NULL)
    , m_pCodeWindow(_pCodeWindow)
{
	InitBitmaps();

	CreateGUIControls();

	// Create the toolbar
	RecreateToolbar();

}


CBreakPointWindow::~CBreakPointWindow()
{}


void 
CBreakPointWindow::Save(IniFile& _IniFile) const
{
	_IniFile.Set("BreakPoint", "x", GetPosition().x);
	_IniFile.Set("BreakPoint", "y", GetPosition().y);
	_IniFile.Set("BreakPoint", "w", GetSize().GetWidth());
	_IniFile.Set("BreakPoint", "h", GetSize().GetHeight());
}


void 
CBreakPointWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("BreakPoint", "x", &x, GetPosition().x);
	_IniFile.Get("BreakPoint", "y", &y, GetPosition().y);
	_IniFile.Get("BreakPoint", "w", &w, GetSize().GetWidth());
	_IniFile.Get("BreakPoint", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}


void 
CBreakPointWindow::CreateGUIControls()
{
	SetTitle(wxT("Breakpoints"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 400, 370);
	Center();

	m_BreakPointListView = new CBreakPointView(this, ID_BPS, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	NotifyUpdate();
}


void
CBreakPointWindow::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[Toolbar_Delete].GetWidth(),
		h = m_Bitmaps[Toolbar_Delete].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	toolBar->AddTool(IDM_DELETE, _T("Delete"), m_Bitmaps[Toolbar_Delete], _T("Delete the selected BreakPoint or MemoryCheck"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_ADD_BREAKPOINT,    _T("BP"),    m_Bitmaps[Toolbar_Add_BreakPoint], _T("Add BreakPoint..."));

    // just add memory breakpoints if you can use them
    if (Memory::AreMemoryBreakpointsActivated())
    {
	    toolBar->AddTool(IDM_ADD_MEMORYCHECK, _T("MC"), m_Bitmaps[Toolbar_Add_Memcheck], _T("Add MemoryCheck..."));
    }

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}


void
CBreakPointWindow::RecreateToolbar()
{
	// delete and recreate the toolbar
	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	wxToolBar* theToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(theToolBar);
	SetToolBar(theToolBar);
}


void
CBreakPointWindow::InitBitmaps()
{
	// load orignal size 48x48
	m_Bitmaps[Toolbar_Delete] = wxGetBitmapFromMemory(toolbar_delete_png);
	m_Bitmaps[Toolbar_Add_BreakPoint] = wxGetBitmapFromMemory(toolbar_add_breakpoint_png);
	m_Bitmaps[Toolbar_Add_Memcheck] = wxGetBitmapFromMemory(toolbar_add_memcheck_png);

	// scale to 24x24 for toolbar
	for (size_t n = Toolbar_Delete; n < WXSIZEOF(m_Bitmaps); n++)
	{
		m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(16, 16));
	}
}


void 
CBreakPointWindow::OnClose(wxCloseEvent& /*event*/)
{
	Hide();
}


void CBreakPointWindow::NotifyUpdate()
{
	if (m_BreakPointListView != NULL)
	{
		m_BreakPointListView->Update();
	}
}


void 
CBreakPointWindow::OnDelete(wxCommandEvent& event)
{
	if (m_BreakPointListView)
	{
		m_BreakPointListView->DeleteCurrentSelection();
	}
}


void 
CBreakPointWindow::OnAddBreakPoint(wxCommandEvent& event)
{
	BreakPointDlg bpDlg(this);
	bpDlg.ShowModal();
}


void 
CBreakPointWindow::OnAddMemoryCheck(wxCommandEvent& event)
{
	MemoryCheckDlg memDlg(this);
	memDlg.ShowModal();
}


void
CBreakPointWindow::OnActivated(wxListEvent& event)
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

