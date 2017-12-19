// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/listctrl.h>

class PatchView : public wxListCtrl
{
public:
  PatchView(wxWindow* parent, const wxWindowID id);

  void Repopulate();
  void ToggleCurrentSelection();
  void DeleteCurrentSelection();
};
