// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/PatchWindow.h"

#include <array>

// clang-format off
#include <wx/bitmap.h>
#include <wx/aui/framemanager.h>
#include <wx/image.h>
#include <wx/listbase.h>
#include <wx/panel.h>
// clang-format on

#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/PatchView.h"
#include "DolphinWX/AuiToolBar.h"
#include "DolphinWX/WxUtils.h"

class PatchBar : public DolphinAuiToolBar
{
public:
  PatchBar(PatchWindow* parent, const wxWindowID id)
      : DolphinAuiToolBar(parent, id, wxDefaultPosition, wxDefaultSize,
                          wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT)
  {
    wxSize bitmap_size = FromDIP(wxSize(24, 24));
    SetToolBitmapSize(bitmap_size);

    static const std::array<const char* const, Num_Bitmaps> image_names{
        "debugger_delete", "debugger_clear", "debugger_breakpoint"};
    for (std::size_t i = 0; i < image_names.size(); ++i)
    {
      m_bitmaps[i] =
          WxUtils::LoadScaledThemeBitmap(image_names[i], this, bitmap_size, wxDefaultSize,
                                         WxUtils::LSI_SCALE_DOWN | WxUtils::LSI_ALIGN_CENTER);
    }

    AddTool(ID_DELETE, _("Delete"), m_bitmaps[Toolbar_Delete]);
    Bind(wxEVT_TOOL, &PatchWindow::OnDelete, parent, ID_DELETE);

    AddTool(ID_CLEAR, _("Clear"), m_bitmaps[Toolbar_Clear]);
    Bind(wxEVT_TOOL, &PatchWindow::OnClear, parent, ID_CLEAR);

    AddTool(ID_ONOFF, _("On/Off"), m_bitmaps[Toolbar_OnOff]);
    Bind(wxEVT_TOOL, &PatchWindow::OnToggleOnOff, parent, ID_ONOFF);
  }

private:
  enum
  {
    Toolbar_Delete,
    Toolbar_Clear,
    Toolbar_OnOff,
    Num_Bitmaps
  };

  enum
  {
    ID_DELETE = 2000,
    ID_CLEAR,
    ID_ONOFF,
  };

  std::array<wxBitmap, Num_Bitmaps> m_bitmaps;
};

PatchWindow::PatchWindow(wxWindow* parent, wxWindowID id, const wxString& title,
                         const wxPoint& position, const wxSize& size, long style)
    : wxPanel(parent, id, position, size, style, title)
{
  m_mgr.SetManagedWindow(this);
  m_mgr.SetFlags(wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

  m_patch_view = new PatchView(this, wxID_ANY);

  m_mgr.AddPane(new PatchBar(this, wxID_ANY), wxAuiPaneInfo()
                                                  .ToolbarPane()
                                                  .Top()
                                                  .LeftDockable(true)
                                                  .RightDockable(true)
                                                  .BottomDockable(false)
                                                  .Floatable(false));
  m_mgr.AddPane(m_patch_view, wxAuiPaneInfo().CenterPane());
  m_mgr.Update();
}

PatchWindow::~PatchWindow()
{
  m_mgr.UnInit();
}

void PatchWindow::NotifyUpdate()
{
  m_patch_view->Repopulate();
}

void PatchWindow::OnDelete(wxCommandEvent&)
{
  m_patch_view->DeleteCurrentSelection();

  NotifyUpdate();
}

void PatchWindow::OnClear(wxCommandEvent&)
{
  PowerPC::debug_interface.ClearPatches();

  NotifyUpdate();
}

void PatchWindow::OnToggleOnOff(wxCommandEvent&)
{
  m_patch_view->ToggleCurrentSelection();

  NotifyUpdate();
}
