// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <functional>

#include <wx/checkbox.h>

#include "DolphinWX/ConfigUtils.h"

namespace ConfigUtils
{
void LoadValue(const SettingsMap& map, wxCheckBox* control)
{
  control->SetValue(Get<bool>(map, control));
}

void SaveOnChange(const SettingsMap& map, wxCheckBox* checkbox)
{
  // MSVC needs some help to be able to compile this. The type has to be fully specified.
  const std::function<void(wxCommandEvent&)> handler = [&map, checkbox](auto&) {
    Set(map, checkbox, checkbox->IsChecked());
  };

  checkbox->Bind(wxEVT_CHECKBOX, handler);
}
};  // namespace ConfigUtils
