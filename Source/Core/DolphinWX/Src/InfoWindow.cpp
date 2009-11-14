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

#include "svnrev.h"
#include "Globals.h"
#include "InfoWindow.h"
#include "CPUDetect.h"
#include "Core.h"
#include "ConfigManager.h"
#include "CDUtils.h"
#include "Summarize.h"//This holds all the summarize functions

BEGIN_EVENT_TABLE(wxInfoWindow, wxWindow)
	EVT_SIZE(                                            wxInfoWindow::OnEvent_Window_Resize)
	EVT_CLOSE(                                           wxInfoWindow::OnEvent_Window_Close)
    EVT_BUTTON(ID_BUTTON_CLOSE,                          wxInfoWindow::OnEvent_ButtonClose_Press)
END_EVENT_TABLE()

wxInfoWindow::wxInfoWindow(wxFrame* parent, const wxPoint& pos, const wxSize& size) :
	wxFrame(parent, wxID_ANY, _T("System Information"), pos, size, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
	// Create the GUI controls
	Init_ChildControls();

	// Setup Window
	SetBackgroundColour(wxColour(COLOR_GRAY));
	SetSize(size);
	SetPosition(pos);
	Layout();
	Show();
}

wxInfoWindow::~wxInfoWindow()
{
	// On Disposal
}

void wxInfoWindow::Init_ChildControls()
{
	std::string Info;
	Info = StringFromFormat("Dolphin Revision: %s", SVN_REV_STR);



	Info.append(StringFromFormat(
		"\n\n%s\n%s\n%s\n\n%s\n\n",
		Summarize_Drives().c_str(),
		Summarize_CPU().c_str(),
		Summarize_Plug().c_str(),
		Summarize_Settings().c_str()
		));
	
	// Main Notebook
	m_Notebook_Main = new wxNotebook(this, ID_NOTEBOOK_MAIN, wxDefaultPosition, wxDefaultSize);
		// --- Tabs ---

		// $ Log Tab
		m_Tab_Log = new wxPanel(m_Notebook_Main, ID_TAB_LOG, wxDefaultPosition, wxDefaultSize);
		m_TextCtrl_Log = new wxTextCtrl(m_Tab_Log, ID_TEXTCTRL_LOG, wxString::FromAscii(Info.c_str()), wxDefaultPosition, wxSize(100, 600),
										wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
		wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
		wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
		sTabLog->Add(HStrip1, 0, wxALL, 5);
		sTabLog->Add(m_TextCtrl_Log, 1, wxALL|wxEXPAND, 5);

		m_Tab_Log->SetSizer(sTabLog);
		m_Tab_Log->Layout();

	// Add Tabs to Notebook
	m_Notebook_Main->AddPage(m_Tab_Log, _T("System Information"));

	// Button Strip
	m_Button_Close = new wxButton(this, ID_BUTTON_CLOSE, _T("Close"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(m_Button_Close, 0, wxALL, 5);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook_Main, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxALL, 5);
	SetSizer(sMain);
	Layout();

	Fit();
}



void wxInfoWindow::OnEvent_Window_Resize(wxSizeEvent& WXUNUSED (event))
{
	Layout();
}
void wxInfoWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED (event))
{
	Destroy();
}
void wxInfoWindow::OnEvent_Window_Close(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

