// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ARDecrypt.h"
#include "Core/ActionReplay.h"
#include "DolphinWX/ARCodeAddEdit.h"
#include "DolphinWX/WxUtils.h"

CARCodeAddEdit::CARCodeAddEdit(int _selection, std::vector<ActionReplay::ARCode>* _arCodes,
                               wxWindow* parent, wxWindowID id, const wxString& title,
                               const wxPoint& position, const wxSize& size, long style)
    : wxDialog(parent, id, title, position, size, style), arCodes(_arCodes), selection(_selection)
{
  Bind(wxEVT_BUTTON, &CARCodeAddEdit::SaveCheatData, this, wxID_OK);

  ActionReplay::ARCode tempEntries;
  wxString currentName;

  if (selection == wxNOT_FOUND)
  {
    tempEntries.name = "";
  }
  else
  {
    currentName = StrToWxStr(arCodes->at(selection).name);
    tempEntries = arCodes->at(selection);
  }

  wxBoxSizer* sEditCheat = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, _("Cheat Code"));
  wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);

  wxStaticText* EditCheatNameText = new wxStaticText(this, wxID_ANY, _("Name:"));
  wxStaticText* EditCheatCodeText = new wxStaticText(this, wxID_ANY, _("Code:"));
  wxStaticText* EditCheatMeaningText = new wxStaticText(this, wxID_ANY, _("Meaning:"));

  EditCheatName =
      new wxTextCtrl(this, wxID_ANY, currentName, wxDefaultPosition, wxSize(300, wxDefaultCoord));

  EntrySelection = new wxSpinButton(this);
  EntrySelection->SetRange(1, std::max((int)arCodes->size(), 1));
  EntrySelection->SetValue((int)(arCodes->size() - selection));
  EntrySelection->Bind(wxEVT_SPIN, &CARCodeAddEdit::ChangeEntry, this);

  EditCheatCode = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, 100),
                                 wxTE_MULTILINE);
  EditCheatMeaning = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxSize(300, 100), wxTE_MULTILINE | wxTE_READONLY);

  UpdateTextCtrl(tempEntries);

  sgEntry->Add(EditCheatNameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER | wxALL, 5);
  sgEntry->Add(EditCheatCodeText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER | wxALL, 5);
  sgEntry->Add(EditCheatMeaningText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER | wxALL, 5);
  sgEntry->Add(EditCheatName, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);
  sgEntry->Add(EntrySelection, wxGBPosition(0, 2), wxGBSpan(3, 1), wxEXPAND | wxALL, 5);
  sgEntry->Add(EditCheatCode, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);
  sgEntry->Add(EditCheatMeaning, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);
  sgEntry->AddGrowableCol(1);
  sgEntry->AddGrowableRow(1);
  sbEntry->Add(sgEntry, 1, wxEXPAND | wxALL);

  sEditCheat->Add(sbEntry, 1, wxEXPAND | wxALL, 5);
  sEditCheat->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 5);

  SetSizerAndFit(sEditCheat);
  SetFocus();
}

void CARCodeAddEdit::ChangeEntry(wxSpinEvent& event)
{
  ActionReplay::ARCode currentCode = arCodes->at((int)arCodes->size() - event.GetPosition());
  EditCheatName->SetValue(StrToWxStr(currentCode.name));
  UpdateTextCtrl(currentCode);
}

