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

#include "Globals.h"
#include "CheatsWindow.h"
#include "ActionReplay.h"
#include "Core.h"

using namespace ActionReplay;

BEGIN_EVENT_TABLE(wxCheatsWindow, wxWindow)
	EVT_SIZE(                                            wxCheatsWindow::OnEvent_Window_Resize)
	EVT_CLOSE(                                           wxCheatsWindow::OnEvent_Window_Close)
    EVT_BUTTON(ID_BUTTON_CLOSE,                          wxCheatsWindow::OnEvent_ButtonClose_Press)
	EVT_LISTBOX(ID_CHECKLISTBOX_CHEATSLIST,              wxCheatsWindow::OnEvent_CheatsList_ItemSelected)
	EVT_CHECKLISTBOX(ID_CHECKLISTBOX_CHEATSLIST,         wxCheatsWindow::OnEvent_CheatsList_ItemToggled)
	EVT_BUTTON(ID_BUTTON_APPLYCODES,                     wxCheatsWindow::OnEvent_ButtonUpdateCodes_Press)
	EVT_BUTTON(ID_BUTTON_UPDATELOG,                      wxCheatsWindow::OnEvent_ButtonUpdateLog_Press)
	EVT_CHECKBOX(ID_CHECKBOX_LOGAR,                      wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange)
END_EVENT_TABLE()

wxCheatsWindow::wxCheatsWindow(wxFrame* parent, const wxPoint& pos, const wxSize& size) :
	wxFrame(parent, wxID_ANY, _T("Action Replay"), pos, size, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
	// Create the GUI controls
	Init_ChildControls();

	// Setup Window
	SetBackgroundColour(wxColour(COLOR_GRAY));
	SetSize(size);
	SetPosition(pos);

	// Load Data
	Load_ARCodes();

	Layout();
	Show();
}

wxCheatsWindow::~wxCheatsWindow()
{
	// On Disposal
}

