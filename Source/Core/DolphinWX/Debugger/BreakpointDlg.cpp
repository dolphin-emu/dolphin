// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/BreakpointDlg.h"

#include <string>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/WxUtils.h"

BreakPointDlg::BreakPointDlg(wxWindow* _Parent) : wxDialog(_Parent, wxID_ANY, _("Add Breakpoint"))
{
  Bind(wxEVT_BUTTON, &BreakPointDlg::OnOK, this, wxID_OK);

  m_pEditAddress = new wxTextCtrl(this, wxID_ANY, "80000000");

  const int space5 = FromDIP(5);
  wxBoxSizer* main_szr = new wxBoxSizer(wxVERTICAL);
  main_szr->AddSpacer(space5);
  main_szr->Add(m_pEditAddress, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);
  main_szr->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_szr->AddSpacer(space5);

  SetSizerAndFit(main_szr);
  SetFocus();
}

void BreakPointDlg::OnOK(wxCommandEvent& event)
{
  wxString AddressString = m_pEditAddress->GetLineText(0);
  u32 Address = 0;
  if (AsciiToHex(WxStrToStr(AddressString), Address))
  {
    PowerPC::breakpoints.Add(Address);
    EndModal(wxID_OK);
  }
  else
  {
    WxUtils::ShowErrorDialog(
        wxString::Format(_("The address %s is invalid."), WxStrToStr(AddressString).c_str()));
  }

  event.Skip();
}
