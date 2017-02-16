// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/MainToolBar.h"

#include <array>
#include <utility>

#include "Core/Core.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_RELOAD_TOOLBAR_BITMAPS, wxCommandEvent);

MainToolBar::MainToolBar(ToolBarType type, wxWindow* parent, wxWindowID id, const wxPoint& pos,
                         const wxSize& size, long style, const wxString& name)
    : wxToolBar{parent, id, pos, size, style, name}, m_type{type}
{
  wxToolBar::SetToolBitmapSize(FromDIP(wxSize{32, 32}));
  InitializeBitmaps();
  AddToolBarButtons();

  BindEvents();
}

void MainToolBar::BindEvents()
{
  Bind(DOLPHIN_EVT_RELOAD_TOOLBAR_BITMAPS, &MainToolBar::OnReloadBitmaps, this);

  BindMainButtonEvents();

  if (m_type == ToolBarType::Debug)
    BindDebuggerButtonEvents();
}

void MainToolBar::BindMainButtonEvents()
{
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning, wxID_OPEN);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning, wxID_REFRESH);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunningOrPaused, IDM_STOP);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunningOrPaused, IDM_TOGGLE_FULLSCREEN);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunningOrPaused, IDM_SCREENSHOT);
}

void MainToolBar::BindDebuggerButtonEvents()
{
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEP);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEPOVER);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEPOUT);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_SKIP);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_SETPC);
}

void MainToolBar::OnReloadBitmaps(wxCommandEvent& WXUNUSED(event))
{
  Freeze();

  m_icon_bitmaps.clear();
  InitializeBitmaps();
  ApplyThemeBitmaps();

  if (m_type == ToolBarType::Debug)
    ApplyDebuggerBitmaps();

  Thaw();
}

void MainToolBar::Refresh(bool erase_background, const wxRect* rect)
{
  wxToolBar::Refresh(erase_background, rect);

  RefreshPlayButton();
}

void MainToolBar::InitializeBitmaps()
{
  InitializeThemeBitmaps();

  if (m_type == ToolBarType::Debug)
    InitializeDebuggerBitmaps();
}

void MainToolBar::InitializeThemeBitmaps()
{
  m_icon_bitmaps.insert({{TOOLBAR_FILEOPEN, CreateBitmap("open")},
                         {TOOLBAR_REFRESH, CreateBitmap("refresh")},
                         {TOOLBAR_PLAY, CreateBitmap("play")},
                         {TOOLBAR_STOP, CreateBitmap("stop")},
                         {TOOLBAR_PAUSE, CreateBitmap("pause")},
                         {TOOLBAR_SCREENSHOT, CreateBitmap("screenshot")},
                         {TOOLBAR_FULLSCREEN, CreateBitmap("fullscreen")},
                         {TOOLBAR_CONFIGMAIN, CreateBitmap("config")},
                         {TOOLBAR_CONFIGGFX, CreateBitmap("graphics")},
                         {TOOLBAR_CONTROLLER, CreateBitmap("classic")},
                         {TOOLBAR_CONFIGVR, CreateBitmap("vr")}});
}

void MainToolBar::InitializeDebuggerBitmaps()
{
  m_icon_bitmaps.insert(
      {{TOOLBAR_DEBUG_STEP, CreateDebuggerBitmap("toolbar_debugger_step")},
       {TOOLBAR_DEBUG_STEPOVER, CreateDebuggerBitmap("toolbar_debugger_step_over")},
       {TOOLBAR_DEBUG_STEPOUT, CreateDebuggerBitmap("toolbar_debugger_step_out")},
       {TOOLBAR_DEBUG_SKIP, CreateDebuggerBitmap("toolbar_debugger_skip")},
       {TOOLBAR_DEBUG_GOTOPC, CreateDebuggerBitmap("toolbar_debugger_goto_pc")},
       {TOOLBAR_DEBUG_SETPC, CreateDebuggerBitmap("toolbar_debugger_set_pc")}});
}

wxBitmap MainToolBar::CreateBitmap(const std::string& name) const
{
  return WxUtils::LoadScaledThemeBitmap(name, this, GetToolBitmapSize());
}

wxBitmap MainToolBar::CreateDebuggerBitmap(const std::string& name) const
{
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif
  constexpr auto scale_flags = WxUtils::LSI_SCALE_DOWN | WxUtils::LSI_ALIGN_CENTER;

  return WxUtils::LoadScaledResourceBitmap(name, this, GetToolBitmapSize(), wxDefaultSize,
                                           scale_flags);
}

void MainToolBar::ApplyThemeBitmaps()
{
  constexpr std::array<std::pair<int, ToolBarBitmapID>, 9> bitmap_entries{
      {{wxID_OPEN, TOOLBAR_FILEOPEN},
       {wxID_REFRESH, TOOLBAR_REFRESH},
       {IDM_STOP, TOOLBAR_STOP},
       {IDM_TOGGLE_FULLSCREEN, TOOLBAR_FULLSCREEN},
       {IDM_SCREENSHOT, TOOLBAR_SCREENSHOT},
       {wxID_PREFERENCES, TOOLBAR_CONFIGMAIN},
       {IDM_CONFIG_GFX_BACKEND, TOOLBAR_CONFIGGFX},
       {IDM_CONFIG_CONTROLLERS, TOOLBAR_CONTROLLER},
       {IDM_CONFIG_VR, TOOLBAR_CONFIGVR}}};

  for (const auto& entry : bitmap_entries)
    ApplyBitmap(entry.first, entry.second);

  // Separate, as the play button is dual-state and doesn't have a fixed bitmap.
  RefreshPlayButton();
}

