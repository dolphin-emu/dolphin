// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <string>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/windowid.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/DebuggerPanel.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/TextureCacheBase.h"

BEGIN_EVENT_TABLE(GFXDebuggerPanel, wxPanel)
	EVT_CLOSE(GFXDebuggerPanel::OnClose)
	EVT_BUTTON(ID_PAUSE,GFXDebuggerPanel::OnPauseButton)
	EVT_BUTTON(ID_PAUSE_AT_NEXT,GFXDebuggerPanel::OnPauseAtNextButton)
	EVT_BUTTON(ID_PAUSE_AT_NEXT_FRAME,GFXDebuggerPanel::OnPauseAtNextFrameButton)
	EVT_BUTTON(ID_CONT,GFXDebuggerPanel::OnContButton)
	EVT_BUTTON(ID_DUMP,GFXDebuggerPanel::OnDumpButton)
	EVT_BUTTON(ID_UPDATE_SCREEN,GFXDebuggerPanel::OnUpdateScreenButton)
	EVT_BUTTON(ID_CLEAR_SCREEN,GFXDebuggerPanel::OnClearScreenButton)
	EVT_BUTTON(ID_CLEAR_TEXTURE_CACHE,GFXDebuggerPanel::OnClearTextureCacheButton)
	EVT_BUTTON(ID_CLEAR_VERTEX_SHADER_CACHE,GFXDebuggerPanel::OnClearVertexShaderCacheButton)
	EVT_BUTTON(ID_CLEAR_PIXEL_SHADER_CACHE,GFXDebuggerPanel::OnClearPixelShaderCacheButton)
END_EVENT_TABLE()

GFXDebuggerPanel::GFXDebuggerPanel(wxWindow *parent, wxWindowID id, const wxPoint &position,
									const wxSize& size, long style, const wxString &title)
	: wxPanel(parent, id, position, size, style, title)
{
	g_pdebugger = this;

	CreateGUIControls();

	LoadSettings();
}

GFXDebuggerPanel::~GFXDebuggerPanel()
{
	g_pdebugger = nullptr;
	GFXDebuggerPauseFlag = false;
}

void GFXDebuggerPanel::OnClose(wxCloseEvent& event)
{
	// save the window position when we hide the window
	SaveSettings();

	event.Skip();
}

