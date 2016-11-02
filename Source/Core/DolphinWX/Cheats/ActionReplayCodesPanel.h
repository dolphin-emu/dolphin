// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <wx/panel.h>

#include "Core/ActionReplay.h"

class wxButton;
class wxCheckListBox;
class wxListBox;
class wxStaticBoxSizer;
class wxStaticText;

class ARCodeAddEdit;
class IniFile;

// GetClientData() -> ActionReplay::ARCode* [immutable]
wxDECLARE_EVENT(DOLPHIN_EVT_ARCODE_TOGGLED, wxCommandEvent);

class ActionReplayCodesPanel final : public wxPanel
{
public:
  enum Style
  {
    STYLE_LIST = 0,           // Show checklist
    STYLE_SIDE_PANEL = 1,     // Show side panel displaying code content
    STYLE_MODIFY_BUTTONS = 2  // Show buttons for adding/editing/removing codes
  };

  explicit ActionReplayCodesPanel(wxWindow* parent, Style styles = STYLE_LIST);
  ~ActionReplayCodesPanel() override;

  void LoadCodes(const IniFile& global_ini, const IniFile& local_ini);
  const std::vector<ActionReplay::ARCode>& GetCodes() { return m_codes; }
  void SaveCodes(IniFile* local_ini);
  void AppendNewCode(const ActionReplay::ARCode& code);
  void Clear();

  bool IsModified() const { return m_was_modified; }
  void ClearModified() { m_was_modified = false; }
  Style GetCodePanelStyle() const { return m_styles; }
  void SetCodePanelStyle(Style style);

private:
  void CreateGUI();
  void Repopulate();
  void UpdateSidePanel();
  void UpdateModifyButtons();
  void GenerateToggleEvent(const ActionReplay::ARCode& code);
  void OnCodeSelectionChanged(wxCommandEvent&);
  void OnCodeChecked(wxCommandEvent&);
  void OnCodeDoubleClick(wxCommandEvent&);
  void OnAddNewCodeClick(wxCommandEvent&);
  void OnEditCodeClick(wxCommandEvent&);
  void OnRemoveCodeClick(wxCommandEvent&);

  std::vector<ActionReplay::ARCode> m_codes;

  wxStaticText* m_label_code_name = nullptr;
  wxStaticText* m_label_num_codes = nullptr;
  wxCheckListBox* m_checklist_cheats = nullptr;
  wxListBox* m_list_codes = nullptr;
  wxPanel* m_modify_buttons = nullptr;
  wxButton* m_btn_edit_code = nullptr;
  wxButton* m_btn_remove_code = nullptr;
  wxStaticBoxSizer* m_side_panel = nullptr;

  ARCodeAddEdit* m_editor = nullptr;

  Style m_styles = STYLE_LIST;
  bool m_was_modified = false;
};

constexpr ActionReplayCodesPanel::Style operator|(ActionReplayCodesPanel::Style a,
                                                  ActionReplayCodesPanel::Style b)
{
  return static_cast<ActionReplayCodesPanel::Style>(static_cast<int>(a) | b);
}

constexpr ActionReplayCodesPanel::Style operator&(ActionReplayCodesPanel::Style a,
                                                  ActionReplayCodesPanel::Style b)
{
  return static_cast<ActionReplayCodesPanel::Style>(static_cast<int>(a) & b);
}
