// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/font.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include "DolphinWX/Cheats/ARCodeAddEdit.h"
#include "DolphinWX/Cheats/ActionReplayCodesPanel.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_ARCODE_TOGGLED, wxCommandEvent);

ActionReplayCodesPanel::ActionReplayCodesPanel(wxWindow* parent, Style styles) : wxPanel(parent)
{
  SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
  CreateGUI();
  SetCodePanelStyle(styles);
}

ActionReplayCodesPanel::~ActionReplayCodesPanel()
{
}

void ActionReplayCodesPanel::LoadCodes(const IniFile& global_ini, const IniFile& local_ini)
{
  m_codes = ActionReplay::LoadCodes(global_ini, local_ini);
  m_was_modified = false;
  Repopulate();
}

void ActionReplayCodesPanel::SaveCodes(IniFile* local_ini)
{
  ActionReplay::SaveCodes(local_ini, m_codes);
  m_was_modified = false;
}

void ActionReplayCodesPanel::AppendNewCode(const ActionReplay::ARCode& code)
{
  m_codes.push_back(code);
  int idx = m_checklist_cheats->Append(m_checklist_cheats->EscapeMnemonics(StrToWxStr(code.name)));
  if (code.active)
    m_checklist_cheats->Check(idx);
  m_was_modified = true;
  GenerateToggleEvent(code);
}

void ActionReplayCodesPanel::Clear()
{
  m_was_modified = false;
  m_codes.clear();
  m_codes.shrink_to_fit();
  Repopulate();
}

void ActionReplayCodesPanel::SetCodePanelStyle(Style styles)
{
  m_styles = styles;
  m_side_panel->GetStaticBox()->Show(!!(styles & STYLE_SIDE_PANEL));
  m_modify_buttons->Show(!!(styles & STYLE_MODIFY_BUTTONS));
  UpdateSidePanel();
  UpdateModifyButtons();
  Layout();
}

