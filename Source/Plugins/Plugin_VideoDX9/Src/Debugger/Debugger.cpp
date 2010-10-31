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
#include "FileUtil.h"

#include "VideoConfig.h"
#include "../Globals.h"
#include "../D3DBase.h"
#include "../FramebufferManager.h"
#include "../TextureCache.h"
#include "../VertexShaderCache.h"
#include "../PixelShaderCache.h"

extern int g_Preset;

BEGIN_EVENT_TABLE(GFXDebuggerDX9, wxPanel)
	EVT_CLOSE(GFXDebuggerDX9::OnClose)
	EVT_CHECKBOX(ID_SAVETOFILE,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_INFOLOG,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_PRIMLOG,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETEXTURES,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVETARGETS,GFXDebuggerDX9::GeneralSettings)
	EVT_CHECKBOX(ID_SAVESHADERS,GFXDebuggerDX9::GeneralSettings)
	EVT_BUTTON(ID_PAUSE,GFXDebuggerDX9::OnPauseButton)
	EVT_BUTTON(ID_PAUSE_AT_NEXT,GFXDebuggerDX9::OnPauseAtNextButton)
	EVT_BUTTON(ID_PAUSE_AT_NEXT_FRAME,GFXDebuggerDX9::OnPauseAtNextFrameButton)
	EVT_BUTTON(ID_GO,GFXDebuggerDX9::OnGoButton)
	EVT_BUTTON(ID_DUMP,GFXDebuggerDX9::OnDumpButton)
	EVT_BUTTON(ID_UPDATE_SCREEN,GFXDebuggerDX9::OnUpdateScreenButton)
	EVT_BUTTON(ID_CLEAR_SCREEN,GFXDebuggerDX9::OnClearScreenButton)
	EVT_BUTTON(ID_CLEAR_TEXTURE_CACHE,GFXDebuggerDX9::OnClearTextureCacheButton)
	EVT_BUTTON(ID_CLEAR_VERTEX_SHADER_CACHE,GFXDebuggerDX9::OnClearVertexShaderCacheButton)
	EVT_BUTTON(ID_CLEAR_PIXEL_SHADER_CACHE,GFXDebuggerDX9::OnClearPixelShaderCacheButton)
END_EVENT_TABLE()


GFXDebuggerDX9::GFXDebuggerDX9(wxWindow *parent, wxWindowID id, const wxPoint &position,
							   const wxSize& size, long style, const wxString &title)
	: wxPanel(parent, id, position, size, style, title)
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

	//g_Config.iLog = bInfoLog ? CONF_LOG : 0;
	//g_Config.iLog |= bPrimLog ? CONF_PRIMLOG : 0;
	//g_Config.iLog |= bSaveTextures ? CONF_SAVETEXTURES : 0;
	//g_Config.iLog |= bSaveTargets ? CONF_SAVETARGETS : 0;
	//g_Config.iLog |= bSaveShaders ? CONF_SAVESHADERS : 0;

	//file.Set("VideoWindow", "ConfBits", g_Config.iLog);

	file.Save(File::GetUserPath(F_DEBUGGERCONFIG_IDX));
}

