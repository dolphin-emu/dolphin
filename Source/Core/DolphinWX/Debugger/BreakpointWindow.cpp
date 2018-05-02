// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/BreakpointWindow.h"

#include <array>

// clang-format off
#include <wx/bitmap.h>
#include <wx/aui/framemanager.h>
#include <wx/image.h>
#include <wx/listbase.h>
#include <wx/panel.h>
// clang-format on

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointDlg.h"
#include "DolphinWX/Debugger/BreakpointView.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/MemoryCheckDlg.h"
#include "DolphinWX/AuiToolBar.h"
#include "DolphinWX/WxUtils.h"

class CBreakPointBar : public DolphinAuiToolBar
{
public:
  CBreakPointBar(CBreakPointWindow* parent, const wxWindowID id)
      : DolphinAuiToolBar(parent, id, wxDefaultPosition, wxDefaultSize,
                          wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT)
  {
    InitializeThemedBitmaps();

    AddTool(ID_DELETE, _("Delete"), m_Bitmaps[Toolbar_Delete]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::OnDelete, parent, ID_DELETE);

    AddTool(ID_CLEAR, _("Clear"), m_Bitmaps[Toolbar_Clear]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::OnClear, parent, ID_CLEAR);

    AddTool(ID_ADDBP, "+BP", m_Bitmaps[Toolbar_Add_BP]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::OnAddBreakPoint, parent, ID_ADDBP);

    AddTool(ID_ADDMC, "+MBP", m_Bitmaps[Toolbar_Add_MC]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::OnAddMemoryCheck, parent, ID_ADDMC);

    AddTool(ID_ENABLE_DISABLE_BP, _("On/Off"), m_Bitmaps[Toolbar_Enable_Disable_BP]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::OnEnableDisableBreakPoint, parent, ID_ENABLE_DISABLE_BP);

    AddTool(ID_LOAD, _("Load"), m_Bitmaps[Toolbar_Load]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::Event_LoadAll, parent, ID_LOAD);

    AddTool(ID_SAVE, _("Save"), m_Bitmaps[Toolbar_Save]);
    Bind(wxEVT_TOOL, &CBreakPointWindow::Event_SaveAll, parent, ID_SAVE);
  }

  void ReloadBitmaps()
  {
    Freeze();

    InitializeThemedBitmaps();
    for (int i = 0; i < ID_NUM; ++i)
      SetToolBitmap(i, m_Bitmaps[i]);

    Thaw();
  }

private:
  enum
  {
    Toolbar_Delete = 0,
    Toolbar_Clear,
    Toolbar_Add_BP,
    Toolbar_Add_MC,
    Toolbar_Enable_Disable_BP,
    Toolbar_Load,
    Toolbar_Save,
    Num_Bitmaps
  };

  enum
  {
    ID_DELETE = 0,
    ID_CLEAR,
    ID_ADDBP,
    ID_ADDMC,
    ID_ENABLE_DISABLE_BP,
    ID_LOAD,
    ID_SAVE,
    ID_NUM
  };

  void InitializeThemedBitmaps()
  {
    wxSize bitmap_size = FromDIP(wxSize(24, 24));
    SetToolBitmapSize(bitmap_size);

    static const std::array<const char* const, Num_Bitmaps> m_image_names{
        {"debugger_delete", "debugger_clear", "debugger_add_breakpoint", "debugger_add_memorycheck",
         "debugger_breakpoint", "debugger_load", "debugger_save"}};

    for (std::size_t i = 0; i < m_image_names.size(); ++i)
    {
      m_Bitmaps[i] =
          WxUtils::LoadScaledThemeBitmap(m_image_names[i], this, bitmap_size, wxDefaultSize,
                                         WxUtils::LSI_SCALE_DOWN | WxUtils::LSI_ALIGN_CENTER);
    }
  }

  wxBitmap m_Bitmaps[Num_Bitmaps];
};