void wxCheatsWindow::Init_ChildControls()
{
	// Main Notebook
	m_Notebook_Main = new wxNotebook(this, ID_NOTEBOOK_MAIN, wxDefaultPosition, wxDefaultSize);
		// --- Tabs ---
		// $ Cheats List Tab
		m_Tab_Cheats = new wxPanel(m_Notebook_Main, ID_TAB_CHEATS, wxDefaultPosition, wxDefaultSize);
		m_CheckListBox_CheatsList = new wxCheckListBox(m_Tab_Cheats, ID_CHECKLISTBOX_CHEATSLIST, wxDefaultPosition, wxSize(300, 0), m_CheatStringList, wxLB_HSCROLL, wxDefaultValidator);
		m_Label_Codename = new wxStaticText(m_Tab_Cheats, ID_LABEL_CODENAME, _T("Name: "), wxDefaultPosition, wxDefaultSize);
		m_GroupBox_Info = new wxStaticBox(m_Tab_Cheats, ID_GROUPBOX_INFO, _T("Code Info"), wxDefaultPosition, wxDefaultSize);
		m_Button_ApplyCodes = new wxButton(m_Tab_Cheats, ID_BUTTON_APPLYCODES, _T("Apply Changes"), wxDefaultPosition, wxDefaultSize);
		m_Label_NumCodes = new wxStaticText(m_Tab_Cheats, ID_LABEL_NUMCODES, _T("Number Of Codes: "),  wxDefaultPosition, wxDefaultSize);
		m_ListBox_CodesList = new wxListBox(m_Tab_Cheats, ID_LISTBOX_CODESLIST, wxDefaultPosition, wxSize(120, 150), 0, 0, wxLB_HSCROLL);

		wxBoxSizer* sCheatsListH = new wxBoxSizer(wxHORIZONTAL);
		sCheatsListH->Add(m_CheckListBox_CheatsList, 1, wxALL|wxEXPAND, 5);

		wxStaticBoxSizer* sGroupBoxInfo = new wxStaticBoxSizer(m_GroupBox_Info, wxVERTICAL);
		sGroupBoxInfo->Add(m_Label_Codename, 0, wxALL, 5);
		sGroupBoxInfo->Add(m_Label_NumCodes, 0, wxALL, 5);
		sGroupBoxInfo->Add(m_ListBox_CodesList, 0, wxALL, 5);

		wxBoxSizer* sB1 = new wxBoxSizer(wxVERTICAL);
		sB1->Add(m_Button_ApplyCodes, 0, wxALL, 5);
		sB1->Add(sGroupBoxInfo, 0, wxALL, 5);

		m_Sizer_TabCheats = new wxGridBagSizer();
		m_Sizer_TabCheats->AddGrowableRow(0);
		m_Sizer_TabCheats->Add(sCheatsListH, wxGBPosition(0, 0), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
		m_Sizer_TabCheats->Add(sB1, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_LEFT|wxALL, 5);

		m_Tab_Cheats->SetSizer(m_Sizer_TabCheats);
		m_Tab_Cheats->Layout();


		// $ Log Tab
		m_Tab_Log = new wxPanel(m_Notebook_Main, ID_TAB_LOG, wxDefaultPosition, wxDefaultSize);

		m_Button_UpdateLog = new wxButton(m_Tab_Log, ID_BUTTON_UPDATELOG, wxT("Update"));
		m_CheckBox_LogAR = new wxCheckBox(m_Tab_Log, ID_CHECKBOX_LOGAR, wxT("Enable AR Logging"));
		m_CheckBox_LogAR->SetValue(IsSelfLogging());
		m_TextCtrl_Log = new wxTextCtrl(m_Tab_Log, ID_TEXTCTRL_LOG, wxT(""), wxDefaultPosition, wxSize(100, 600), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

		wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
		HStrip1->Add(m_CheckBox_LogAR, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
		HStrip1->Add(m_Button_UpdateLog, 0, wxALL, 5);

		wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
		sTabLog->Add(HStrip1, 0, wxALL, 5);
		sTabLog->Add(m_TextCtrl_Log, 1, wxALL|wxEXPAND, 5);

		m_Tab_Log->SetSizer(sTabLog);
		m_Tab_Log->Layout();

	// Add Tabs to Notebook
	m_Notebook_Main->AddPage(m_Tab_Cheats, _T("Codes List"));
	m_Notebook_Main->AddPage(m_Tab_Log, _T("Logging"));

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
void wxCheatsWindow::OnEvent_Window_Resize(wxSizeEvent& WXUNUSED (event))
{
	Layout();
}
void wxCheatsWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED (event))
{
	Destroy();
}
void wxCheatsWindow::OnEvent_Window_Close(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}
void wxCheatsWindow::Load_ARCodes()
{
	indexList.clear();
	size_t size = GetCodeListSize();
	for (size_t i = 0; i < size; i++)
	{
		ARCode code = GetARCode(i);
		ARCodeIndex ind;
		u32 index = m_CheckListBox_CheatsList->Append(wxString::FromAscii(code.name.c_str()));
		m_CheckListBox_CheatsList->Check(index, code.active);
		ind.index = i;
		ind.uiIndex = index;
		indexList.push_back(ind);
	}
}
void wxCheatsWindow::OnEvent_CheatsList_ItemSelected(wxCommandEvent& WXUNUSED (event))
{
	int index = m_CheckListBox_CheatsList->GetSelection();
	for (size_t i = 0; i < indexList.size(); i++)
	{
		if ((int)indexList[i].uiIndex == index)
		{
			ARCode code = GetARCode(i);
			m_Label_Codename->SetLabel(wxT("Name: ") + wxString::FromAscii(code.name.c_str()));
			char text[CHAR_MAX];
			char* numcodes = text;
			sprintf(numcodes, "Number of Codes: %lu", code.ops.size());
			m_Label_NumCodes->SetLabel(wxString::FromAscii(numcodes));
			m_ListBox_CodesList->Clear();
			for (size_t j = 0; j < code.ops.size(); j++)
			{
				char text2[CHAR_MAX];
			    char* ops = text2;
				sprintf(ops, "%08x %08x", code.ops[j].cmd_addr, code.ops[j].value);
				m_ListBox_CodesList->Append(wxString::FromAscii(ops));
			}
		}
	}
	m_Sizer_TabCheats->Layout();
}
void wxCheatsWindow::OnEvent_CheatsList_ItemToggled(wxCommandEvent& WXUNUSED (event))
{
	int index = m_CheckListBox_CheatsList->GetSelection();
	for (size_t i = 0; i < indexList.size(); i++)
	{
		if ((int)indexList[i].uiIndex == index)
		{
			SetARCode_IsActive(m_CheckListBox_CheatsList->IsChecked(index), indexList[i].index);
		}
	}
}
void wxCheatsWindow::OnEvent_ButtonUpdateCodes_Press(wxCommandEvent& WXUNUSED (event))
{
	for (size_t i = 0; i < indexList.size(); i++)
	{
		SetARCode_IsActive(m_CheckListBox_CheatsList->IsChecked(indexList[i].uiIndex), indexList[i].index);
	}
}

void wxCheatsWindow::OnEvent_ButtonUpdateLog_Press(wxCommandEvent& WXUNUSED (event))
{
	m_TextCtrl_Log->Clear();
	const std::vector<std::string> &arLog = GetSelfLog();
	for (u32 i = 0; i < arLog.size(); i++)
	{
		m_TextCtrl_Log->AppendText(wxString::FromAscii(arLog[i].c_str()));
	}
}

void wxCheatsWindow::OnEvent_CheckBoxEnableLogging_StateChange(wxCommandEvent& WXUNUSED (event))
{
	EnableSelfLogging(m_CheckBox_LogAR->IsChecked());
}
