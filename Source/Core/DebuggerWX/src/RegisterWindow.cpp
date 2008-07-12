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
#include "RegisterWindow.h"
#include "PowerPC/PowerPC.h"
#include "RegisterView.h"

extern const char* GetGRPName(unsigned int index);


BEGIN_EVENT_TABLE(CRegisterWindow, wxDialog)
EVT_CLOSE(CRegisterWindow::OnClose)
END_EVENT_TABLE()

CRegisterWindow::CRegisterWindow(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	, m_GPRListView(NULL)
{
	CreateGUIControls();
}


CRegisterWindow::~CRegisterWindow()
{}


void CRegisterWindow::CreateGUIControls()
{
	SetTitle(wxT("Registers"));
	SetIcon(wxNullIcon);
	SetSize(8, 8, 400, 370);
	Center();

	m_GPRListView = new CRegisterView(this, ID_GPR, wxDefaultPosition, GetSize(),
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT | wxLC_SINGLE_SEL | wxLC_SORT_ASCENDING);

	NotifyUpdate();
}


void CRegisterWindow::OnClose(wxCloseEvent& /*event*/)
{
	Hide();
}


void CRegisterWindow::NotifyUpdate()
{
	if (m_GPRListView != NULL)
	{
		m_GPRListView->Update();
	}
}


