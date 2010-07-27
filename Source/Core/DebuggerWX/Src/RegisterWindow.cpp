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
	sGrid->SetSizeHints(this);

	NotifyUpdate();
}

void CRegisterWindow::NotifyUpdate()
{
	if (m_GPRGridView != NULL)
	{
		m_GPRGridView->Update();
	}
}
