// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <utility>
#include <vector>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stockitem.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/ARDecrypt.h"
#include "Core/ActionReplay.h"
#include "DolphinWX/Cheats/ARCodeAddEdit.h"
#include "DolphinWX/WxUtils.h"

ARCodeAddEdit::ARCodeAddEdit(ActionReplay::ARCode code, wxWindow* parent, wxWindowID id,
                             const wxString& title, const wxPoint& position, const wxSize& size,
                             long style)
    : wxDialog(parent, id, title, position, size, style), m_code(std::move(code))
{
  CreateGUI();
}

void ARCodeAddEdit::CreateGUI()
{
  const int space10 = FromDIP(10);
  const int space5 = FromDIP(5);

  wxBoxSizer* sEditCheat = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, _("Cheat Code"));
  wxGridBagSizer* sgEntry = new wxGridBagSizer(space10, space10);

  wxStaticText* lbl_cheat_name = new wxStaticText(sbEntry->GetStaticBox(), wxID_ANY, _("Name:"));
  wxStaticText* lbl_cheat_codes = new wxStaticText(sbEntry->GetStaticBox(), wxID_ANY, _("Code:"));

  m_txt_cheat_name = new wxTextCtrl(sbEntry->GetStaticBox(), wxID_ANY, wxEmptyString);
  m_txt_cheat_name->SetValue(StrToWxStr(m_code.name));

  m_cheat_codes =
      new wxTextCtrl(sbEntry->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                     wxDLG_UNIT(this, wxSize(240, 128)), wxTE_MULTILINE);
  for (const auto& op : m_code.ops)
    m_cheat_codes->AppendText(wxString::Format("%08X %08X\n", op.cmd_addr, op.value));

  {
    wxFont font{m_cheat_codes->GetFont()};
    font.SetFamily(wxFONTFAMILY_TELETYPE);
#ifdef _WIN32
    // Windows uses Courier New for monospace even though there are better fonts.
    font.SetFaceName("Consolas");
#endif
    m_cheat_codes->SetFont(font);
  }

  sgEntry->Add(lbl_cheat_name, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER);
  sgEntry->Add(lbl_cheat_codes, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER);
  sgEntry->Add(m_txt_cheat_name, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND);
  sgEntry->Add(m_cheat_codes, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND);
  sgEntry->AddGrowableCol(1);
  sgEntry->AddGrowableRow(1);
  sbEntry->Add(sgEntry, 1, wxEXPAND | wxALL, space5);

  // OS X UX: ID_NO becomes "Don't Save" when paired with wxID_SAVE
  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_SAVE));
  buttons->AddButton(new wxButton(this, wxID_NO, wxGetStockLabel(wxID_CANCEL)));
  buttons->Realize();

  sEditCheat->AddSpacer(space5);
  sEditCheat->Add(sbEntry, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sEditCheat->AddSpacer(space10);
  sEditCheat->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sEditCheat->AddSpacer(space5);

  Bind(wxEVT_BUTTON, &ARCodeAddEdit::SaveCheatData, this, wxID_SAVE);

  SetEscapeId(wxID_NO);
  SetAffirmativeId(wxID_SAVE);
  SetSizerAndFit(sEditCheat);
}

void ARCodeAddEdit::SaveCheatData(wxCommandEvent& WXUNUSED(event))
{
  std::vector<ActionReplay::AREntry> decrypted_lines;
  std::vector<std::string> encrypted_lines;

  // Split the entered cheat into lines.
  const std::vector<std::string> input_lines =
      SplitString(WxStrToStr(m_cheat_codes->GetValue()), '\n');

  for (size_t i = 0; i < input_lines.size(); i++)
  {
    // Make sure to ignore unneeded whitespace characters.
    std::string line_str = StripSpaces(input_lines[i]);

    if (line_str.empty())
      continue;

    // Let's parse the current line.  Is it in encrypted or decrypted form?
    std::vector<std::string> pieces = SplitString(line_str, ' ');

    if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8)
    {
      // Decrypted code line.
      u32 addr = std::stoul(pieces[0], nullptr, 16);
      u32 value = std::stoul(pieces[1], nullptr, 16);

      decrypted_lines.emplace_back(addr, value);
      continue;
    }
    else if (pieces.size() == 1)
    {
      pieces = SplitString(line_str, '-');

      if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 &&
          pieces[2].size() == 5)
      {
        // Encrypted code line.  We'll have to decode it later.
        encrypted_lines.emplace_back(pieces[0] + pieces[1] + pieces[2]);
        continue;
      }
    }

    // If the above-mentioned conditions weren't met, then something went wrong.
    if (wxMessageBox(
            wxString::Format(_("Unable to parse line %u of the entered AR code as a valid "
                               "encrypted or decrypted code. Make sure you typed it correctly.\n\n"
                               "Would you like to ignore this line and continue parsing?"),
                             (unsigned)(i + 1)),
            _("Parsing Error"), wxYES_NO | wxICON_ERROR, this) == wxNO)
    {
      return;
    }
  }

  // If the entered code was in encrypted form, we decode it here.
  if (!encrypted_lines.empty())
  {
    // If the code contains a mixture of encrypted and unencrypted lines then we can't process it.
    int mode = wxYES;
    if (!decrypted_lines.empty())
    {
      mode =
          wxMessageBox(_("This Action Replay code contains both encrypted and unencrypted lines; "
                         "you should check that you have entered it correctly.\n\n"
                         "Do you want to discard all unencrypted lines?"),
                       _("Invalid Mixed Code"), wxYES_NO | wxCANCEL | wxICON_ERROR, this);
      // YES = Discard the unencrypted lines then decrypt the encrypted ones instead.
      // NO = Discard the encrypted lines, keep the unencrypted ones
      // CANCEL = Stop and let the user go back to editing
      if (mode == wxCANCEL)
        return;
      if (mode == wxYES)
        decrypted_lines.clear();
    }

    if (mode == wxYES)
      ActionReplay::DecryptARCode(encrypted_lines, &decrypted_lines);
  }

  // There's no point creating a code with no content.
  if (decrypted_lines.empty())
  {
    WxUtils::ShowErrorDialog(_("The resulting decrypted AR code doesn't contain any lines."));
    return;
  }

  ActionReplay::ARCode new_code;
  new_code.name = WxStrToStr(m_txt_cheat_name->GetValue());
  new_code.ops = std::move(decrypted_lines);
  new_code.active = true;
  new_code.user_defined = true;

  if (new_code.name != m_code.name || new_code.ops != m_code.ops)
  {
    m_code = std::move(new_code);
    AcceptAndClose();
  }
  else
  {
    EndDialog(GetEscapeId());
  }
}
