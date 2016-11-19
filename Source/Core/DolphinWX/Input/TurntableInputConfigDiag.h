// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class TurntableInputConfigDialog final : public InputConfigDialog
{
public:
  TurntableInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                             int port_num = 0);
};
