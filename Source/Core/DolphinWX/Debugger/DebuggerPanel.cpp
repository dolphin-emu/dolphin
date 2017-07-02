// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <string>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Debugger/DebuggerPanel.h"
#include "DolphinWX/WxUtils.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/TextureCacheBase.h"

GFXDebuggerPanel::GFXDebuggerPanel(wxWindow* parent, wxWindowID id, const wxPoint& position,
                                   const wxSize& size, long style, const wxString& title)
    : wxPanel(parent, id, position, size, style, title)
{
  g_pdebugger = this;

  CreateGUIControls();
}

GFXDebuggerPanel::~GFXDebuggerPanel()
{
  g_pdebugger = nullptr;
  GFXDebuggerPauseFlag = false;
}

struct PauseEventMap
{
  PauseEvent event;
  const wxString ListStr;
};

static PauseEventMap* pauseEventMap;

void GFXDebuggerPanel::CreateGUIControls()
{
  static PauseEventMap map[] = {{NEXT_FRAME, _("Frame")},
                                {NEXT_FLUSH, _("Flush")},

                                {NEXT_PIXEL_SHADER_CHANGE, _("Pixel Shader")},
                                {NEXT_VERTEX_SHADER_CHANGE, _("Vertex Shader")},
                                {NEXT_TEXTURE_CHANGE, _("Texture")},
                                {NEXT_NEW_TEXTURE, _("New Texture")},

                                {NEXT_XFB_CMD, _("XFB Cmd")},
                                {NEXT_EFB_CMD, _("EFB Cmd")},

                                {NEXT_MATRIX_CMD, _("Matrix Cmd")},
                                {NEXT_VERTEX_CMD, _("Vertex Cmd")},
                                {NEXT_TEXTURE_CMD, _("Texture Cmd")},
                                {NEXT_LIGHT_CMD, _("Light Cmd")},
                                {NEXT_FOG_CMD, _("Fog Cmd")},

                                {NEXT_SET_TLUT, _("TLUT Cmd")},

                                {NEXT_ERROR, _("Error")}};
  pauseEventMap = map;
  static constexpr int numPauseEventMap = ArraySize(map);

  const int space3 = FromDIP(3);

  m_pButtonPause = new wxButton(this, wxID_ANY, _("Pause"));
  m_pButtonPause->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnPauseButton, this);

  m_pButtonPauseAtNext = new wxButton(this, wxID_ANY, _("Pause After"));
  m_pButtonPauseAtNext->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnPauseAtNextButton, this);

  m_pButtonPauseAtNextFrame = new wxButton(this, wxID_ANY, _("Go to Next Frame"));
  m_pButtonPauseAtNextFrame->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnPauseAtNextFrameButton, this);

  m_pButtonCont = new wxButton(this, wxID_ANY, _("Continue"));
  m_pButtonCont->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnContButton, this);

  m_pCount = new wxTextCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxDefaultSize, wxTE_RIGHT,
                            wxDefaultValidator, _("Count"));
  m_pCount->SetMinSize(WxUtils::GetTextWidgetMinSize(m_pCount, 10000));

  m_pPauseAtList = new wxChoice(this, wxID_ANY);
  for (int i = 0; i < numPauseEventMap; i++)
  {
    m_pPauseAtList->Append(pauseEventMap[i].ListStr);
  }
  m_pPauseAtList->SetSelection(0);

  m_pButtonDump = new wxButton(this, wxID_ANY, _("Dump"));
  m_pButtonDump->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnDumpButton, this);

  m_pButtonUpdateScreen = new wxButton(this, wxID_ANY, _("Update Screen"));
  m_pButtonUpdateScreen->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnUpdateScreenButton, this);

  m_pButtonClearScreen = new wxButton(this, wxID_ANY, _("Clear Screen"));
  m_pButtonClearScreen->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnClearScreenButton, this);

  m_pButtonClearTextureCache = new wxButton(this, wxID_ANY, _("Clear Textures"));
  m_pButtonClearTextureCache->Bind(wxEVT_BUTTON, &GFXDebuggerPanel::OnClearTextureCacheButton,
                                   this);

  const wxString clear_vertex_shaders = _("Clear Vertex Shaders");
  m_pButtonClearVertexShaderCache =
      new wxButton(this, wxID_ANY, clear_vertex_shaders, wxDefaultPosition, wxDefaultSize, 0,
                   wxDefaultValidator, clear_vertex_shaders);
  m_pButtonClearVertexShaderCache->Bind(wxEVT_BUTTON,
                                        &GFXDebuggerPanel::OnClearVertexShaderCacheButton, this);

  const wxString clear_pixel_shaders = _("Clear Pixel Shaders");
  m_pButtonClearPixelShaderCache =
      new wxButton(this, wxID_ANY, clear_pixel_shaders, wxDefaultPosition, wxDefaultSize, 0,
                   wxDefaultValidator, clear_pixel_shaders);
  m_pButtonClearPixelShaderCache->Bind(wxEVT_BUTTON,
                                       &GFXDebuggerPanel::OnClearPixelShaderCacheButton, this);

  m_pDumpList = new wxChoice(this, wxID_ANY);
  m_pDumpList->Insert(_("Pixel Shader"), 0);
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

  wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer* const pPauseAtNextSzr = new wxBoxSizer(wxHORIZONTAL);
  pPauseAtNextSzr->Add(m_pCount, 0, wxALIGN_CENTER_VERTICAL);
  pPauseAtNextSzr->Add(m_pPauseAtList, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space3);

  wxFlexGridSizer* const flow_szr = new wxFlexGridSizer(2, space3, space3);
  flow_szr->Add(m_pButtonPause, 0, wxEXPAND);
  flow_szr->AddSpacer(1);
  flow_szr->Add(m_pButtonPauseAtNext, 0, wxEXPAND);
  flow_szr->Add(pPauseAtNextSzr, 0, wxEXPAND);
  flow_szr->Add(m_pButtonPauseAtNextFrame, 0, wxEXPAND);
  flow_szr->AddSpacer(1);
  flow_szr->Add(m_pButtonCont, 0, wxEXPAND);
  flow_szr->AddSpacer(1);

  wxStaticBoxSizer* const pFlowCtrlBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Flow Control"));
  pFlowCtrlBox->Add(flow_szr, 1, wxEXPAND);

  wxBoxSizer* const pDumpSzr = new wxBoxSizer(wxHORIZONTAL);
  pDumpSzr->Add(m_pButtonDump, 0, wxALIGN_CENTER_VERTICAL);
  pDumpSzr->Add(m_pDumpList, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space3);

  wxGridSizer* const pDbgGrid = new wxGridSizer(2, space3, space3);
  pDbgGrid->Add(m_pButtonUpdateScreen, 0, wxEXPAND);
  pDbgGrid->Add(m_pButtonClearScreen, 0, wxEXPAND);
  pDbgGrid->Add(m_pButtonClearTextureCache, 0, wxEXPAND);
  pDbgGrid->Add(m_pButtonClearVertexShaderCache, 0, wxEXPAND);
  pDbgGrid->Add(m_pButtonClearPixelShaderCache, 0, wxEXPAND);

  wxStaticBoxSizer* const pDebugBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Debugging"));
  pDebugBox->Add(pDumpSzr, 0, wxEXPAND);
  pDebugBox->Add(pDbgGrid, 1, wxTOP, space3);

  sMain->Add(pFlowCtrlBox);
  sMain->Add(pDebugBox);
  SetSizerAndFit(sMain);

  OnContinue();
}

