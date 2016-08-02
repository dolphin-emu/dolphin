// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/RegisterView.h"
#include "DolphinWX/Debugger/RegisterWindow.h"

CRegisterWindow::CRegisterWindow(wxWindow* parent, wxWindowID id, const wxPoint& position,
                                 const wxSize& size, long style, const wxString& name)
    : wxPanel(parent, id, position, size, style, name), m_GPRGridView(nullptr)
{
  CreateGUIControls();
}

void CRegisterWindow::CreateGUIControls()
{
  wxBoxSizer* sGrid = new wxBoxSizer(wxVERTICAL);
  m_GPRGridView = new CRegisterView(this);
  sGrid->Add(m_GPRGridView, 1, wxEXPAND);
  SetSizer(sGrid);

  NotifyUpdate();
}

void CRegisterWindow::NotifyUpdate()
{
  if (m_GPRGridView != nullptr)
    m_GPRGridView->Repopulate();
}