void GFXDebuggerPanel::SaveSettings() const
{
	IniFile file;
	file.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	// TODO: make this work when we close the entire program too, currently on total close we get
	// weird values, perhaps because of some conflict with the rendering window

	// TODO: get the screen resolution and make limits from that
	if (GetPosition().x < 1000 &&
	    GetPosition().y < 1000 &&
	    GetSize().GetWidth() < 1000 &&
	    GetSize().GetHeight() < 1000)
	{
		IniFile::Section* video_window = file.GetOrCreateSection("VideoWindow");
		video_window->Set("x", GetPosition().x);
		video_window->Set("y", GetPosition().y);
		video_window->Set("w", GetSize().GetWidth());
		video_window->Set("h", GetSize().GetHeight());
	}

	file.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

void GFXDebuggerPanel::LoadSettings()
{
	IniFile file;
	file.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

	int x = 100;
	int y = 100;
	int w = 100;
	int h = 100;

	IniFile::Section* video_window = file.GetOrCreateSection("VideoWindow");
	video_window->Get("x", &x, GetPosition().x);
	video_window->Get("y", &y, GetPosition().y);
	video_window->Get("w", &w, GetSize().GetWidth());
	video_window->Get("h", &h, GetSize().GetHeight());

	SetSize(x, y, w, h);
}

struct PauseEventMap
{
	PauseEvent event;
	const wxString ListStr;
};

static PauseEventMap* pauseEventMap;

void GFXDebuggerPanel::CreateGUIControls()
{
	static PauseEventMap map[] = {
		{NEXT_FRAME,                _("Frame")},
		{NEXT_FLUSH,                _("Flush")},

		{NEXT_PIXEL_SHADER_CHANGE,  _("Pixel Shader")},
		{NEXT_VERTEX_SHADER_CHANGE, _("Vertex Shader")},
		{NEXT_TEXTURE_CHANGE,       _("Texture")},
		{NEXT_NEW_TEXTURE,          _("New Texture")},

		{NEXT_XFB_CMD,              _("XFB Cmd")},
		{NEXT_EFB_CMD,              _("EFB Cmd")},

		{NEXT_MATRIX_CMD,           _("Matrix Cmd")},
		{NEXT_VERTEX_CMD,           _("Vertex Cmd")},
		{NEXT_TEXTURE_CMD,          _("Texture Cmd")},
		{NEXT_LIGHT_CMD,            _("Light Cmd")},
		{NEXT_FOG_CMD,              _("Fog Cmd")},

		{NEXT_SET_TLUT,             _("TLUT Cmd")},

		{NEXT_ERROR,                _("Error")}
	};
	pauseEventMap = map;
	const int numPauseEventMap = sizeof(map)/sizeof(PauseEventMap);

	// Basic settings
	CenterOnParent();

	m_pButtonPause = new wxButton(this, ID_PAUSE, _("Pause"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Pause"));
	m_pButtonPauseAtNext = new wxButton(this, ID_PAUSE_AT_NEXT, _("Pause After"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Pause At Next"));
	m_pButtonPauseAtNextFrame = new wxButton(this, ID_PAUSE_AT_NEXT_FRAME, _("Go to Next Frame"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Next Frame"));
	m_pButtonCont = new wxButton(this, ID_CONT, _("Continue"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Continue"));

	m_pCount = new wxTextCtrl(this, ID_COUNT, "1", wxDefaultPosition, wxSize(50,25), wxTE_RIGHT, wxDefaultValidator, _("Count"));

	m_pPauseAtList = new wxChoice(this, ID_PAUSE_AT_LIST, wxDefaultPosition, wxSize(100,25), 0, nullptr,0,wxDefaultValidator, _("PauseAtList"));
	for (int i=0; i<numPauseEventMap; i++)
	{
		m_pPauseAtList->Append(pauseEventMap[i].ListStr);
	}
	m_pPauseAtList->SetSelection(0);

	m_pButtonDump = new wxButton(this, ID_DUMP, _("Dump"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Dump"));
	m_pButtonUpdateScreen = new wxButton(this, ID_UPDATE_SCREEN, _("Update Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Update Screen"));
	m_pButtonClearScreen = new wxButton(this, ID_CLEAR_SCREEN, _("Clear Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Clear Screen"));
	m_pButtonClearTextureCache = new wxButton(this, ID_CLEAR_TEXTURE_CACHE, _("Clear Textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Clear Textures"));
	m_pButtonClearVertexShaderCache = new wxButton(this, ID_CLEAR_VERTEX_SHADER_CACHE, _("Clear V Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Clear V Shaders"));
	m_pButtonClearPixelShaderCache = new wxButton(this, ID_CLEAR_PIXEL_SHADER_CACHE, _("Clear P Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _("Clear P Shaders"));

	m_pDumpList = new wxChoice(this, ID_DUMP_LIST, wxDefaultPosition, wxSize(120,25), 0, nullptr, 0 ,wxDefaultValidator, _("DumpList"));
	m_pDumpList->Insert(_("Pixel Shader"),0);
	m_pDumpList->Append(_("Vertex Shader"));
	m_pDumpList->Append(_("Pixel Shader Constants"));
	m_pDumpList->Append(_("Vertex Shader Constants"));
	m_pDumpList->Append(_("Textures"));
	m_pDumpList->Append(_("Frame Buffer"));
	m_pDumpList->Append(_("Geometry data"));
	m_pDumpList->Append(_("Vertex Description"));
	m_pDumpList->Append(_("Vertex Matrices"));
	m_pDumpList->Append(_("Statistics"));
	m_pDumpList->SetSelection(0);

	// Layout everything on m_MainPanel
	wxBoxSizer *sMain = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* const pFlowCtrlBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Flow Control"));
	wxBoxSizer* const pPauseAtNextSzr = new wxBoxSizer(wxHORIZONTAL);
	pFlowCtrlBox->Add(m_pButtonPause);
	pPauseAtNextSzr->Add(m_pButtonPauseAtNext);
	pPauseAtNextSzr->Add(m_pCount);
	pPauseAtNextSzr->Add(m_pPauseAtList);
	pFlowCtrlBox->Add(pPauseAtNextSzr);
	pFlowCtrlBox->Add(m_pButtonPauseAtNextFrame);
	pFlowCtrlBox->Add(m_pButtonCont);

	wxStaticBoxSizer* const pDebugBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Debugging"));
	wxBoxSizer* const pDumpSzr = new wxBoxSizer(wxHORIZONTAL);
	pDumpSzr->Add(m_pButtonDump);
	pDumpSzr->Add(m_pDumpList);
	pDebugBox->Add(pDumpSzr);
	wxGridSizer* const pDbgGrid = new wxGridSizer(2, 5, 5);
	pDbgGrid->Add(m_pButtonUpdateScreen);
	pDbgGrid->Add(m_pButtonClearScreen);
	pDbgGrid->Add(m_pButtonClearTextureCache);
	pDbgGrid->Add(m_pButtonClearVertexShaderCache);
	pDbgGrid->Add(m_pButtonClearPixelShaderCache);
	pDebugBox->Add(pDbgGrid);

	sMain->Add(pFlowCtrlBox, 0, 0, 5);
	sMain->Add(pDebugBox, 0, 0, 5);
	SetSizerAndFit(sMain);

	OnContinue();
}

void GFXDebuggerPanel::OnPause()
{
	m_pButtonDump->Enable(true);
	m_pDumpList->Enable(true);
	m_pButtonUpdateScreen->Enable(true);
	m_pButtonClearScreen->Enable(true);
	m_pButtonClearTextureCache->Enable(true);
	m_pButtonClearVertexShaderCache->Enable(true);
	m_pButtonClearPixelShaderCache->Enable(true);
}

void GFXDebuggerPanel::OnContinue()
{
	m_pButtonDump->Enable(false);
	m_pDumpList->Enable(false);
	m_pButtonUpdateScreen->Enable(false);
	m_pButtonClearScreen->Enable(false);
	m_pButtonClearTextureCache->Enable(false);
	m_pButtonClearVertexShaderCache->Enable(false);
	m_pButtonClearPixelShaderCache->Enable(false);
}


// General settings
void GFXDebuggerPanel::GeneralSettings(wxCommandEvent& event)
{
	SaveSettings();
}

void GFXDebuggerPanel::OnPauseButton(wxCommandEvent& event)
{
	GFXDebuggerPauseFlag = true;
}

void GFXDebuggerPanel::OnPauseAtNextButton(wxCommandEvent& event)
{
	GFXDebuggerPauseFlag = false;
	GFXDebuggerToPauseAtNext = pauseEventMap[m_pPauseAtList->GetSelection()].event;
	wxString val = m_pCount->GetValue();
	long value;
	if (val.ToLong(&value))
		GFXDebuggerEventToPauseCount = value;
	else
		GFXDebuggerEventToPauseCount = 1;
}

void GFXDebuggerPanel::OnPauseAtNextFrameButton(wxCommandEvent& event)
{
	GFXDebuggerPauseFlag = false;
	GFXDebuggerToPauseAtNext = NEXT_FRAME;
	GFXDebuggerEventToPauseCount = 1;
}

void GFXDebuggerPanel::OnDumpButton(wxCommandEvent& event)
{
	std::string dump_path = File::GetUserPath(D_DUMP_IDX) + "Debug/" +
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID + "/";
	if (!File::CreateFullPath(dump_path))
		return;

	switch (m_pDumpList->GetSelection())
	{
		case 0: // Pixel Shader
			DumpPixelShader(dump_path);
			break;

		case 1: // Vertex Shader
			DumpVertexShader(dump_path);
			break;

		case 2: // Pixel Shader Constants
			DumpPixelShaderConstants(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 3: // Vertex Shader Constants
			DumpVertexShaderConstants(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 4: // Textures
			DumpTextures(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 5: // Frame Buffer
			DumpFrameBuffer(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 6: // Geometry
			DumpGeometry(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 7: // Vertex Description
			DumpVertexDecl(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 8: // Vertex Matrices
			DumpMatrices(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;

		case 9: // Statistics
			DumpStats(dump_path);
			WxUtils::ShowErrorDialog(_("Not implemented"));
			break;
	}
}

void GFXDebuggerPanel::OnContButton(wxCommandEvent& event)
{
	GFXDebuggerToPauseAtNext = NOT_PAUSE;
	GFXDebuggerPauseFlag = false;
}

void GFXDebuggerPanel::OnClearScreenButton(wxCommandEvent& event)
{
	// TODO
	WxUtils::ShowErrorDialog(_("Not implemented"));
}

void GFXDebuggerPanel::OnClearTextureCacheButton(wxCommandEvent& event)
{
	TextureCache::Invalidate();
}

void GFXDebuggerPanel::OnClearVertexShaderCacheButton(wxCommandEvent& event)
{
	// TODO
	WxUtils::ShowErrorDialog(_("Not implemented"));
}

void GFXDebuggerPanel::OnClearPixelShaderCacheButton(wxCommandEvent& event)
{
	// TODO
	WxUtils::ShowErrorDialog(_("Not implemented"));
}

void GFXDebuggerPanel::OnUpdateScreenButton(wxCommandEvent& event)
{
	WxUtils::ShowErrorDialog(_("Not implemented"));
	GFXDebuggerUpdateScreen();
}