void CARCodeAddEdit::SaveCheatData(wxCommandEvent& WXUNUSED(event))
{
  std::vector<ActionReplay::AREntry> decryptedLines;
  std::vector<std::string> encryptedLines;

  // Split the entered cheat into lines.
  std::vector<std::string> userInputLines;
  SplitString(WxStrToStr(EditCheatCode->GetValue()), '\n', userInputLines);

  for (size_t i = 0; i < userInputLines.size(); i++)
  {
    // Make sure to ignore unneeded whitespace characters.
    std::string line_str = StripSpaces(userInputLines[i]);

    if (line_str == "")
      continue;

    // Let's parse the current line.  Is it in encrypted or decrypted form?
    std::vector<std::string> pieces;
    SplitString(line_str, ' ', pieces);

    if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8)
    {
      // Decrypted code line.
      u32 addr = std::stoul(pieces[0], nullptr, 16);
      u32 value = std::stoul(pieces[1], nullptr, 16);

      decryptedLines.emplace_back(addr, value);
      continue;
    }
    else if (pieces.size() == 1)
    {
      SplitString(line_str, '-', pieces);

      if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 &&
          pieces[2].size() == 5)
      {
        // Encrypted code line.  We'll have to decode it later.
        encryptedLines.push_back(pieces[0] + pieces[1] + pieces[2]);
        continue;
      }
    }

    // If the above-mentioned conditions weren't met, then something went wrong.
    if (!PanicYesNoT("Unable to parse line %u of the entered AR code as a valid "
                     "encrypted or decrypted code.  Make sure you typed it correctly.\n"
                     "Would you like to ignore this line and continue parsing?",
                     (unsigned)(i + 1)))
    {
      return;
    }
  }

  // If the entered code was in encrypted form, we decode it here.
  if (encryptedLines.size())
  {
    // TODO: what if both decrypted AND encrypted lines are entered into a single AR code?
    ActionReplay::DecryptARCode(encryptedLines, decryptedLines);
  }

  // Codes with no lines appear to be deleted/hidden from the list.  Let's prevent that.
  if (!decryptedLines.size())
  {
    WxUtils::ShowErrorDialog(_("The resulting decrypted AR code doesn't contain any lines."));
    return;
  }

  if (selection == wxNOT_FOUND)
  {
    // Add a new AR cheat code.
    ActionReplay::ARCode newCheat;

    newCheat.name = WxStrToStr(EditCheatName->GetValue());
    newCheat.ops = decryptedLines;
    newCheat.active = true;
    newCheat.user_defined = true;

    arCodes->push_back(newCheat);
  }
  else
  {
    // Update the currently-selected AR cheat code.
    arCodes->at(selection).name = WxStrToStr(EditCheatName->GetValue());
    arCodes->at(selection).ops = decryptedLines;
  }

  AcceptAndClose();
}

