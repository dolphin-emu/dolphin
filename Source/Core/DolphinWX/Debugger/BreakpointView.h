// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/listctrl.h>

class CBreakPointView : public wxListCtrl
{
public:
  CBreakPointView(wxWindow* parent, const wxWindowID id);

  void Repopulate();
  void DeleteCurrentSelection();
};
