// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/eventfilter.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "Core/ConfigManager.h"

class GCAdapterConfigDiag : public wxDialog
{
public:
	GCAdapterConfigDiag(wxWindow* const parent, const wxString& name, const int tab_num = 0);
	~GCAdapterConfigDiag();

	void ScheduleAdapterUpdate();
	void UpdateAdapter(wxCommandEvent& ev);

private:
	wxStaticText* m_adapter_status;
	int m_pad_id;

	void OnAdapterRumble(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_AdapterRumble[m_pad_id] = event.IsChecked();
	}

	void OnAdapterKonga(wxCommandEvent& event)
	{
		SConfig::GetInstance().m_AdapterKonga[m_pad_id] = event.IsChecked();
	}
};
