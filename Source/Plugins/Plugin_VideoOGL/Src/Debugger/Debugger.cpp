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

#include "VideoConfig.h"
#include "../Globals.h"
#include "FileUtil.h"

extern int g_Preset;

BEGIN_EVENT_TABLE(GFXDebuggerOGL,wxPanel)
	EVT_CLOSE(GFXDebuggerOGL::OnClose)
	EVT_CHECKBOX(ID_SAVETOFILE,GFXDebuggerOGL::GeneralSettings)
	EVT_CHECKBOX(ID_INFOLOG,GFXDebuggerOGL::GeneralSettings)
	EVT_CHECKBOX(ID_PRIMLOG,GFXDebuggerOGL::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETEXTURES,GFXDebuggerOGL::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETARGETS,GFXDebuggerOGL::GeneralSettings)
	EVT_CHECKBOX(ID_SAVESHADERS,GFXDebuggerOGL::GeneralSettings)
END_EVENT_TABLE()

GFXDebuggerOGL::GFXDebuggerOGL(wxWindow *parent, wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style, const wxString &title)
	: wxPanel(parent, id, pos, size, style, title)
{
	CreateGUIControls();

	LoadSettings();
}

GFXDebuggerOGL::~GFXDebuggerOGL()
{
}

void GFXDebuggerOGL::OnClose(wxCloseEvent& event)
{
	// Save the window position
	SaveSettings();

	event.Skip();
}

void GFXDebuggerOGL::SaveSettings() const
{
	IniFile file;
	file.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

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

	g_Config.iLog = bInfoLog ? CONF_LOG : 0;
	g_Config.iLog |= bPrimLog ? CONF_PRIMLOG : 0;
	g_Config.iLog |= bSaveTextures ? CONF_SAVETEXTURES : 0;
	g_Config.iLog |= bSaveTargets ? CONF_SAVETARGETS : 0;
	g_Config.iLog |= bSaveShaders ? CONF_SAVESHADERS : 0;

	file.Set("VideoWindow", "ConfBits", g_Config.iLog);

	file.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

void GFXDebuggerOGL::LoadSettings()
{
	IniFile file;
	file.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	int x = 100, y = 100, w = 100, h = 100;
	file.Get("VideoWindow", "x", &x, GetPosition().x);
	file.Get("VideoWindow", "y", &y, GetPosition().y);
	file.Get("VideoWindow", "w", &w, GetSize().GetWidth());
	file.Get("VideoWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	file.Get("VideoWindow", "ConfBits", &g_Config.iLog, 0);
	bInfoLog = (g_Config.iLog & CONF_LOG) ? true : false;
	bPrimLog = (g_Config.iLog & CONF_PRIMLOG) ? true : false;
	bSaveTextures = (g_Config.iLog & CONF_SAVETEXTURES) ? true : false;
	bSaveTargets = (g_Config.iLog & CONF_SAVETARGETS) ? true : false;
	bSaveShaders = (g_Config.iLog & CONF_SAVESHADERS) ? true : false;
	m_Check[1]->SetValue(bInfoLog);
	m_Check[2]->SetValue(bPrimLog);
	m_Check[3]->SetValue(bSaveTextures);
	m_Check[4]->SetValue(bSaveTargets);
	m_Check[5]->SetValue(bSaveShaders);
}

void GFXDebuggerOGL::CreateGUIControls()
{
	// Basic settings
	CenterOnParent();

	// Options
	wxStaticBoxSizer *sOptions = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Options"));
	m_Check[0] = new wxCheckBox(this, ID_SAVETOFILE, wxT("Save to file"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[1] = new wxCheckBox(this, ID_INFOLOG, wxT("Info log"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[2] = new wxCheckBox(this, ID_PRIMLOG, wxT("Primary log"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[3] = new wxCheckBox(this, ID_SAVETEXTURES, wxT("Save Textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[4] = new wxCheckBox(this, ID_SAVETARGETS, wxT("Save Targets"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Check[5] = new wxCheckBox(this, ID_SAVESHADERS, wxT("Save Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	for (int i = 0; i < NUM_OPTIONS-ID_SAVETOFILE; ++i)
		sOptions->Add(m_Check[i], 0, 0, 5);

	// Layout everything
	wxBoxSizer *sMain = new wxBoxSizer(wxHORIZONTAL);
	sMain->Add(sOptions);
	SetSizerAndFit(sMain);
	Fit();
}

// General settings
void GFXDebuggerOGL::GeneralSettings(wxCommandEvent& event)
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
