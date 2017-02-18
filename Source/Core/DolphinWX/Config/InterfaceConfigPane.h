// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/arrstr.h>
#include <wx/panel.h>

#include "DolphinWX/ConfigUtils.h"

class wxButton;
class wxCheckBox;
class wxChoice;

class InterfaceConfigPane final : public wxPanel
{
public:
  InterfaceConfigPane(wxWindow* parent, wxWindowID id);

private:
  void InitializeGUI();
  void LoadGUIValues();
  void BindEvents();
  void LoadThemes();

  void OnInterfaceLanguageChoiceChanged(wxCommandEvent&);
  void OnThemeSelected(wxCommandEvent&);

  wxArrayString m_interface_lang_strings;

  wxCheckBox* m_confirm_stop_checkbox;
  wxCheckBox* m_panic_handlers_checkbox;
  wxCheckBox* m_osd_messages_checkbox;
  wxCheckBox* m_pause_focus_lost_checkbox;
  wxChoice* m_interface_lang_choice;
  wxChoice* m_theme_choice;

  ConfigUtils::SettingsMap m_settings;
};
