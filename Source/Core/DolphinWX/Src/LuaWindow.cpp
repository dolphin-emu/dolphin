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

#include "LuaWindow.h"
#include "LuaInterface.h"

#include <map>


int wxLuaWindow::luaCount = 0;

// Constant Colors
const unsigned long COLOR_GRAY = 0xDCDCDC;

BEGIN_EVENT_TABLE(wxLuaWindow, wxWindow)
	EVT_SIZE(                                            wxLuaWindow::OnEvent_Window_Resize)
	EVT_CLOSE(                                           wxLuaWindow::OnEvent_Window_Close)
    EVT_BUTTON(ID_BUTTON_CLOSE,                          wxLuaWindow::OnEvent_ButtonClose_Press)
	EVT_BUTTON(ID_BUTTON_LOAD,                           wxLuaWindow::OnEvent_ScriptLoad_Press)
	EVT_BUTTON(ID_BUTTON_RUN,                            wxLuaWindow::OnEvent_ScriptRun_Press)
	EVT_BUTTON(ID_BUTTON_STOP,                           wxLuaWindow::OnEvent_ScriptStop_Press)
END_EVENT_TABLE()

std::map<int, wxLuaWindow *> g_contextMap;

void LuaPrint(int uid, const char *msg)
{
	g_contextMap[uid]->PrintMessage(msg);
}

void LuaStop(int uid, bool ok)
{
	if(ok)
		g_contextMap[uid]->PrintMessage("Script completed successfully!\n");
	else
		g_contextMap[uid]->PrintMessage("Script failed\n");

	g_contextMap[uid]->OnStop();
}

wxLuaWindow::wxLuaWindow(wxFrame* parent, const wxPoint& pos, const wxSize& size) :
	wxFrame(parent, wxID_ANY, _T("Lua Script Console"), pos, size, wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
	// Create Lua context
	luaID = luaCount;
	Lua::OpenLuaContext(luaID, LuaPrint, NULL, LuaStop);
	g_contextMap[luaID] = this;
	luaCount++;
	bScriptRunning = false;

	// Create the GUI controls
	InitGUIControls();

	// Setup Window
	SetBackgroundColour(wxColour(COLOR_GRAY));
	SetSize(size);
	SetPosition(pos);
	Layout();
	Show();
}

wxLuaWindow::~wxLuaWindow()
{
	// On Disposal
	Lua::CloseLuaContext(luaID);
	g_contextMap.erase(luaID);
}

void wxLuaWindow::PrintMessage(const char *text)
{
	m_TextCtrl_Log->AppendText(wxString::FromAscii(text));
}

void wxLuaWindow::InitGUIControls()
{
	// $ Log Console
	m_Tab_Log = new wxPanel(this, ID_TAB_LOG, wxDefaultPosition, wxDefaultSize);
	m_TextCtrl_Log = new wxTextCtrl(m_Tab_Log, ID_TEXTCTRL_LOG, wxT(""), wxDefaultPosition, wxSize(100, 600),
									wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	wxBoxSizer *HStrip1 = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sTabLog = new wxBoxSizer(wxVERTICAL);
	sTabLog->Add(HStrip1, 0, wxALL, 5);
	sTabLog->Add(m_TextCtrl_Log, 1, wxALL|wxEXPAND, 5);

	m_Tab_Log->SetSizer(sTabLog);
	m_Tab_Log->Layout();

	// Button Strip
	m_Button_Close = new wxButton(this, ID_BUTTON_CLOSE, _T("Close"), wxDefaultPosition, wxDefaultSize);
	m_Button_LoadScript = new wxButton(this, ID_BUTTON_LOAD, _T("Load Script..."), wxDefaultPosition, wxDefaultSize);
	m_Button_Run = new wxButton(this, ID_BUTTON_RUN, _T("Run"), wxDefaultPosition, wxDefaultSize);
	m_Button_Stop = new wxButton(this, ID_BUTTON_STOP, _T("Stop"), wxDefaultPosition, wxDefaultSize);
	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);

	m_Button_Run->Disable();
	m_Button_Stop->Disable();

	sButtons->Add(m_Button_Close, 0, wxALL, 5);
	sButtons->Add(m_Button_LoadScript, 0, wxALL, 5);
	sButtons->Add(m_Button_Run, 0, wxALL, 5);
	sButtons->Add(m_Button_Stop, 0, wxALL, 5);

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Tab_Log, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxALL, 5);
	SetSizer(sMain);
	Layout();

	Fit();
}

void wxLuaWindow::OnEvent_ScriptLoad_Press(wxCommandEvent&  WXUNUSED(event)) 
{
	wxString path = wxFileSelector(
		_T("Select the script to load"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		wxString::Format
		(
		_T("Lua Scripts (lua)|*.lua|All files (%s)|%s"),
		wxFileSelectorDefaultWildcardStr,
		wxFileSelectorDefaultWildcardStr
		),
		wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
		this);

	if(!path.IsEmpty())
		currentScript = path;
	else
		return;

	m_TextCtrl_Log->Clear();
	m_TextCtrl_Log->AppendText(wxString::FromAscii(
		StringFromFormat("Script %s loaded successfully.\n", 
						 path.mb_str()).c_str()));
	m_Button_Run->Enable();
}

void wxLuaWindow::OnEvent_ScriptRun_Press(wxCommandEvent&  WXUNUSED(event)) 
{
	m_TextCtrl_Log->AppendText(wxT("Running script...\n"));
	bScriptRunning = true;
	m_Button_LoadScript->Disable();
	m_Button_Run->Disable();
	m_Button_Stop->Enable();

	Lua::RunLuaScriptFile(luaID, (const char *)currentScript.mb_str());
}

void wxLuaWindow::OnEvent_ScriptStop_Press(wxCommandEvent&  WXUNUSED(event)) 
{
	Lua::StopLuaScript(luaID);
	OnStop();
	PrintMessage("Script stopped!\n");
}

void wxLuaWindow::OnStop()
{
	bScriptRunning = false;
	m_Button_LoadScript->Enable();
	m_Button_Run->Enable();
	m_Button_Stop->Disable();
}

void wxLuaWindow::OnEvent_Window_Resize(wxSizeEvent& WXUNUSED (event))
{
	Layout();
}
void wxLuaWindow::OnEvent_ButtonClose_Press(wxCommandEvent& WXUNUSED (event))
{
	Destroy();
}
void wxLuaWindow::OnEvent_Window_Close(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

