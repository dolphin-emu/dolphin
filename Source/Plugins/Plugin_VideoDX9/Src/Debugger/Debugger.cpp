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


#include "IniFile.h"
#include "Debugger.h"

#include "Config.h"
#include "../Globals.h"
#include "../D3DBase.h"

extern int g_Preset;

BEGIN_EVENT_TABLE(GFXDebuggerDX9,wxDialog)
	EVT_CLOSE(GFXDebuggerDX9::OnClose)
	EVT_CHECKBOX(ID_SAVETOFILE,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_INFOLOG,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_PRIMLOG,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETEXTURES,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETARGETS,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVESHADERS,GFXDebuggerDX9::GeneralSettings)
	EVT_BUTTON(ID_PAUSE,GFXDebuggerDX9::OnPauseButton)
	EVT_BUTTON(ID_PAUSE_AT_NEXT,GFXDebuggerDX9::OnPauseAtNextButton)
	EVT_BUTTON(ID_GO,GFXDebuggerDX9::OnGoButton)
	EVT_BUTTON(ID_DUMP,GFXDebuggerDX9::OnDumpButton)
	EVT_BUTTON(ID_UPDATE_SCREEN,GFXDebuggerDX9::OnUpdateScreenButton)
	EVT_BUTTON(ID_CLEAR_SCREEN,GFXDebuggerDX9::OnClearScreenButton)
END_EVENT_TABLE()

GFXDebuggerDX9::GFXDebuggerDX9(wxWindow *parent, wxWindowID id, const wxString &title,
				   const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();

	LoadSettings();
}

GFXDebuggerDX9::~GFXDebuggerDX9()
{
	SaveSettings();
}

void GFXDebuggerDX9::OnClose(wxCloseEvent& event)
{
	// save the window position when we hide the window
	SaveSettings();

	event.Skip(); // This means wxDialog's Destroy is used
}

void GFXDebuggerDX9::SaveSettings() const
{
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);

	// TODO: make this work when we close the entire program too, currently on total close we get
	// weird values, perhaps because of some conflict with the rendering window
	// TODO: get the screen resolution and make limits from that
	if (GetPosition().x < 1000 && GetPosition().y < 1000
		                       && GetSize().GetWidth() < 1000 
							   && GetSize().GetHeight() < 1000)
	{
		file.Set("VideoWindow", "x", GetPosition().x);
		file.Set("VideoWindow", "y", GetPosition().y);
		file.Set("VideoWindow", "w", GetSize().GetWidth());
		file.Set("VideoWindow", "h", GetSize().GetHeight());
	}

	file.Set("VideoWindow", "WriteToFile", m_Check[0]->IsChecked());

	//g_Config.iLog = bInfoLog ? CONF_LOG : 0;
	//g_Config.iLog |= bPrimLog ? CONF_PRIMLOG : 0;
	//g_Config.iLog |= bSaveTextures ? CONF_SAVETEXTURES : 0;
	//g_Config.iLog |= bSaveTargets ? CONF_SAVETARGETS : 0;
	//g_Config.iLog |= bSaveShaders ? CONF_SAVESHADERS : 0;

	//file.Set("VideoWindow", "ConfBits", g_Config.iLog);

	file.Save(DEBUGGER_CONFIG_FILE);
}

