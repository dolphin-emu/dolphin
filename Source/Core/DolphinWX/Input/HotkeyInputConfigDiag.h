// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class wxBoxSizer;
class wxNotebook;
class wxPanel;

class HotkeyInputConfigDialog final : public InputConfigDialog
{
public:
  HotkeyInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                          bool using_debugger, int port_num = 0);

private:
  wxBoxSizer* CreateMainSizer();
  wxBoxSizer* CreateDeviceRelatedSizer();
  wxBoxSizer* CreateDeviceProfileSizer();

  void InitializeNotebook();
  wxPanel* CreateGeneralPanel();
  wxPanel* CreateTASToolsPanel();
  wxPanel* CreateDebuggingPanel();
  wxPanel* CreateWiiPanel();
  wxPanel* CreateGraphicsPanel();
  wxPanel* CreateStereoscopic3DPanel();
  wxPanel* CreateSaveAndLoadStatePanel();
  wxPanel* CreateOtherStateManagementPanel();

  wxNotebook* m_notebook;
  bool m_using_debugger;
};
