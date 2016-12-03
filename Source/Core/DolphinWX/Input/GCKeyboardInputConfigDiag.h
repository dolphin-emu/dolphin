// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class GCKeyboardInputConfigDialog final : public InputConfigDialog
{
public:
  GCKeyboardInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                              int port_num = 0);
};