void GFXDebuggerDX9::LoadSettings()
{
	IniFile file;
	file.Load(DEBUGGER_CONFIG_FILE);

	int x = 100, y = 100, w = 100, h = 100;
	file.Get("VideoWindow", "x", &x, GetPosition().x);
	file.Get("VideoWindow", "y", &y, GetPosition().y);
	file.Get("VideoWindow", "w", &w, GetSize().GetWidth());
	file.Get("VideoWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	//file.Get("VideoWindow", "ConfBits", &g_Config.iLog, 0);
	//bInfoLog = (g_Config.iLog & CONF_LOG) ? true : false;
	//bPrimLog = (g_Config.iLog & CONF_PRIMLOG) ? true : false;
	//bSaveTextures = (g_Config.iLog & CONF_SAVETEXTURES) ? true : false;
	//bSaveTargets = (g_Config.iLog & CONF_SAVETARGETS) ? true : false;
	//bSaveShaders = (g_Config.iLog & CONF_SAVESHADERS) ? true : false;
	//m_Check[1]->SetValue(bInfoLog);
	//m_Check[2]->SetValue(bPrimLog);
	//m_Check[3]->SetValue(bSaveTextures);
	//m_Check[4]->SetValue(bSaveTargets);
	//m_Check[5]->SetValue(bSaveShaders);
}

void GFXDebuggerDX9::CreateGUIControls()
{
	// Basic settings
	SetIcon(wxNullIcon);
	CenterOnParent();

	// MainPanel
	m_MainPanel = new wxPanel(this, ID_MAINPANEL, wxDefaultPosition, wxDefaultSize);

	// Options
	wxStaticBoxSizer *sOptions = new wxStaticBoxSizer(wxVERTICAL, m_MainPanel, wxT("Options"));
	m_Check[0] = new wxCheckBox(m_MainPanel, ID_SAVETOFILE, wxT("Save to file"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1] = new wxCheckBox(m_MainPanel, ID_INFOLOG, wxT("Info log"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[2] = new wxCheckBox(m_MainPanel, ID_PRIMLOG, wxT("Primary log"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[3] = new wxCheckBox(m_MainPanel, ID_SAVETEXTURES, wxT("Save Textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[4] = new wxCheckBox(m_MainPanel, ID_SAVETARGETS, wxT("Save Targets"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[5] = new wxCheckBox(m_MainPanel, ID_SAVESHADERS, wxT("Save Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	m_pButtonPause = new wxButton(m_MainPanel, ID_PAUSE, wxT("Pause"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Pause"));
	m_pButtonPauseAtNext = new wxButton(m_MainPanel, ID_PAUSE_AT_NEXT, wxT("Pause At Next"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Pause At Next"));
	m_pButtonGo = new wxButton(m_MainPanel, ID_GO, wxT("Go"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Go"));

	m_pPauseAtList = new wxChoice(m_MainPanel, ID_PAUSE_AT_LIST, wxDefaultPosition, wxSize(100,25), 0, NULL,0,wxDefaultValidator, wxT("PauseAtList"));
	//m_pPauseAtList->SetEditable(false);
	m_pPauseAtList->Insert(wxT("Frame"),0);
	m_pPauseAtList->Append(wxT("Flush"));
	m_pPauseAtList->Append(wxT("Fifo"));
	m_pPauseAtList->Append(wxT("Dlist Call"));
	m_pPauseAtList->Append(wxT("Pixel Shader Change"));
	m_pPauseAtList->Append(wxT("Vertex Shader Change"));
	m_pPauseAtList->Append(wxT("New Texture"));
	m_pPauseAtList->Append(wxT("Render To Texture"));
	m_pPauseAtList->Append(wxT("Matrix Ucode"));
	m_pPauseAtList->Append(wxT("Vertex Ucode"));
	m_pPauseAtList->Append(wxT("Texture Ucode"));
	m_pPauseAtList->Append(wxT("Light Ucode"));
	m_pPauseAtList->Append(wxT("Frame Buffer Command"));
	m_pPauseAtList->Append(wxT("Fog Command"));
	m_pPauseAtList->Append(wxT("Set Color Constant"));
	m_pPauseAtList->Append(wxT("Ucode"));
	m_pPauseAtList->SetSelection(0);

	m_pButtonDump = new wxButton(m_MainPanel, ID_DUMP, wxT("Dump"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Dump"));
	m_pButtonUpdateScreen = new wxButton(m_MainPanel, ID_UPDATE_SCREEN, wxT("Update Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Update Screen"));
	m_pButtonClearScreen = new wxButton(m_MainPanel, ID_CLEAR_SCREEN, wxT("Clear Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Clear Screen"));
	m_pCount = new wxTextCtrl(m_MainPanel, ID_COUNT, wxT("1"), wxDefaultPosition, wxSize(50,25), 0, wxDefaultValidator, wxT("Count"));

	m_pDumpList = new wxChoice(m_MainPanel, ID_DUMP_LIST, wxDefaultPosition, wxSize(120,25), 0, NULL,0,wxDefaultValidator, wxT("DumpList"));
	m_pDumpList->Insert(wxT("Pixel Shader"),0);
	m_pDumpList->Append(wxT("Vertex Shader"));
	m_pDumpList->Append(wxT("Pixel Shader Constants"));
	m_pDumpList->Append(wxT("Vertex Shader Constants"));
	m_pDumpList->Append(wxT("Texture 0"));
	m_pDumpList->Append(wxT("Texture 1"));
	m_pDumpList->Append(wxT("Texture 2"));
	m_pDumpList->Append(wxT("Texture 3"));
	m_pDumpList->Append(wxT("Texture 4"));
	m_pDumpList->Append(wxT("Texture 5"));
	m_pDumpList->Append(wxT("Texture 6"));
	m_pDumpList->Append(wxT("Texture 8"));
	m_pDumpList->Append(wxT("Frame Buffer"));
	m_pDumpList->Append(wxT("Vertices"));
	m_pDumpList->Append(wxT("Vertex Description"));
	m_pDumpList->Append(wxT("Vertex Matrices"));
	m_pDumpList->Append(wxT("Statistics"));
	m_pDumpList->SetSelection(0);

    for (int i = 0; i < NUM_OPTIONS-ID_SAVETOFILE; ++i)
		sOptions->Add(m_Check[i], 0, 0, 5);

	// Layout everything on m_MainPanel
	wxBoxSizer *sMain = new wxBoxSizer(wxVERTICAL);

	sMain->Add(sOptions);
	sMain->Add(m_pButtonPause, 0, 0, 5);
	sMain->Add(m_pButtonPauseAtNext, 0, 0, 5);
	sMain->Add(m_pCount,0,0,5);
	sMain->Add(m_pPauseAtList, 0, 0, 5);
	sMain->Add(m_pButtonDump, 0, 0, 5);
	sMain->Add(m_pDumpList, 0, 0, 5);
	sMain->Add(m_pButtonUpdateScreen, 0, 0, 5);
	sMain->Add(m_pButtonClearScreen, 0, 0, 5);
	sMain->Add(m_pButtonGo, 0, 0, 5);
	m_MainPanel->SetSizerAndFit(sMain);
	Fit();
}

// General settings
void GFXDebuggerDX9::GeneralSettings(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_INFOLOG:
		bInfoLog = event.IsChecked();
		break;
	case ID_PRIMLOG:
		bPrimLog = event.IsChecked();
		break;
	case ID_SAVETEXTURES:
		bSaveTextures = event.IsChecked();
		break;
	case ID_SAVETARGETS:
		bSaveTargets = event.IsChecked();
		break;
	case ID_SAVESHADERS:
		bSaveShaders = event.IsChecked();
		break;
	}
	SaveSettings();
}

volatile bool DX9DebuggerPauseFlag = false;
volatile PauseEvent DX9DebuggerToPauseAtNext = NOT_PAUSE;
volatile int DX9DebuggerEventToPauseCount = 0;

void GFXDebuggerDX9::OnPauseButton(wxCommandEvent& event)
{
	DX9DebuggerPauseFlag = true;
}

void GFXDebuggerDX9::OnPauseAtNextButton(wxCommandEvent& event)
{
	DX9DebuggerPauseFlag = false;
	DX9DebuggerToPauseAtNext = (PauseEvent)(m_pPauseAtList->GetSelection());
	wxString val = m_pCount->GetValue();
	long value;
	if (val.ToLong(&value) )
		DX9DebuggerEventToPauseCount = PauseEvent(value);
	else
		DX9DebuggerEventToPauseCount = 1;
}

void GFXDebuggerDX9::OnDumpButton(wxCommandEvent& event)
{
}

void GFXDebuggerDX9::OnGoButton(wxCommandEvent& event)
{
	DX9DebuggerToPauseAtNext = NOT_PAUSE;
	DX9DebuggerPauseFlag = false;
}

void GFXDebuggerDX9::OnUpdateScreenButton(wxCommandEvent& event)
{
}
void GFXDebuggerDX9::OnClearScreenButton(wxCommandEvent& event)
{
}

void UpdateFPSDisplay(const char *text);
void DX9DebuggerCheckAndPause()
{
	while( DX9DebuggerPauseFlag )
	{
		UpdateFPSDisplay("Paused by Video Debugger");
		Sleep(5);
	}
}

void ContinueDX9Debugger()
{
	DX9DebuggerPauseFlag = false;
}

#ifdef ENABLE_DX_DEBUGGER

void DX9Debugger_Pause_Count_N(PauseEvent event,bool update)
{
	if (DX9DebuggerToPauseAtNext == event || DX9DebuggerPauseFlag)
	{
		DX9DebuggerEventToPauseCount--;
		if (DX9DebuggerEventToPauseCount<=0 || DX9DebuggerPauseFlag)
		{
			DX9DebuggerToPauseAtNext = NOT_PAUSE;
			DX9DebuggerPauseFlag = true;

			if (update)
			{
				D3D::EndFrame();
				D3D::BeginFrame(false);
			}
			DX9DebuggerCheckAndPause();
		}
	}
}

#endif ENABLE_DX_DEBUGGER


