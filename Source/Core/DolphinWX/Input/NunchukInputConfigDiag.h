// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class NunchukInputConfigDialog final : public InputConfigDialog
{
public:
  NunchukInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                           int port_num = 0);
};
