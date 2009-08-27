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
#include "BreakpointWindow.h"
#include "BreakpointView.h"
#include "CodeWindow.h"
#include "HW/Memmap.h"
#include "BreakPointDlg.h"
#include "MemoryCheckDlg.h"
#include "Host.h"
#include "PowerPC/PowerPC.h"

#include <wx/mstream.h>

extern "C" {
#include "../resources/toolbar_add_breakpoint.c"
#include "../resources/toolbar_add_memorycheck.c"
#include "../resources/toolbar_delete.c"
}

BEGIN_EVENT_TABLE(CBreakPointWindow, wxFrame)
	EVT_CLOSE(CBreakPointWindow::OnClose)
	EVT_MENU(IDM_DELETE, CBreakPointWindow::OnDelete)
	EVT_MENU(IDM_CLEAR, CBreakPointWindow::OnClear)
	EVT_MENU(IDM_ADD_BREAKPOINT, CBreakPointWindow::OnAddBreakPoint)
	EVT_MENU(IDM_ADD_BREAKPOINTMANY, CBreakPointWindow::OnAddBreakPointMany)	
	EVT_MENU(IDM_ADD_MEMORYCHECK, CBreakPointWindow::OnAddMemoryCheck)
	EVT_MENU(IDM_ADD_MEMORYCHECKMANY, CBreakPointWindow::OnAddMemoryCheckMany)	
    EVT_LIST_ITEM_ACTIVATED(ID_BPS, CBreakPointWindow::OnActivated)
END_EVENT_TABLE()

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

void CBreakPointWindow::Save(IniFile& _IniFile) const
{
	_IniFile.Set("BreakPoint", "x", GetPosition().x);
	_IniFile.Set("BreakPoint", "y", GetPosition().y);
	_IniFile.Set("BreakPoint", "w", GetSize().GetWidth());
	_IniFile.Set("BreakPoint", "h", GetSize().GetHeight());
}
void CBreakPointWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("BreakPoint", "x", &x, GetPosition().x);
	_IniFile.Get("BreakPoint", "y", &y, GetPosition().y);
	_IniFile.Get("BreakPoint", "w", &w, GetSize().GetWidth());
	_IniFile.Get("BreakPoint", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}

void CBreakPointWindow::CreateGUIControls()
{
	SetTitle(wxT("Breakpoints"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 400, 370);
	Center();

	m_BreakPointListView = new CBreakPointView(this, ID_BPS, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	NotifyUpdate();
}

void CBreakPointWindow::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[Toolbar_Delete].GetWidth(),
		h = m_Bitmaps[Toolbar_Delete].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	toolBar->AddTool(IDM_DELETE, _T("Delete"), m_Bitmaps[Toolbar_Delete], _T("Delete the selected BreakPoint or MemoryCheck"));
	toolBar->AddTool(IDM_CLEAR, _T("Clear all"), m_Bitmaps[Toolbar_Delete], _T("Clear all BreakPoints and MemoryChecks"));
	
	toolBar->AddSeparator();

	toolBar->AddTool(IDM_ADD_BREAKPOINT,    _T("BP"),    m_Bitmaps[Toolbar_Add_BreakPoint], _T("Add BreakPoint..."));
	toolBar->AddTool(IDM_ADD_BREAKPOINTMANY,    _T("BPs"),    m_Bitmaps[Toolbar_Add_BreakPoint], _T("Add BreakPoints..."));

    // just add memory breakpoints if you can use them
    if (Memory::AreMemoryBreakpointsActivated())
    {
	    toolBar->AddTool(IDM_ADD_MEMORYCHECK, _T("MC"), m_Bitmaps[Toolbar_Add_Memcheck], _T("Add MemoryCheck..."));
		toolBar->AddTool(IDM_ADD_MEMORYCHECKMANY, _T("MCs"), m_Bitmaps[Toolbar_Add_Memcheck], _T("Add MemoryChecks..."));
    }

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}

void CBreakPointWindow::RecreateToolbar()
{
	// delete and recreate the toolbar
	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	wxToolBar* theToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(theToolBar);
	SetToolBar(theToolBar);
}

void CBreakPointWindow::InitBitmaps()
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

void CBreakPointWindow::OnClose(wxCloseEvent& /*event*/)
{
	Hide();
}

void CBreakPointWindow::NotifyUpdate()
{
	if (m_BreakPointListView != NULL) m_BreakPointListView->Update();
}

void CBreakPointWindow::OnDelete(wxCommandEvent& event)
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Breakpoint actions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Clear all breakpoints
void CBreakPointWindow::OnClear(wxCommandEvent& event)
{
	PowerPC::breakpoints.Clear();
	NotifyUpdate();
}
// Add one breakpoint
void CBreakPointWindow::OnAddBreakPoint(wxCommandEvent& event)
{
	BreakPointDlg bpDlg(this, this);
	bpDlg.ShowModal();
}
// Load breakpoints from file
void CBreakPointWindow::OnAddBreakPointMany(wxCommandEvent& event)
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
		wxMessageBox(_T("You have no GameIni/BreakPoints.ini file"));
	}

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory check actions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void 
CBreakPointWindow::OnAddMemoryCheck(wxCommandEvent& event)
{
	MemoryCheckDlg memDlg(this);
	memDlg.ShowModal();
}

// Load memory checks from file
void CBreakPointWindow::OnAddMemoryCheckMany(wxCommandEvent& event)
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
/////////////////////////////////////////////////////////////////////////////////////////////////////////
