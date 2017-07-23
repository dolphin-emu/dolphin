// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GCAdapterConfigDiag.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "InputCommon/GCAdapter.h"

wxDEFINE_EVENT(wxEVT_ADAPTER_UPDATE, wxCommandEvent);

GCAdapterConfigDiag::GCAdapterConfigDiag(wxWindow* const parent, const wxString& name,
                                         const int tab_num)
    : wxDialog(parent, wxID_ANY, name), m_pad_id(tab_num)
{
  wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Enable Rumble"));
  gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble[m_pad_id]);
  gamecube_rumble->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterRumble, this);

  wxCheckBox* const gamecube_konga = new wxCheckBox(this, wxID_ANY, _("Simulate DK Bongos"));
  gamecube_konga->SetValue(SConfig::GetInstance().m_AdapterKonga[m_pad_id]);
  gamecube_konga->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterKonga, this);

  m_adapter_status = new wxStaticText(this, wxID_ANY, _("No Adapter Detected"));

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
  szr->Add(gamecube_rumble, 0, wxEXPAND);
  szr->Add(gamecube_konga, 0, wxEXPAND);
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
  Core::RunAsCPUThread([this] {
    if (GCAdapter::IsDetected())
      m_adapter_status->SetLabelText(_("Adapter Detected"));
    else
      m_adapter_status->SetLabelText(_("No Adapter Detected"));
  });
}

void GCAdapterConfigDiag::OnAdapterRumble(wxCommandEvent& event)
{
  SConfig::GetInstance().m_AdapterRumble[m_pad_id] = event.IsChecked();
}

void GCAdapterConfigDiag::OnAdapterKonga(wxCommandEvent& event)
{
  SConfig::GetInstance().m_AdapterKonga[m_pad_id] = event.IsChecked();
}