void GFXDebuggerPanel::OnPause()
{
  m_pButtonDump->Enable();
  m_pDumpList->Enable();
  m_pButtonUpdateScreen->Enable();
  m_pButtonClearScreen->Enable();
  m_pButtonClearTextureCache->Enable();
  m_pButtonClearVertexShaderCache->Enable();
  m_pButtonClearPixelShaderCache->Enable();
}

void GFXDebuggerPanel::OnContinue()
{
  m_pButtonDump->Disable();
  m_pDumpList->Disable();
  m_pButtonUpdateScreen->Disable();
  m_pButtonClearScreen->Disable();
  m_pButtonClearTextureCache->Disable();
  m_pButtonClearVertexShaderCache->Disable();
  m_pButtonClearPixelShaderCache->Disable();
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
  std::string dump_path =
      File::GetUserPath(D_DUMP_IDX) + "Debug/" + SConfig::GetInstance().GetGameID() + "/";
  if (!File::CreateFullPath(dump_path))
    return;

  switch (m_pDumpList->GetSelection())
  {
  case 0:  // Pixel Shader
    DumpPixelShader(dump_path);
    break;

  case 1:  // Vertex Shader
    DumpVertexShader(dump_path);
    break;

  case 2:  // Pixel Shader Constants
    DumpPixelShaderConstants(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 3:  // Vertex Shader Constants
    DumpVertexShaderConstants(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 4:  // Textures
    DumpTextures(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 5:  // Frame Buffer
    DumpFrameBuffer(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 6:  // Geometry
    DumpGeometry(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 7:  // Vertex Description
    DumpVertexDecl(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 8:  // Vertex Matrices
    DumpMatrices(dump_path);
    WxUtils::ShowErrorDialog(_("Not implemented"));
    break;

  case 9:  // Statistics
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
  g_texture_cache->Invalidate();
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
