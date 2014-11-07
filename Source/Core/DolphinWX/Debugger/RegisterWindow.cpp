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

#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/RegisterWindow.h"

class wxWindow;

BEGIN_EVENT_TABLE(CRegisterWindow, wxPanel)
EVT_GRID_CELL_RIGHT_CLICK(CRegisterView::OnMouseDownR)
EVT_MENU(-1, CRegisterView::OnPopupMenu)
END_EVENT_TABLE()


CRegisterWindow::CRegisterWindow(wxWindow* parent, wxWindowID id,
		const wxPoint& position, const wxSize& size,
		long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, m_GPRGridView(nullptr)
{
	CreateGUIControls();
}

void CRegisterWindow::CreateGUIControls()
{
	wxBoxSizer *sGrid = new wxBoxSizer(wxVERTICAL);
	m_GPRGridView = new CRegisterView(this, ID_GPR);
	sGrid->Add(m_GPRGridView, 1, wxGROW);
	SetSizer(sGrid);

	NotifyUpdate();
}

void CRegisterWindow::NotifyUpdate()
{
	if (m_GPRGridView != nullptr)
		m_GPRGridView->Update();
}
