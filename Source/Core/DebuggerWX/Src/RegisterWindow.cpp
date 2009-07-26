// Copyright (C) 2003-2009 Dolphin Project.

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
	, m_GPRGridView(NULL)
{
	CreateGUIControls();
}

CRegisterWindow::~CRegisterWindow()
{
}

void CRegisterWindow::Save(IniFile& _IniFile) const
{
	_IniFile.Set("RegisterWindow", "x", GetPosition().x);
	_IniFile.Set("RegisterWindow", "y", GetPosition().y);
	_IniFile.Set("RegisterWindow", "w", GetSize().GetWidth());
	_IniFile.Set("RegisterWindow", "h", GetSize().GetHeight());
}

void CRegisterWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("RegisterWindow", "x", &x, GetPosition().x);
	_IniFile.Get("RegisterWindow", "y", &y, GetPosition().y);
	_IniFile.Get("RegisterWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("RegisterWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
}

void CRegisterWindow::CreateGUIControls()
{
	SetTitle(wxT("Registers"));
	SetIcon(wxNullIcon);
	Center();

	wxBoxSizer *sGrid = new wxBoxSizer(wxVERTICAL);
	m_GPRGridView = new CRegisterView(this, ID_GPR);
	sGrid->Add(m_GPRGridView, 1, wxGROW);
	SetSizer(sGrid);
	sGrid->SetSizeHints(this);

	NotifyUpdate();
}

void CRegisterWindow::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Hide();
}

void CRegisterWindow::NotifyUpdate()
{	
	if (m_GPRGridView != NULL)
	{
		m_GPRGridView->Update();
	}
}
