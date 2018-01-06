// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/AssemblerEntryDialog.h"

#include <string>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"

AssemblerEntryDialog::AssemblerEntryDialog(u32 address, wxWindow* parent, const wxString& message,
                                           const wxString& caption, const wxString& value,
                                           long style, const wxPoint& pos)
    : m_address(address)
{
  Create(parent, message, caption, value, style, pos);
}

bool AssemblerEntryDialog::Create(wxWindow* parent, const wxString& message,
                                  const wxString& caption, const wxString& value, long style,
                                  const wxPoint& pos)
{
  // Do not pass style to GetParentForModalDialog() as wxDIALOG_NO_PARENT
  // has the same numeric value as wxTE_MULTILINE and so attempting to create
  // a dialog for editing multiline text would also prevent it from having a
  // parent which is undesirable. As it is, we can't create a text entry
  // dialog without a parent which is not ideal neither but is a much less
  // important problem.
  if (!wxDialog::Create(GetParentForModalDialog(parent, 0), wxID_ANY, caption, pos, wxDefaultSize,
                        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER))
  {
    return false;
  }

  m_dialogStyle = style;
  m_value = value;

  wxBeginBusyCursor();

  wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);

  // 1) text message
  top_sizer->Add(CreateTextSizer(message), wxSizerFlags().DoubleBorder());

  // 2) text ctrl
  m_textctrl = new wxTextCtrl(this, 3000, value, wxDefaultPosition, wxSize(300, wxDefaultCoord),
                              style & ~wxTextEntryDialogStyle);
  m_textctrl->Bind(wxEVT_TEXT, &AssemblerEntryDialog::OnTextChanged, this);

  top_sizer->Add(
      m_textctrl,
      wxSizerFlags(style & wxTE_MULTILINE ? 1 : 0).Expand().TripleBorder(wxLEFT | wxRIGHT));

  m_preview = new wxStaticText(this, wxID_ANY, "");
  top_sizer->Add(m_preview, wxSizerFlags().DoubleBorder(wxUP | wxLEFT | wxRIGHT));

  wxSizer* button_sizer = CreateSeparatedButtonSizer(style & (wxOK | wxCANCEL));
  if (button_sizer)
    top_sizer->Add(button_sizer, wxSizerFlags().DoubleBorder().Expand());

  SetAutoLayout(true);
  SetSizer(top_sizer);

  top_sizer->SetSizeHints(this);
  top_sizer->Fit(this);

  if (style & wxCENTRE)
    Centre(wxBOTH);

  m_textctrl->SelectAll();
  m_textctrl->SetFocus();

  wxEndBusyCursor();

  return true;
}

void AssemblerEntryDialog::OnTextChanged(wxCommandEvent& evt)
{
  unsigned long code;
  std::string result = "Input text is invalid";
  if (evt.GetString().ToULong(&code, 0) && code <= std::numeric_limits<u32>::max())
    result = TabsToSpaces(1, GekkoDisassembler::Disassemble(code, m_address));
  m_preview->SetLabel(wxString::Format(_("Preview: %s"), result.c_str()));
}
