// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <wx/bitmap.h>
#include <wx/toolbar.h>

wxDECLARE_EVENT(DOLPHIN_EVT_RELOAD_TOOLBAR_BITMAPS, wxCommandEvent);

class MainToolBar final : public wxToolBar
{
public:
  enum class ToolBarType
  {
    Regular,
    Debug
  };

  MainToolBar(ToolBarType type, wxWindow* parent, wxWindowID id,
              const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
              long style = wxTB_HORIZONTAL, const wxString& name = wxToolBarNameStr);

  void Refresh(bool erase_background, const wxRect* rect = nullptr) override;

private:
  enum ToolBarBitmapID : int
  {
    TOOLBAR_DEBUG_STEP,
    TOOLBAR_DEBUG_STEPOVER,
    TOOLBAR_DEBUG_STEPOUT,
    TOOLBAR_DEBUG_SKIP,
    TOOLBAR_DEBUG_GOTOPC,
    TOOLBAR_DEBUG_SETPC,

    TOOLBAR_FILEOPEN,
    TOOLBAR_REFRESH,
    TOOLBAR_PLAY,
    TOOLBAR_STOP,
    TOOLBAR_PAUSE,
    TOOLBAR_SCREENSHOT,
    TOOLBAR_FULLSCREEN,
    TOOLBAR_CONFIGMAIN,
    TOOLBAR_CONFIGGFX,
    TOOLBAR_CONTROLLER,
  };

  void BindEvents();
  void BindMainButtonEvents();
  void BindDebuggerButtonEvents();

  void OnReloadBitmaps(wxCommandEvent&);

  void InitializeBitmaps();
  void InitializeThemeBitmaps();
  void InitializeDebuggerBitmaps();

  wxBitmap CreateBitmap(const std::string& name) const;
  wxBitmap CreateDebuggerBitmap(const std::string& name) const;

  void ApplyThemeBitmaps();
  void ApplyDebuggerBitmaps();
  void ApplyBitmap(int tool_id, ToolBarBitmapID bitmap_id);

  void AddToolBarButtons();
  void AddMainToolBarButtons();
  void AddDebuggerToolBarButtons();
  void AddToolBarButton(int tool_id, ToolBarBitmapID bitmap_id, const wxString& label,
                        const wxString& short_help = wxEmptyString);

  void RefreshPlayButton();

  ToolBarType m_type{};
  std::unordered_map<ToolBarBitmapID, wxBitmap, std::hash<int>> m_icon_bitmaps;
};