void ActionReplayCodesPanel::CreateGUI()
{
  // STYLE_LIST
  m_checklist_cheats = new wxCheckListBox(this, wxID_ANY);

  // STYLE_SIDE_PANEL
  m_side_panel = new wxStaticBoxSizer(wxVERTICAL, this, _("Code Info"));
  m_label_code_name = new wxStaticText(m_side_panel->GetStaticBox(), wxID_ANY, _("Name: "),
                                       wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
  m_label_num_codes =
      new wxStaticText(m_side_panel->GetStaticBox(), wxID_ANY, _("Number of Codes: ") + '0');

  m_list_codes = new wxListBox(m_side_panel->GetStaticBox(), wxID_ANY);
  {
    wxFont monospace{m_list_codes->GetFont()};
    monospace.SetFamily(wxFONTFAMILY_TELETYPE);
#ifdef _WIN32
    monospace.SetFaceName("Consolas");  // Windows always uses Courier New
#endif
    m_list_codes->SetFont(monospace);
  }

  const int space5 = FromDIP(5);

  m_side_panel->AddSpacer(space5);
  m_side_panel->Add(m_label_code_name, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  m_side_panel->AddSpacer(space5);
  m_side_panel->Add(m_label_num_codes, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  m_side_panel->AddSpacer(space5);
  m_side_panel->Add(m_list_codes, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  m_side_panel->AddSpacer(space5);
  m_side_panel->SetMinSize(FromDIP(wxSize(180, -1)));

  // STYLE_MODIFY_BUTTONS
  m_modify_buttons = new wxPanel(this);
  wxButton* btn_add_code = new wxButton(m_modify_buttons, wxID_ANY, _("&Add New Code..."));
  m_btn_edit_code = new wxButton(m_modify_buttons, wxID_ANY, _("&Edit Code..."));
  m_btn_remove_code = new wxButton(m_modify_buttons, wxID_ANY, _("&Remove Code"));

  wxBoxSizer* button_layout = new wxBoxSizer(wxHORIZONTAL);
  button_layout->Add(btn_add_code);
  button_layout->AddStretchSpacer();
  button_layout->Add(m_btn_edit_code);
  button_layout->Add(m_btn_remove_code);
  m_modify_buttons->SetSizer(button_layout);

  // Top level layouts
  wxBoxSizer* panel_layout = new wxBoxSizer(wxHORIZONTAL);
  panel_layout->Add(m_checklist_cheats, 1, wxEXPAND);
  panel_layout->Add(m_side_panel, 0, wxEXPAND | wxLEFT, space5);

  wxBoxSizer* main_layout = new wxBoxSizer(wxVERTICAL);
  main_layout->Add(panel_layout, 1, wxEXPAND);
  main_layout->Add(m_modify_buttons, 0, wxEXPAND | wxTOP, space5);

  m_checklist_cheats->Bind(wxEVT_LISTBOX, &ActionReplayCodesPanel::OnCodeSelectionChanged, this);
  m_checklist_cheats->Bind(wxEVT_LISTBOX_DCLICK, &ActionReplayCodesPanel::OnCodeDoubleClick, this);
  m_checklist_cheats->Bind(wxEVT_CHECKLISTBOX, &ActionReplayCodesPanel::OnCodeChecked, this);
  btn_add_code->Bind(wxEVT_BUTTON, &ActionReplayCodesPanel::OnAddNewCodeClick, this);
  m_btn_edit_code->Bind(wxEVT_BUTTON, &ActionReplayCodesPanel::OnEditCodeClick, this);
  m_btn_remove_code->Bind(wxEVT_BUTTON, &ActionReplayCodesPanel::OnRemoveCodeClick, this);

  SetSizer(main_layout);
}

void ActionReplayCodesPanel::Repopulate()
{
  // If the code editor is open then it's invalidated now.
  // (whatever it was doing is no longer relevant)
  if (m_editor)
    m_editor->EndModal(wxID_NO);

  m_checklist_cheats->Freeze();
  m_checklist_cheats->Clear();

  for (const auto& code : m_codes)
  {
    int idx =
        m_checklist_cheats->Append(m_checklist_cheats->EscapeMnemonics(StrToWxStr(code.name)));
    if (code.active)
      m_checklist_cheats->Check(idx);
  }
  m_checklist_cheats->Thaw();

  // Clear side panel contents since selection is invalidated
  UpdateSidePanel();
  UpdateModifyButtons();
}

void ActionReplayCodesPanel::UpdateSidePanel()
{
  if (!(m_styles & STYLE_SIDE_PANEL))
    return;

  wxString name;
  std::size_t code_count = 0;
  if (m_checklist_cheats->GetSelection() != wxNOT_FOUND)
  {
    auto& code = m_codes.at(m_checklist_cheats->GetSelection());
    name = StrToWxStr(code.name);
    code_count = code.ops.size();

    m_list_codes->Freeze();
    m_list_codes->Clear();
    for (const auto& entry : code.ops)
    {
      m_list_codes->Append(wxString::Format("%08X %08X", entry.cmd_addr, entry.value));
    }
    m_list_codes->Thaw();
  }
  else
  {
    m_list_codes->Clear();
  }

  m_label_code_name->SetLabelText(_("Name: ") + name);
  m_label_code_name->Wrap(m_label_code_name->GetSize().GetWidth());
  m_label_code_name->InvalidateBestSize();
  m_label_num_codes->SetLabelText(wxString::Format("%s%zu", _("Number of Codes: "), code_count));
  Layout();
}

void ActionReplayCodesPanel::UpdateModifyButtons()
{
  if (!(m_styles & STYLE_MODIFY_BUTTONS))
    return;

  bool is_user_defined = true;
  bool enable_buttons = false;
  if (m_checklist_cheats->GetSelection() != wxNOT_FOUND)
  {
    is_user_defined = m_codes.at(m_checklist_cheats->GetSelection()).user_defined;
    enable_buttons = true;
  }

  m_btn_edit_code->SetLabel(is_user_defined ? _("&Edit Code...") : _("Clone and &Edit Code..."));
  m_btn_edit_code->Enable(enable_buttons);
  m_btn_remove_code->Enable(enable_buttons && is_user_defined);
  Layout();
}

void ActionReplayCodesPanel::GenerateToggleEvent(const ActionReplay::ARCode& code)
{
  wxCommandEvent toggle_event{DOLPHIN_EVT_ARCODE_TOGGLED, GetId()};
  toggle_event.SetClientData(const_cast<ActionReplay::ARCode*>(&code));
  if (!GetEventHandler()->ProcessEvent(toggle_event))
  {
    // Because wxWS_EX_BLOCK_EVENTS affects all events, propagation needs to be done manually.
    GetParent()->GetEventHandler()->ProcessEvent(toggle_event);
  }
}

void ActionReplayCodesPanel::OnCodeChecked(wxCommandEvent& ev)
{
  auto& code = m_codes.at(ev.GetSelection());
  code.active = m_checklist_cheats->IsChecked(ev.GetSelection());
  m_was_modified = true;
  GenerateToggleEvent(code);
}

void ActionReplayCodesPanel::OnCodeSelectionChanged(wxCommandEvent&)
{
  UpdateSidePanel();
  UpdateModifyButtons();
}

void ActionReplayCodesPanel::OnCodeDoubleClick(wxCommandEvent& ev)
{
  if (!(m_styles & STYLE_MODIFY_BUTTONS))
    return;

  OnEditCodeClick(ev);
}

void ActionReplayCodesPanel::OnAddNewCodeClick(wxCommandEvent&)
{
  ARCodeAddEdit editor{{}, this, wxID_ANY, _("Add ActionReplay Code")};
  m_editor = &editor;
  if (editor.ShowModal() == wxID_SAVE)
    AppendNewCode(editor.GetCode());
  m_editor = nullptr;
}

void ActionReplayCodesPanel::OnEditCodeClick(wxCommandEvent&)
{
  int idx = m_checklist_cheats->GetSelection();
  wxASSERT(idx != wxNOT_FOUND);
  auto& code = m_codes.at(idx);
  // If the code is from the global INI then we'll have to clone it.
  if (!code.user_defined)
  {
    ARCodeAddEdit editor{code, this, wxID_ANY, _("Duplicate Bundled ActionReplay Code")};
    m_editor = &editor;
    if (editor.ShowModal() == wxID_SAVE)
      AppendNewCode(editor.GetCode());
    m_editor = nullptr;
    return;
  }

  ARCodeAddEdit editor{code, this};
  m_editor = &editor;
  if (editor.ShowModal() == wxID_SAVE)
  {
    code = editor.GetCode();
    m_checklist_cheats->SetString(idx, m_checklist_cheats->EscapeMnemonics(StrToWxStr(code.name)));
    m_checklist_cheats->Check(idx, code.active);
    m_was_modified = true;
    UpdateSidePanel();
    GenerateToggleEvent(code);
  }
  m_editor = nullptr;
}

void ActionReplayCodesPanel::OnRemoveCodeClick(wxCommandEvent&)
{
  int idx = m_checklist_cheats->GetSelection();
  wxASSERT(idx != wxNOT_FOUND);
  m_codes.erase(m_codes.begin() + idx);
  m_checklist_cheats->Delete(idx);
  m_was_modified = true;

  UpdateModifyButtons();
}
