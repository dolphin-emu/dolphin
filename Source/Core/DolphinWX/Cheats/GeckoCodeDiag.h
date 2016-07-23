// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <wx/panel.h>

#include "Core/GeckoCode.h"

class IniFile;
class wxButton;
class wxCheckListBox;
class wxListBox;
class wxStaticText;
class wxTextCtrl;

// GetClientData() -> GeckoCode* [immutable]
wxDECLARE_EVENT(DOLPHIN_EVT_GECKOCODE_TOGGLED, wxCommandEvent);

namespace Gecko
{
class CodeConfigPanel : public wxPanel
{
public:
  CodeConfigPanel(wxWindow* const parent);

  void LoadCodes(const IniFile& globalIni, const IniFile& localIni, const std::string& gameid = "",
                 bool checkRunning = false);
  const std::vector<GeckoCode>& GetCodes() const { return m_gcodes; }
protected:
  void UpdateInfoBox(wxCommandEvent&);
  void ToggleCode(wxCommandEvent& evt);
  void DownloadCodes(wxCommandEvent&);
  // void ApplyChanges(wxCommandEvent&);

  void UpdateCodeList(bool checkRunning = false);

private:
  std::vector<GeckoCode> m_gcodes;

  std::string m_gameid;

  // wxwidgets stuff
  wxCheckListBox* m_listbox_gcodes;
  struct
  {
    wxStaticText *label_name, *label_notes, *label_creator;
    wxTextCtrl* textctrl_notes;
    wxListBox* listbox_codes;
  } m_infobox;
  wxButton* btn_download;
};
}
