// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/Config/GCAdapterConfigDiag.h"
#include "InputCommon/GCAdapter.h"

wxDEFINE_EVENT(wxEVT_ADAPTER_UPDATE, wxCommandEvent);


GCAdapterConfigDiag::GCAdapterConfigDiag(wxWindow* const parent, const wxString& name, const int tab_num)
	: wxDialog(parent, wxID_ANY, name, wxPoint(128,-1)), m_pad_id(tab_num)
{
	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);


	wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Rumble"));
	gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble[m_pad_id]);
	gamecube_rumble->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterRumble, this);

	wxCheckBox* const gamecube_konga = new wxCheckBox(this, wxID_ANY, _("Simulate DK TaruKonga"));
	gamecube_konga->SetValue(SConfig::GetInstance().m_AdapterKonga[m_pad_id]);
	gamecube_konga->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterKonga, this);

	m_adapter_status = new wxStaticText(this, wxID_ANY, _("Adapter Not Detected"));

#if defined(__LIBUSB__) || defined (_WIN32)
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
#endif

	szr->Add(m_adapter_status, 0, wxEXPAND);
	szr->Add(gamecube_rumble, 0, wxEXPAND);
	szr->Add(gamecube_konga, 0, wxEXPAND);
	szr->Add(CreateButtonSizer(wxOK | wxNO_DEFAULT), 0, wxEXPAND|wxALL, 5);

	SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
	SetSizerAndFit(szr);
	Center();

	Bind(wxEVT_ADAPTER_UPDATE, &GCAdapterConfigDiag::UpdateAdapter, this);
}

void GCAdapterConfigDiag::ScheduleAdapterUpdate()
{
	wxQueueEvent(this, new wxCommandEvent(wxEVT_ADAPTER_UPDATE));
}

void GCAdapterConfigDiag::UpdateAdapter(wxCommandEvent& ev)
{
#if defined(__LIBUSB__) || defined (_WIN32)
	bool unpause = Core::PauseAndLock(true);
	if (GCAdapter::IsDetected())
		m_adapter_status->SetLabelText(_("Adapter Detected"));
	else
		m_adapter_status->SetLabelText(_("Adapter Not Detected"));
	Core::PauseAndLock(false, unpause);
#endif
}

GCAdapterConfigDiag::~GCAdapterConfigDiag()
{
#if defined(__LIBUSB__) || defined (_WIN32)
	GCAdapter::SetAdapterCallback(nullptr);
#endif
}
