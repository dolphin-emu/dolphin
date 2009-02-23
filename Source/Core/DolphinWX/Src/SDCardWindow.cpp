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

#include "SDCardWindow.h"
#include "Globals.h"
#include "IPC_HLE/HW/SDInterface.h"

BEGIN_EVENT_TABLE(wxSDCardWindow, wxWindow)
	EVT_CLOSE(                                           wxSDCardWindow::OnEvent_Window_Close)
    EVT_BUTTON(ID_BUTTON_CLOSE,                          wxSDCardWindow::OnEvent_ButtonClose_Press)
END_EVENT_TABLE()

wxSDCardWindow::wxSDCardWindow(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, _T("SDCard Mounter"), wxDefaultPosition, wxSize(400, 400), wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
	Init_ChildControls();
	Layout();
	Show();
}

wxSDCardWindow::~wxSDCardWindow()
{
	// On Disposal
}

void wxSDCardWindow::Init_ChildControls()
{
	// Button Strip
	m_Button_Close = new wxButton(this, ID_BUTTON_CLOSE, _T("Close"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_Button_Close, 0, wxALL, 5);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sButtons, 0, wxALL, 5);
	SetSizer(sMain);
	Layout();

	Fit();
}

void wxSDCardWindow::OnEvent_Window_Close(wxCloseEvent& WXUNUSED(event))
{
	EndModal(0);
}

void wxSDCardWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED(event))
{
	EndModal(0);
}