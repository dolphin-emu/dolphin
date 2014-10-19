// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"

class wxWindow;

BEGIN_EVENT_TABLE(CWatchWindow, wxPanel)
END_EVENT_TABLE()


CWatchWindow::CWatchWindow(wxWindow* parent, wxWindowID id,
		const wxPoint& position, const wxSize& size,
		long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, m_GPRGridView(nullptr)
{
	CreateGUIControls();
}

void CWatchWindow::CreateGUIControls()
{
	wxBoxSizer *sGrid = new wxBoxSizer(wxVERTICAL);
	m_GPRGridView = new CWatchView(this, ID_GPR);
	sGrid->Add(m_GPRGridView, 1, wxGROW);
	SetSizer(sGrid);

	NotifyUpdate();
}

void CWatchWindow::NotifyUpdate()
{
	if (m_GPRGridView != nullptr)
		m_GPRGridView->Update();
}
