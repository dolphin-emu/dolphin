// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>

#include <wx/bitmap.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/windowid.h>
#include <wx/aui/auibar.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"

extern "C" {
#include "DolphinWX/resources/toolbar_debugger_delete.c"
}

class wxWindow;

BEGIN_EVENT_TABLE(CWatchWindow, wxPanel)
EVT_GRID_CELL_RIGHT_CLICK(CWatchView::OnMouseDownR)
EVT_MENU(-1, CWatchView::OnPopupMenu)
END_EVENT_TABLE()

class CWatchToolbar : public wxAuiToolBar
{
public:
CWatchToolbar(CWatchWindow* parent, const wxWindowID id)
	: wxAuiToolBar(parent, id, wxDefaultPosition, wxDefaultSize,
			wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT)
{
	SetToolBitmapSize(wxSize(16, 16));

	m_Bitmaps[Toolbar_File] = wxBitmap(wxGetBitmapFromMemory(toolbar_delete_png).ConvertToImage().Rescale(16, 16));

	AddTool(ID_LOAD, _("Load"), m_Bitmaps[Toolbar_File]);
	Bind(wxEVT_TOOL, &CWatchWindow::LoadAll, parent, ID_LOAD);

	AddTool(ID_SAVE, _("Save"), m_Bitmaps[Toolbar_File]);
	Bind(wxEVT_TOOL, &CWatchWindow::Event_SaveAll, parent, ID_SAVE);
}

private:

	enum
	{
		Toolbar_File,
		Num_Bitmaps
	};

	enum
	{
		ID_LOAD,
		ID_SAVE
	};

	wxBitmap m_Bitmaps[Num_Bitmaps];
};

CWatchWindow::CWatchWindow(wxWindow* parent, wxWindowID id,
		const wxPoint& position, const wxSize& size,
		long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, m_GPRGridView(nullptr)
{
	m_mgr.SetManagedWindow(this);
	m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

	wxBoxSizer *sGrid = new wxBoxSizer(wxVERTICAL);
	m_GPRGridView = new CWatchView(this, ID_GPR);
	sGrid->Add(m_GPRGridView, 1, wxGROW);
	SetSizer(sGrid);

	m_mgr.AddPane(new CWatchToolbar(this, wxID_ANY), wxAuiPaneInfo().ToolbarPane().Top().
		LeftDockable(true).RightDockable(true).BottomDockable(false).Floatable(false));
	m_mgr.AddPane(m_GPRGridView, wxAuiPaneInfo().CenterPane());
	m_mgr.Update();
}

void CWatchWindow::NotifyUpdate()
{
	if (m_GPRGridView != nullptr)
		m_GPRGridView->Update();
}

void CWatchWindow::Event_SaveAll(wxCommandEvent& WXUNUSED(event))
{
	SaveAll();
}

void CWatchWindow::SaveAll()
{
	IniFile ini;
	if (ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX)))
	{
		ini.SetLines("Watches", PowerPC::watches.GetStrings());
		ini.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
	}
}

void CWatchWindow::LoadAll(wxCommandEvent& WXUNUSED(event))
{
	IniFile ini;
	Watches::TWatchesStr watches;

	if (!ini.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX)))
	{
		return;
	}

	if (ini.GetLines("Watches", &watches, false))
	{
		PowerPC::watches.AddFromStrings(watches);
	}

	NotifyUpdate();
}
