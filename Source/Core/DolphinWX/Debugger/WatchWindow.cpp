// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>

// clang-format off
#include <wx/bitmap.h>
#include <wx/panel.h>
// clang-format on

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/WatchView.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/AuiToolBar.h"
#include "DolphinWX/WxUtils.h"

class CWatchToolbar : public DolphinAuiToolBar
{
public:
  CWatchToolbar(CWatchWindow* parent, const wxWindowID id)
      : DolphinAuiToolBar(parent, id, wxDefaultPosition, wxDefaultSize,
                          wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT)
  {
    InitialiseThemedBitmaps();

    AddTool(ID_LOAD, _("Load"), m_Bitmaps[Toolbar_Load]);
    Bind(wxEVT_TOOL, &CWatchWindow::Event_LoadAll, parent, ID_LOAD);

    AddTool(ID_SAVE, _("Save"), m_Bitmaps[Toolbar_Save]);
    Bind(wxEVT_TOOL, &CWatchWindow::Event_SaveAll, parent, ID_SAVE);
  }

  void ReloadBitmaps()
  {
    Freeze();

    InitialiseThemedBitmaps();
    for (int i = 0; i < ID_NUM; ++i)
      SetToolBitmap(i, m_Bitmaps[i]);

    Thaw();
  }

private:
  enum
  {
    Toolbar_Load,
    Toolbar_Save,
    Num_Bitmaps
  };

  enum
  {
    ID_LOAD = 0,
    ID_SAVE,
    ID_NUM
  };

  void InitialiseThemedBitmaps()
  {
    wxSize bitmap_size = FromDIP(wxSize(24, 24));
    SetToolBitmapSize(bitmap_size);

    static const std::array<const char* const, Num_Bitmaps> m_image_names{
        {"debugger_load", "debugger_save"}};

    for (std::size_t i = 0; i < m_image_names.size(); ++i)
    {
      m_Bitmaps[i] =
          WxUtils::LoadScaledThemeBitmap(m_image_names[i], this, bitmap_size, wxDefaultSize,
                                         WxUtils::LSI_SCALE_DOWN | WxUtils::LSI_ALIGN_CENTER);
    }
  }

  wxBitmap m_Bitmaps[Num_Bitmaps];
};

CWatchWindow::CWatchWindow(wxWindow* parent, wxWindowID id, const wxPoint& position,
                           const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, position, size, style, name), m_GPRGridView(nullptr)
{
  m_mgr.SetManagedWindow(this);
  m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

  m_GPRGridView = new CWatchView(this);

  m_watch_toolbar = new CWatchToolbar(this, wxID_ANY);

  m_mgr.AddPane(m_watch_toolbar, wxAuiPaneInfo()
                                     .ToolbarPane()
                                     .Top()
                                     .LeftDockable(true)
                                     .RightDockable(true)
                                     .BottomDockable(false)
                                     .Floatable(false));
  m_mgr.AddPane(m_GPRGridView, wxAuiPaneInfo().CenterPane());
  m_mgr.Update();
}

CWatchWindow::~CWatchWindow()
{
  m_mgr.UnInit();
}

void CWatchWindow::NotifyUpdate()
{
  if (m_GPRGridView != nullptr)
    m_GPRGridView->Repopulate();
}

void CWatchWindow::Event_SaveAll(wxCommandEvent& WXUNUSED(event))
{
  SaveAll();
}

void CWatchWindow::SaveAll()
{
  IniFile ini;
  ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
           false);
  ini.SetLines("Watches", PowerPC::watches.GetStrings());
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini");
}

void CWatchWindow::Event_LoadAll(wxCommandEvent& WXUNUSED(event))
{
  LoadAll();
}

void CWatchWindow::LoadAll()
{
  IniFile ini;
  Watches::TWatchesStr watches;

  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  if (ini.GetLines("Watches", &watches, false))
  {
    PowerPC::watches.Clear();
    PowerPC::watches.AddFromStrings(watches);
  }

  NotifyUpdate();
}

void CWatchWindow::ReloadBitmaps()
{
  m_watch_toolbar->ReloadBitmaps();
}
