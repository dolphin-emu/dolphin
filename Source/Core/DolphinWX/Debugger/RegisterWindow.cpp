// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/wx.h>

#include "RegisterWindow.h"
#include "PowerPC/PowerPC.h"
#include "RegisterView.h"

extern const char* GetGRPName(unsigned int index);

BEGIN_EVENT_TABLE(CRegisterWindow, wxPanel)
END_EVENT_TABLE()


CRegisterWindow::CRegisterWindow(wxWindow* parent, wxWindowID id,
		const wxPoint& position, const wxSize& size,
		long style, const wxString& name)
	: wxPanel(parent, id, position, size, style, name)
	, m_GPRGridView(NULL)
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
	if (m_GPRGridView != NULL)
		m_GPRGridView->Update();
}
