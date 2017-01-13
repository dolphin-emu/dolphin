// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GCAdapterConfigDiag.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "InputCommon/GCAdapter.h"
#include "Common/Logging/Log.h"

wxDEFINE_EVENT(wxEVT_ADAPTER_UPDATE, wxCommandEvent);

GCAdapterConfigDiag::GCAdapterConfigDiag(wxWindow* const parent, const wxString& name,
                                         const int tab_num)
    : wxDialog(parent, wxID_ANY, name), m_pad_id(tab_num)
{
  wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Rumble"));
  gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble[m_pad_id]);
  gamecube_rumble->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterRumble, this);

  wxCheckBox* const gamecube_konga = new wxCheckBox(this, wxID_ANY, _("Simulate DK Bongos"));
  gamecube_konga->SetValue(SConfig::GetInstance().m_AdapterKonga[m_pad_id]);
  gamecube_konga->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterKonga, this);

  m_adapter_status = new wxStaticText(this, wxID_ANY, _("Adapter Not Detected"));

  wxCheckBox* const modbxw201fix = new wxCheckBox(this, wxID_ANY, _("Use MOD: BX-W201 adapter fix (Z button won't work)."));
  modbxw201fix->SetValue(SConfig::GetInstance().m_UseMODBXW201Fix);
  modbxw201fix->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnUseMODBXW201Fix, this);

  if (!GCAdapter::IsDetected())
  {
    if (!GCAdapter::IsDriverDetected())
    {
      m_adapter_status->SetLabelText(_("Driver Not Detected"));
      gamecube_rumble->Disable();
    }
  }
  else
  {
    m_adapter_status->SetLabelText(_("Adapter Detected"));
  }
  GCAdapter::SetAdapterCallback(std::bind(&GCAdapterConfigDiag::ScheduleAdapterUpdate, this));

  const int space5 = FromDIP(5);

  wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
  szr->Add(m_adapter_status, 0, wxEXPAND);
  szr->Add(new wxStaticLine(this), 0, wxEXPAND);
  szr->Add(gamecube_rumble, 0, wxEXPAND);
  szr->Add(gamecube_konga, 0, wxEXPAND);
  szr->Add(new wxStaticLine(this), 0, wxEXPAND);
  szr->Add(modbxw201fix, 0, wxEXPAND);
  szr->AddSpacer(space5);
  szr->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr->AddSpacer(space5);

  SetSizerAndFit(szr);
  Center();

  Bind(wxEVT_ADAPTER_UPDATE, &GCAdapterConfigDiag::OnUpdateAdapter, this);
}

GCAdapterConfigDiag::~GCAdapterConfigDiag()
{
  GCAdapter::SetAdapterCallback(nullptr);
}

void GCAdapterConfigDiag::ScheduleAdapterUpdate()
{
  wxQueueEvent(this, new wxCommandEvent(wxEVT_ADAPTER_UPDATE));
}

void GCAdapterConfigDiag::OnUpdateAdapter(wxCommandEvent& WXUNUSED(event))
{
  bool unpause = Core::PauseAndLock(true);
  if (GCAdapter::IsDetected())
    m_adapter_status->SetLabelText(_("Adapter Detected"));
  else
    m_adapter_status->SetLabelText(_("Adapter Not Detected"));
  Core::PauseAndLock(false, unpause);
}

void GCAdapterConfigDiag::OnAdapterRumble(wxCommandEvent& event)
{
  SConfig::GetInstance().m_AdapterRumble[m_pad_id] = event.IsChecked();
}

void GCAdapterConfigDiag::OnAdapterKonga(wxCommandEvent& event)
{
  SConfig::GetInstance().m_AdapterKonga[m_pad_id] = event.IsChecked();
}

// Set if user wants to use the third-party chinese clone adapter marked as "Mod: BX-W201".
// This will fix the always pressed L trigger.  The Z-button does not work on this adapter.
void GCAdapterConfigDiag::OnUseMODBXW201Fix(wxCommandEvent& event)
{
  const auto checked = event.IsChecked();
  SConfig::GetInstance().m_UseMODBXW201Fix = checked;
  if (checked) {
    NOTICE_LOG(SERIALINTERFACE, "MOD: BX-W201 fix has been enabled. The Z button won't work due to hardware bug.");
  } else {
    NOTICE_LOG(SERIALINTERFACE, "MOD: BX-W201 fix has been disabled. ");
  }
}