// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class HotkeyInputConfigDialog final : public InputConfigDialog
{
public:
  HotkeyInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                          bool using_debugger, int port_num = 0);
};