CBreakPointWindow::CBreakPointWindow(CCodeWindow* _pCodeWindow, wxWindow* parent, wxWindowID id,
                                     const wxString& title, const wxPoint& position,
                                     const wxSize& size, long style)
    : wxPanel(parent, id, position, size, style, title), m_pCodeWindow(_pCodeWindow)
{
  m_mgr.SetManagedWindow(this);
  m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

  m_BreakPointListView = new CBreakPointView(this, wxID_ANY);
  m_BreakPointListView->Bind(wxEVT_LIST_ITEM_SELECTED, &CBreakPointWindow::OnSelectBP, this);

  m_breakpointBar = new CBreakPointBar(this, wxID_ANY);

  m_mgr.AddPane(m_breakpointBar, wxAuiPaneInfo()
                                     .ToolbarPane()
                                     .Top()
                                     .LeftDockable(true)
                                     .RightDockable(true)
                                     .BottomDockable(false)
                                     .Floatable(false));
  m_mgr.AddPane(m_BreakPointListView, wxAuiPaneInfo().CenterPane());
  m_mgr.Update();
}

CBreakPointWindow::~CBreakPointWindow()
{
  m_mgr.UnInit();
}

void CBreakPointWindow::NotifyUpdate()
{
  m_BreakPointListView->Repopulate();
}

void CBreakPointWindow::ReloadBitmaps()
{
  m_breakpointBar->ReloadBitmaps();
}

void CBreakPointWindow::OnDelete(wxCommandEvent& WXUNUSED(event))
{
  m_BreakPointListView->DeleteCurrentSelection();
}

// jump to begin addr
void CBreakPointWindow::OnSelectBP(wxListEvent& event)
{
  long Index = event.GetIndex();
  if (Index >= 0)
  {
    u32 Address = (u32)m_BreakPointListView->GetItemData(Index);
    if (m_pCodeWindow)
      m_pCodeWindow->JumpToAddress(Address);
  }
}

// Clear all breakpoints and memchecks
void CBreakPointWindow::OnClear(wxCommandEvent& WXUNUSED(event))
{
  PowerPC::debug_interface.ClearBreakpoints();
  PowerPC::debug_interface.ClearAllMemChecks();

  NotifyUpdate();
}

void CBreakPointWindow::OnAddBreakPoint(wxCommandEvent& WXUNUSED(event))
{
  BreakPointDlg bpDlg(this);
  if (bpDlg.ShowModal() == wxID_OK)
  {
    NotifyUpdate();
  }
}

void CBreakPointWindow::OnAddMemoryCheck(wxCommandEvent& WXUNUSED(event))
{
  MemoryCheckDlg memDlg(this);
  if (memDlg.ShowModal() == wxID_OK)
  {
    NotifyUpdate();
  }
}

void CBreakPointWindow::OnEnableDisableBreakPoint(wxCommandEvent& WXUNUSED(event))
{
  long index = m_BreakPointListView->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (index >= 0)
  {
    u32 address = static_cast<u32>(m_BreakPointListView->GetItemData(index));
    if (PowerPC::debug_interface.HasBreakpoint(address, Common::Debug::BreakPoint::State::Disabled))
    {
      PowerPC::debug_interface.EnableBreakpointAt(address);
    }
    else
    {
      PowerPC::debug_interface.DisableBreakpointAt(address);
    }
    NotifyUpdate();
  }
}

void CBreakPointWindow::Event_SaveAll(wxCommandEvent& WXUNUSED(event))
{
  SaveAll();
}

void CBreakPointWindow::SaveAll()
{
  // simply dump all to bp/mc files in a way we can read again
  IniFile ini;
  ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
           false);
  ini.SetLines("BreakPoints", PowerPC::debug_interface.SaveBreakpointsToStrings());
  ini.SetLines("MemoryBreakPoints", PowerPC::memchecks.GetStrings());
  ini.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini");
}

void CBreakPointWindow::Event_LoadAll(wxCommandEvent& WXUNUSED(event))
{
  LoadAll();
  return;
}

void CBreakPointWindow::LoadAll()
{
  IniFile ini;
  std::vector<std::string> newbps;
  MemChecks::TMemChecksStr newmcs;

  if (!ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + SConfig::GetInstance().GetGameID() + ".ini",
                false))
  {
    return;
  }

  if (ini.GetLines("BreakPoints", &newbps, false))
  {
    PowerPC::debug_interface.ClearBreakpoints();
    PowerPC::debug_interface.LoadBreakpointsFromStrings(newbps);
  }

  if (ini.GetLines("MemoryBreakPoints", &newmcs, false))
  {
    PowerPC::memchecks.Clear();
    PowerPC::memchecks.AddFromStrings(newmcs);
  }

  NotifyUpdate();
}