void MainToolBar::ApplyDebuggerBitmaps()
{
  constexpr std::array<std::pair<int, ToolBarBitmapID>, 6> bitmap_entries{
      {{IDM_STEP, TOOLBAR_DEBUG_STEP},
       {IDM_STEPOVER, TOOLBAR_DEBUG_STEPOVER},
       {IDM_STEPOUT, TOOLBAR_DEBUG_STEPOUT},
       {IDM_SKIP, TOOLBAR_DEBUG_SKIP},
       {IDM_GOTOPC, TOOLBAR_DEBUG_GOTOPC},
       {IDM_SETPC, TOOLBAR_DEBUG_SETPC}}};

  for (const auto& entry : bitmap_entries)
    ApplyBitmap(entry.first, entry.second);
}

void MainToolBar::ApplyBitmap(int tool_id, ToolBarBitmapID bitmap_id)
{
  const auto& bitmap = m_icon_bitmaps[bitmap_id];

  SetToolDisabledBitmap(tool_id, WxUtils::CreateDisabledButtonBitmap(bitmap));
  SetToolNormalBitmap(tool_id, bitmap);
}

void MainToolBar::AddToolBarButtons()
{
  if (m_type == ToolBarType::Debug)
  {
    AddDebuggerToolBarButtons();
    AddSeparator();
  }

  AddMainToolBarButtons();
}

void MainToolBar::AddMainToolBarButtons()
{
  AddToolBarButton(wxID_OPEN, TOOLBAR_FILEOPEN, _("Open"), _("Open file..."));
  AddToolBarButton(wxID_REFRESH, TOOLBAR_REFRESH, _("Refresh"), _("Refresh game list"));
  AddSeparator();
  AddToolBarButton(IDM_PLAY, TOOLBAR_PLAY, _("Play"), _("Play"));
  AddToolBarButton(IDM_STOP, TOOLBAR_STOP, _("Stop"), _("Stop"));
  AddToolBarButton(IDM_TOGGLE_FULLSCREEN, TOOLBAR_FULLSCREEN, _("FullScr"), _("Toggle fullscreen"));
  AddToolBarButton(IDM_SCREENSHOT, TOOLBAR_SCREENSHOT, _("ScrShot"), _("Take screenshot"));
  AddSeparator();
  AddToolBarButton(wxID_PREFERENCES, TOOLBAR_CONFIGMAIN, _("Config"), _("Configure..."));
  AddToolBarButton(IDM_CONFIG_GFX_BACKEND, TOOLBAR_CONFIGGFX, _("Graphics"),
                   _("Graphics settings"));
  AddToolBarButton(IDM_CONFIG_CONTROLLERS, TOOLBAR_CONTROLLER, _("Controllers"),
                   _("Controller settings"));
  AddToolBarButton(IDM_CONFIG_VR, TOOLBAR_CONFIGVR, _("VR"), _("VR Settings"));
}

void MainToolBar::AddDebuggerToolBarButtons()
{
  // i18n: Here, "Step" is a verb. This function is used for
  // going through code step by step.
  AddToolBarButton(IDM_STEP, TOOLBAR_DEBUG_STEP, _("Step"), _("Step into the next instruction"));
  // i18n: Here, "Step" is a verb. This function is used for
  // going through code step by step.
  AddToolBarButton(IDM_STEPOVER, TOOLBAR_DEBUG_STEPOVER, _("Step Over"),
                   _("Step over the next instruction"));
  // i18n: Here, "Step" is a verb. This function is used for
  // going through code step by step.
  AddToolBarButton(IDM_STEPOUT, TOOLBAR_DEBUG_STEPOUT, _("Step Out"),
                   _("Step out of the current function"));
  AddToolBarButton(IDM_SKIP, TOOLBAR_DEBUG_SKIP, _("Skip"),
                   _("Skips the next instruction completely"));
  AddSeparator();
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  AddToolBarButton(IDM_GOTOPC, TOOLBAR_DEBUG_GOTOPC, _("Show PC"),
                   _("Go to the current instruction"));
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  AddToolBarButton(IDM_SETPC, TOOLBAR_DEBUG_SETPC, _("Set PC"), _("Set the current instruction"));
}

void MainToolBar::AddToolBarButton(int tool_id, ToolBarBitmapID bitmap_id, const wxString& label,
                                   const wxString& short_help)
{
  WxUtils::AddToolbarButton(this, tool_id, label, m_icon_bitmaps[bitmap_id], short_help);
}

void MainToolBar::RefreshPlayButton()
{
  ToolBarBitmapID bitmap_id;
  wxString label;

  if (Core::GetState() == Core::State::Running)
  {
    bitmap_id = TOOLBAR_PAUSE;
    label = _("Pause");
  }
  else
  {
    bitmap_id = TOOLBAR_PLAY;
    label = _("Play");
  }

  FindById(IDM_PLAY)->SetLabel(label);
  SetToolShortHelp(IDM_PLAY, label);
  ApplyBitmap(IDM_PLAY, bitmap_id);
}
