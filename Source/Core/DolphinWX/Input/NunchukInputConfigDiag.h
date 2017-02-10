// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Input/InputConfigDiag.h"

class wxComboBox;

class NunchukInputConfigDialog final : public InputConfigDialog
{
public:
  NunchukInputConfigDialog(wxWindow* parent, InputConfig& config, const wxString& name,
                           wxComboBox* device_cbox_parent, int port_num = 0);
};
