// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Debugger/MemoryCheckDlg.h"

#include <string>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/WxUtils.h"

MemoryCheckDlg::MemoryCheckDlg(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Add a Memory Breakpoint"))
{
  Bind(wxEVT_BUTTON, &MemoryCheckDlg::OnOK, this, wxID_OK);
  Bind(wxEVT_RADIOBUTTON, &MemoryCheckDlg::OnRadioButtonClick, this);

  const int space5 = FromDIP(5);

  m_textAddress = new wxStaticText(this, wxID_ANY, _("Address"));
  m_textStartAddress = new wxStaticText(this, wxID_ANY, _("Start"));
  m_textStartAddress->Disable();
  m_textEndAddress = new wxStaticText(this, wxID_ANY, _("End"));
  m_textEndAddress->Disable();
  m_pEditAddress = new wxTextCtrl(this, wxID_ANY);
  m_pEditStartAddress = new wxTextCtrl(this, wxID_ANY);
  m_pEditStartAddress->Disable();
  m_pEditEndAddress = new wxTextCtrl(this, wxID_ANY);
  m_pEditEndAddress->Disable();
  m_radioAddress = new wxRadioButton(this, wxID_ANY, _("With an Address"), wxDefaultPosition,
                                     wxDefaultSize, wxRB_GROUP);
  m_radioRange = new wxRadioButton(this, wxID_ANY, _("Within a Range"));
  m_radioRead =
      // i18n: This is a selectable condition when adding a breakpoint
      new wxRadioButton(this, wxID_ANY, _("Read"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  // i18n: This is a selectable condition when adding a breakpoint
  m_radioWrite = new wxRadioButton(this, wxID_ANY, _("Write"));
  // i18n: This is a selectable condition when adding a breakpoint
  m_radioReadWrite = new wxRadioButton(this, wxID_ANY, _("Read or Write"));
  // i18n: This is a selectable action when adding a breakpoint
  m_radioLog = new wxRadioButton(this, wxID_ANY, _("Write to Log"), wxDefaultPosition,
                                 wxDefaultSize, wxRB_GROUP);
  // i18n: This is a selectable action when adding a breakpoint
  m_radioBreak = new wxRadioButton(this, wxID_ANY, _("Break"));
  // i18n: This is a selectable action when adding a breakpoint
  m_radioBreakLog = new wxRadioButton(this, wxID_ANY, _("Write to Log and Break"));
  m_radioBreakLog->SetValue(true);

  auto* sAddressBox = new wxBoxSizer(wxHORIZONTAL);
  sAddressBox->AddSpacer(space5);
  sAddressBox->Add(m_textAddress, 0, wxALIGN_CENTER_VERTICAL);
  sAddressBox->AddSpacer(space5);
  sAddressBox->Add(m_pEditAddress, 1, wxALIGN_CENTER_VERTICAL);
  sAddressBox->AddSpacer(space5);

  auto* sAddressRangeBox = new wxBoxSizer(wxHORIZONTAL);
  sAddressRangeBox->AddSpacer(space5);
  sAddressRangeBox->Add(m_textStartAddress, 0, wxALIGN_CENTER_VERTICAL);
  sAddressRangeBox->AddSpacer(space5);
  sAddressRangeBox->Add(m_pEditStartAddress, 1, wxALIGN_CENTER_VERTICAL);
  sAddressRangeBox->AddSpacer(space5);
  sAddressRangeBox->Add(m_textEndAddress, 0, wxALIGN_CENTER_VERTICAL);
  sAddressRangeBox->AddSpacer(space5);
  sAddressRangeBox->Add(m_pEditEndAddress, 1, wxALIGN_CENTER_VERTICAL);
  sAddressRangeBox->AddSpacer(space5);

  auto* sActions = new wxStaticBoxSizer(wxVERTICAL, this, _("Condition"));
  sActions->Add(m_radioRead, 0, wxEXPAND);
  sActions->Add(m_radioWrite, 0, wxEXPAND);
  sActions->Add(m_radioReadWrite, 0, wxEXPAND);
  m_radioWrite->SetValue(true);

  auto* sFlags = new wxStaticBoxSizer(wxVERTICAL, this, _("Action"));
  sFlags->Add(m_radioLog);
  sFlags->Add(m_radioBreak);
  sFlags->Add(m_radioBreakLog);

  auto* sOptionsBox = new wxBoxSizer(wxHORIZONTAL);
  sOptionsBox->Add(sActions, 1, wxEXPAND, space5);
  sOptionsBox->Add(sFlags, 1, wxEXPAND | wxLEFT, space5);

  auto* sControls = new wxBoxSizer(wxVERTICAL);
  sControls->Add(m_radioAddress, 0, wxEXPAND);
  sControls->AddSpacer(5);
  sControls->Add(sAddressBox, 0, wxEXPAND);
  sControls->AddSpacer(5);
  sControls->Add(m_radioRange, 0, wxEXPAND);
  sControls->AddSpacer(5);
  sControls->Add(sAddressRangeBox, 0, wxEXPAND);
  sControls->AddSpacer(5);
  sControls->Add(sOptionsBox, 0, wxEXPAND);

  auto* sMainSizer = new wxBoxSizer(wxVERTICAL);
  sMainSizer->AddSpacer(space5);
  sMainSizer->Add(sControls, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMainSizer->AddSpacer(space5);
  sMainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sMainSizer->AddSpacer(space5);

  SetSizerAndFit(sMainSizer);
  SetFocus();
  m_pEditAddress->SetFocus();
}

void MemoryCheckDlg::OnRadioButtonClick(wxCommandEvent& event)
{
  if (m_radioAddress->GetValue())
  {
    m_pEditAddress->Enable();
    m_textAddress->Enable();
    m_pEditStartAddress->Disable();
    m_pEditEndAddress->Disable();
    m_textStartAddress->Disable();
    m_textEndAddress->Disable();
  }
  else
  {
    m_pEditStartAddress->Enable();
    m_textStartAddress->Enable();
    m_textEndAddress->Enable();
    m_pEditEndAddress->Enable();
    m_pEditAddress->Disable();
    m_textAddress->Disable();
  }
}

void MemoryCheckDlg::OnOK(wxCommandEvent& event)
{
  wxString StartAddressString;
  wxString EndAddressString;
  if (m_radioAddress->GetValue())
  {
    StartAddressString = m_pEditAddress->GetLineText(0);
    EndAddressString = m_pEditAddress->GetLineText(0);
  }
  else
  {
    StartAddressString = m_pEditStartAddress->GetLineText(0);
    EndAddressString = m_pEditEndAddress->GetLineText(0);
  }
  bool OnRead = m_radioRead->GetValue() || m_radioReadWrite->GetValue();
  bool OnWrite = m_radioWrite->GetValue() || m_radioReadWrite->GetValue();
  bool Log = m_radioLog->GetValue() || m_radioBreakLog->GetValue();
  bool Break = m_radioBreak->GetValue() || m_radioBreakLog->GetValue();

  u32 StartAddress = UINT32_MAX, EndAddress = 0;
  bool EndAddressOK =
      EndAddressString.Len() && AsciiToHex(WxStrToStr(EndAddressString), EndAddress);

  if (AsciiToHex(WxStrToStr(StartAddressString), StartAddress) && (OnRead || OnWrite) &&
      (Log || Break))
  {
    TMemCheck MemCheck;

    if (!EndAddressOK)
      EndAddress = StartAddress;

    MemCheck.start_address = StartAddress;
    MemCheck.end_address = EndAddress;
    MemCheck.is_ranged = StartAddress != EndAddress;
    MemCheck.is_break_on_read = OnRead;
    MemCheck.is_break_on_write = OnWrite;
    MemCheck.log_on_hit = Log;
    MemCheck.break_on_hit = Break;

    PowerPC::memchecks.Add(MemCheck);
    EndModal(wxID_OK);
  }

  event.Skip();
}