void CARCodeAddEdit::UpdateTextCtrl(ActionReplay::ARCode arCode)
{
  EditCheatCode->Clear();
  EditCheatMeaning->Clear();

  if (arCode.name.empty())
  {
    EditCheatMeaning->Disable();
    return;
  }

  bool do_fill_and_slide = false;
  bool do_memory_copy = false;

  // used for conditional codes
  int skip_count = 0;

  u32 val_last = 0;

  for (auto& op : arCode.ops)
  {
    EditCheatCode->AppendText(wxString::Format("%08X %08X\n", op.cmd_addr, op.value));
    {
      const ActionReplay::ARAddr addr(op.cmd_addr);
      const u32 data = op.value;
      // Zero codes
      if (0x0 == addr)  // Check if the code is a zero code
      {
        const u8 zcode = data >> 29;
        switch (zcode)
        {
        case ActionReplay::ZCODE_END:  // END OF CODES
          EditCheatMeaning->AppendText("ZCode: End Of Codes");
          break;

        // TODO: the "00000000 40000000"(end if) codes fall into this case, I don't think that is
        // correct
        case ActionReplay::ZCODE_NORM:  // Normal execution of codes
                                        // Todo: Set register 1BB4 to 0
          if (data == 0x40000000)
            EditCheatMeaning->AppendText("}");
          else
            EditCheatMeaning->AppendText(
                "ZCode: Normal execution of codes, set register 1BB4 to 0 (zcode not supported)");
          break;

        case ActionReplay::ZCODE_ROW:  // Executes all codes in the same row
                                       // Todo: Set register 1BB4 to 1
          EditCheatMeaning->AppendText("ZCode: Executes all codes in the same row, Set register "
                                       "1BB4 to 1 (zcode not supported)");
          break;

        case ActionReplay::ZCODE_04:  // Fill & Slide or Memory Copy
          if (0x3 == ((data >> 25) & 0x03))
          {
            do_memory_copy = true;
            val_last = data;
            EditCheatMeaning->AppendText(
                wxString::Format("ZCode: Memory Copy, val_last=%8X", val_last));
          }
          else
          {
            do_fill_and_slide = true;
            val_last = data;
            EditCheatMeaning->AppendText(
                wxString::Format("ZCode: Fill And Slide, val_last=%8X", val_last));
          }
          break;

        default:
          EditCheatMeaning->AppendText(wxString::Format("ZCode: Unknown: %02X", zcode));
        }
      }
      else
      {
        const u32 new_addr = addr.GCAddress();
        switch (addr.type)
        {
        case 0x00:
          switch (addr.subtype)
          {
          case ActionReplay::SUB_RAM_WRITE:  // Ram write (and fill)
            switch (addr.size)
            {
            case ActionReplay::DATATYPE_8BIT:
              EditCheatMeaning->AppendText(wxString::Format("fill %d byte(s) at %8X with %02X (%d)",
                                                            (data >> 8) + 1, new_addr, data & 0xFF,
                                                            data & 0xFF));
              break;
            case ActionReplay::DATATYPE_16BIT:
              EditCheatMeaning->AppendText(
                  wxString::Format("fill %d two-byte number(s) at %8X with %04X (%d)",
                                   (data >> 16) + 1, new_addr, data & 0xFFFF, data & 0xFFFF));
              break;
            case ActionReplay::DATATYPE_32BIT:
              EditCheatMeaning->AppendText(wxString::Format("set %8X to %08X", new_addr, data));
              if (data == 0x4E800020)
                EditCheatMeaning->AppendText(" = blr (return)");
              else if ((data & 0xFFFFFF00) == 0x38600000)
                EditCheatMeaning->AppendText(
                    wxString::Format(" = li r3, 0x%X (Result = %d)", data & 0xFF, data & 0xFF));
              else
                EditCheatMeaning->AppendText(wxString::Format(" (%d)", data));
              break;
            case ActionReplay::DATATYPE_32BIT_FLOAT:
              EditCheatMeaning->AppendText(
                  wxString::Format("set %s%8X to %f", new_addr, *(float*)&data));
              break;
            }
            break;
          case ActionReplay::SUB_WRITE_POINTER:
            switch (addr.size)
            {
            case ActionReplay::DATATYPE_8BIT:
              EditCheatMeaning->AppendText(wxString::Format("set pointer(%8X)+%06X to %02X (%d)",
                                                            new_addr, data >> 8, data & 0xFF,
                                                            data & 0xFF));
              break;
            case ActionReplay::DATATYPE_16BIT:
              EditCheatMeaning->AppendText(wxString::Format("set pointer(%8X)+%04X to %04X (%d)",
                                                            new_addr, data >> 16, data & 0xFFFF,
                                                            data & 0xFFFF));
              break;
            case ActionReplay::DATATYPE_32BIT:
              EditCheatMeaning->AppendText(
                  wxString::Format("set pointer(%8X) to %08X (%d)", new_addr, data, data));
              break;
            case ActionReplay::DATATYPE_32BIT_FLOAT:
              EditCheatMeaning->AppendText(
                  wxString::Format("set pointer(%8X) to %f", new_addr, *(float*)&data));
              break;
            }
            break;
          case ActionReplay::SUB_ADD_CODE:  // Increment Value
            switch (addr.size)
            {
            case ActionReplay::DATATYPE_8BIT:
              EditCheatMeaning->AppendText(
                  wxString::Format("add %02X (%d) to %8x", data & 0xFF, data & 0xFF, new_addr));
              break;
            case ActionReplay::DATATYPE_16BIT:
              EditCheatMeaning->AppendText(wxString::Format("add %04X (%d) to %8x", data & 0xFFFF,
                                                            (s16)(data & 0xFFFF), new_addr));
              break;
            case ActionReplay::DATATYPE_32BIT:
              EditCheatMeaning->AppendText(
                  wxString::Format("add %08X (%d) to %8x", data, (s32)data, new_addr));
              break;
            case ActionReplay::DATATYPE_32BIT_FLOAT:
              EditCheatMeaning->AppendText(
                  wxString::Format("add %f to %8x", *(float*)&data, new_addr));
              break;
            }
            break;
          case ActionReplay::SUB_MASTER_CODE:  // Master Code & Write to CCXXXXXX
            EditCheatMeaning->AppendText(wxString::Format(
                "Doing Master Code And Write to CCXXXXXX (ncode not supported) %8X, %8X",
                addr.address, data));
            break;
          }
          break;

        default:
          const u32 new_addr = addr.GCAddress();
          const char *t, *comp, *s;
          switch (addr.subtype)
          {
          case ActionReplay::CONDTIONAL_ALL_LINES:
            s = "continue, else return";
            break;
          case ActionReplay::CONDTIONAL_ALL_LINES_UNTIL:
            s = "{";
            break;
          case ActionReplay::CONDTIONAL_ONE_LINE:
            s = "";
            break;
          case ActionReplay::CONDTIONAL_TWO_LINES:
            s = "do next two lines";
            break;
          }
          switch (addr.type)
          {
          case ActionReplay::CONDTIONAL_EQUAL:
            comp = "==";
            t = "";
            break;
          case ActionReplay::CONDTIONAL_NOT_EQUAL:
            comp = "!=";
            t = "";
            break;
          case ActionReplay::CONDTIONAL_LESS_THAN_SIGNED:
            comp = "<";
            t = "signed ";
            break;
          case ActionReplay::CONDTIONAL_LESS_THAN_UNSIGNED:
            comp = "<";
            t = "unsigned ";
            break;
          case ActionReplay::CONDTIONAL_GREATER_THAN_SIGNED:
            comp = ">";
            t = "signed ";
            break;
          case ActionReplay::CONDTIONAL_GREATER_THAN_UNSIGNED:
            comp = ">";
            t = "unsigned ";
            break;
          case ActionReplay::CONDTIONAL_AND:
            comp = "AND";
            t = "";
            break;
          }
          switch (addr.size)
          {
          case ActionReplay::DATATYPE_8BIT:
            EditCheatMeaning->AppendText(wxString::Format("if %sbyte at %8X %s %02X (%d) then %s",
                                                          t, new_addr, comp, data & 0xFF,
                                                          data & 0xFF, s));
            break;

          case ActionReplay::DATATYPE_16BIT:
            EditCheatMeaning->AppendText(
                wxString::Format("if %stwo bytes at %8X %s %04X (%d) then %s", t, new_addr, comp,
                                 data & 0xFFFF, data & 0xFFFF, s));
            break;

          case ActionReplay::DATATYPE_32BIT:
            EditCheatMeaning->AppendText(wxString::Format(
                "if %sfour bytes at %8X %s %08X (%d) then %s", t, new_addr, comp, data, data, s));
            break;
          case ActionReplay::DATATYPE_32BIT_FLOAT:
            EditCheatMeaning->AppendText(
                wxString::Format("if %sfloating point number at %8X %s %f then %s", t, new_addr,
                                 comp, *(float*)&data, s));
            break;
          }
          break;
        }
      }
    }
    EditCheatMeaning->AppendText("\n");
  }  // next
  if (EditCheatMeaning->GetNumberOfLines() > 0)
    EditCheatMeaning->Enable();
  else
    EditCheatMeaning->Disable();
}