void GFXDebuggerDX9::LoadSettings()
{
	IniFile file;
	file.Load(File::GetUserPath(F_DEBUGGERCONFIG_IDX));

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

struct PauseEventMap
{
	PauseEvent event;
	const wxString ListStr;
};

static PauseEventMap pauseEventMap[] = {
	{NEXT_FRAME,				wxT("Frame")},
	{NEXT_FLUSH,				wxT("Flush")},

	{NEXT_PIXEL_SHADER_CHANGE,	wxT("Pixel Shader")},
	{NEXT_VERTEX_SHADER_CHANGE,	wxT("Vertex Shader")},
	{NEXT_TEXTURE_CHANGE,		wxT("Texture")},
	{NEXT_NEW_TEXTURE,			wxT("New Texture")},

	{NEXT_XFB_CMD,				wxT("XFB Cmd")},
	{NEXT_EFB_CMD,				wxT("EFB Cmd")},

	{NEXT_MATRIX_CMD,			wxT("Matrix Cmd")},
	{NEXT_VERTEX_CMD,			wxT("Vertex Cmd")},
	{NEXT_TEXTURE_CMD,			wxT("Texture Cmd")},
	{NEXT_LIGHT_CMD,			wxT("Light Cmd")},
	{NEXT_FOG_CMD,				wxT("Fog Cmd")},

	{NEXT_SET_TLUT,				wxT("TLUT Cmd")},

	{NEXT_ERROR,				wxT("Error")}
};
static const int numPauseEventMap = sizeof(pauseEventMap)/sizeof(PauseEventMap);


static GFXDebuggerDX9 *g_pdebugger = NULL;

void GFXDebuggerDX9::CreateGUIControls()
{
	g_pdebugger = this;

	// Basic settings
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
	m_pButtonPauseAtNextFrame = new wxButton(m_MainPanel, ID_PAUSE_AT_NEXT_FRAME, wxT("Next Frame"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Next Frame"));
	m_pButtonGo = new wxButton(m_MainPanel, ID_GO, wxT("Go"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Go"));

	m_pPauseAtList = new wxChoice(m_MainPanel, ID_PAUSE_AT_LIST, wxDefaultPosition, wxSize(100,25), 0, NULL,0,wxDefaultValidator, wxT("PauseAtList"));
	for (int i=0; i<numPauseEventMap; i++)
	{
		m_pPauseAtList->Append(pauseEventMap[i].ListStr);
	}

	m_pPauseAtList->SetSelection(0);

	m_pButtonDump = new wxButton(m_MainPanel, ID_DUMP, wxT("Dump"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Dump"));
	m_pButtonUpdateScreen = new wxButton(m_MainPanel, ID_UPDATE_SCREEN, wxT("Update Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Update Screen"));
	m_pButtonClearScreen = new wxButton(m_MainPanel, ID_CLEAR_SCREEN, wxT("Clear Screen"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Clear Screen"));
	m_pButtonClearTextureCache = new wxButton(m_MainPanel, ID_CLEAR_TEXTURE_CACHE, wxT("Clear Textures"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Clear Textures"));
	m_pButtonClearVertexShaderCache = new wxButton(m_MainPanel, ID_CLEAR_VERTEX_SHADER_CACHE, wxT("Clear V Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Clear V Shaders"));
	m_pButtonClearPixelShaderCache = new wxButton(m_MainPanel, ID_CLEAR_PIXEL_SHADER_CACHE, wxT("Clear P Shaders"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("Clear P Shaders"));
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
	m_pDumpList->Append(wxT("Texture 7"));
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
	sMain->Add(m_pButtonClearTextureCache, 0, 0, 5);
	sMain->Add(m_pButtonClearVertexShaderCache, 0, 0, 5);
	sMain->Add(m_pButtonClearPixelShaderCache, 0, 0, 5);
	sMain->Add(m_pButtonPauseAtNextFrame, 0, 0, 5);
	sMain->Add(m_pButtonGo, 0, 0, 5);
	m_MainPanel->SetSizerAndFit(sMain);
	Fit();

	EnableButtons(false);
}

void GFXDebuggerDX9::EnableButtons(bool enable)
{
	m_pButtonDump->Enable(enable);
	m_pButtonUpdateScreen->Enable(enable);
	m_pButtonClearScreen->Enable(enable);
	m_pButtonClearTextureCache->Enable(enable);
	m_pButtonClearVertexShaderCache->Enable(enable);
	m_pButtonClearPixelShaderCache->Enable(enable);
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
	DX9DebuggerToPauseAtNext = pauseEventMap[m_pPauseAtList->GetSelection()].event;
	wxString val = m_pCount->GetValue();
	long value;
	if (val.ToLong(&value) )
		DX9DebuggerEventToPauseCount = value;
	else
		DX9DebuggerEventToPauseCount = 1;
}

void GFXDebuggerDX9::OnPauseAtNextFrameButton(wxCommandEvent& event)
{
	DX9DebuggerPauseFlag = false;
	DX9DebuggerToPauseAtNext = NEXT_FRAME;
	DX9DebuggerEventToPauseCount = 1;
}

void GFXDebuggerDX9::OnDumpButton(wxCommandEvent& event)
{
	switch (m_pDumpList->GetSelection())
	{
		case 0: // Pixel Shader
		{
			char filename[MAX_PATH];
			sprintf(filename, "%s/Debug/%s/dump_ps.txt", File::GetUserPath(D_DUMP_IDX), globals->unique_id);
			File::CreateEmptyFile(filename);
			File::WriteStringToFile(true, PixelShaderCache::GetCurrentShaderCode(), filename);
			break;
		}

		case 1: // Vertex Shader
		{
			char filename[MAX_PATH];
			sprintf(filename, "%s/Debug/%s/dump_vs.txt", File::GetUserPath(D_DUMP_IDX), globals->unique_id);
			File::CreateEmptyFile(filename);
			File::WriteStringToFile(true, VertexShaderCache::GetCurrentShaderCode(), filename);
			break;
		}

		case 2: // Pixel Shader Constants
		{
			char filename[MAX_PATH];
			sprintf(filename, "%s/Debug/%s/dump_ps_consts.txt", File::GetUserPath(D_DUMP_IDX), globals->unique_id);

			FILE* file = fopen(filename, "w");

			float constants[4*C_PENVCONST_END];
			D3D::dev->GetVertexShaderConstantF(0, constants, C_PENVCONST_END);

			for(unsigned int i = C_COLORS;i < C_KCOLORS; i++)
				fprintf(file, "Constant POSNORMALMATRIX %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_KCOLORS;i < C_ALPHA; i++)
				fprintf(file, "Constant KCOLORS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_ALPHA;i < C_TEXDIMS; i++)
				fprintf(file, "Constant ALPHA %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_TEXDIMS;i < C_ZBIAS; i++)
				fprintf(file, "Constant TEXDIMS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_ZBIAS;i < C_INDTEXSCALE; i++)
				fprintf(file, "Constant ZBIAS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_INDTEXSCALE;i < C_INDTEXMTX; i++)
				fprintf(file, "Constant INDTEXSCALE %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_INDTEXMTX;i < C_FOG; i++)
				fprintf(file, "Constant INDTEXMTX %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_FOG;i < C_COLORMATRIX; i++)
				fprintf(file, "Constant FOG %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_COLORMATRIX;i < C_PENVCONST_END; i++)
				fprintf(file, "Constant COLORMATRIX %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			fclose(file);

			break;
		}

		case 3: // Vertex Shader Constants
		{
			char filename[MAX_PATH];
			sprintf(filename, "%s/Debug/%s/dump_vs_consts.txt", File::GetUserPath(D_DUMP_IDX), globals->unique_id);
			FILE* file = fopen(filename, "w");

			float constants[4*C_VENVCONST_END];
			D3D::dev->GetVertexShaderConstantF(0, constants, C_VENVCONST_END);

			for(unsigned int i = C_POSNORMALMATRIX;i < C_PROJECTION; i++)
				fprintf(file, "Constant POSNORMALMATRIX %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_PROJECTION;i < C_MATERIALS; i++)
				fprintf(file, "Constant PROJECTION %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_MATERIALS;i < C_LIGHTS; i++)
				fprintf(file, "Constant MATERIALS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_LIGHTS;i < C_TEXMATRICES; i++)
				fprintf(file, "Constant LIGHTS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_TEXMATRICES;i < C_TRANSFORMMATRICES; i++)
				fprintf(file, "Constant TEXMATRICES %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_TRANSFORMMATRICES;i < C_NORMALMATRICES; i++)
				fprintf(file, "Constant TRANSFORMMATRICES %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_NORMALMATRICES;i < C_POSTTRANSFORMMATRICES; i++)
				fprintf(file, "Constant NORMALMATRICES %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_POSTTRANSFORMMATRICES;i < C_DEPTHPARAMS; i++)
				fprintf(file, "Constant POSTTRANSFORMMATRICES %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			for(unsigned int i = C_DEPTHPARAMS;i < C_VENVCONST_END; i++)
				fprintf(file, "Constant DEPTHPARAMS %d: %f   %f   %f   %f\n", i, constants[4*i], constants[4*i+1], constants[4*i+2], constants[4*i+3]);

			fclose(file);
			break;
		}

		case 4: // Texture 0
		case 5: // Texture 1
		case 6: // Texture 2
		case 7: // Texture 3
		case 8: // Texture 4
		case 9: // Texture 5
		case 10: // Texture 6
		case 11: // Texture 7
		{
			int stage = m_pDumpList->GetSelection() - 4;
			IDirect3DTexture9* texture;
			D3D::dev->GetTexture(stage, (IDirect3DBaseTexture9**)&texture);
			if(!texture) break;
			char filename[MAX_PATH];
			sprintf(filename, "%s/Debug/%s/dump_tex%d.png", File::GetUserPath(D_DUMP_IDX), globals->unique_id, stage);
			IDirect3DSurface9* surface;
			texture->GetSurfaceLevel(0, &surface);
			HRESULT hr = PD3DXSaveSurfaceToFileA(filename, D3DXIFF_PNG, surface, NULL, NULL);
			if (FAILED(hr)) {
				MessageBoxA(NULL, "Failed to dump texture...", "Error", MB_OK);
			}
			surface->Release();
			texture->Release();
			break;
		}

		case 12: // Frame Buffer
			break;

		case 13: // Vertices
			break;

		case 14: // Vertex Description
			break;

		case 15: // Vertex Matrices
			break;

		case 16: // Statistics
			break;

	}
}

void GFXDebuggerDX9::OnGoButton(wxCommandEvent& event)
{
	DX9DebuggerToPauseAtNext = NOT_PAUSE;
	DX9DebuggerPauseFlag = false;
}

void GFXDebuggerDX9::OnClearScreenButton(wxCommandEvent& event)
{
}

void GFXDebuggerDX9::OnClearTextureCacheButton(wxCommandEvent& event)
{
	TextureCache::Invalidate(false);
}

void GFXDebuggerDX9::OnClearVertexShaderCacheButton(wxCommandEvent& event)
{
}

void GFXDebuggerDX9::OnClearPixelShaderCacheButton(wxCommandEvent& event)
{
}

void UpdateFPSDisplay(const char *text);
extern bool D3D::bFrameInProgress;

static void DX9DebuggerUpdateScreen()
{
	// update screen
	if (D3D::bFrameInProgress)
	{
		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(NULL);

		D3D::dev->StretchRect(g_framebufferManager.GetEFBColorRTSurface(), NULL,
			D3D::GetBackBufferSurface(), NULL,
			D3DTEXF_LINEAR);

		D3D::dev->EndScene();
		D3D::dev->Present(NULL, NULL, NULL, NULL);

		D3D::dev->SetRenderTarget(0, g_framebufferManager.GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(g_framebufferManager.GetEFBDepthRTSurface());
		D3D::dev->BeginScene();
	}
	else
	{
		D3D::dev->EndScene();
		D3D::dev->Present(NULL, NULL, NULL, NULL);
		D3D::dev->BeginScene();
	}
}


void DX9DebuggerCheckAndPause(bool update)
{
	if (DX9DebuggerPauseFlag)
	{
		g_pdebugger->EnableButtons(true);
		while( DX9DebuggerPauseFlag )
		{

			UpdateFPSDisplay("Paused by Video Debugger");

			if (update)	DX9DebuggerUpdateScreen();
			Sleep(5);
		}
		g_pdebugger->EnableButtons(false);
	}
}

void DX9DebuggerToPause(bool update)
{
	DX9DebuggerToPauseAtNext = NOT_PAUSE;
	DX9DebuggerPauseFlag = true;
	DX9DebuggerCheckAndPause(update);
}


void ContinueDX9Debugger()
{
	DX9DebuggerPauseFlag = false;
}

void GFXDebuggerDX9::OnUpdateScreenButton(wxCommandEvent& event)
{
	DX9DebuggerUpdateScreen();
}
