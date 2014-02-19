// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include "Core/ConfigManager.h"
#include "VideoBackends/Software/SWVideoConfig.h"
#include "VideoCommon/VideoBackendBase.h"

class VideoConfigDialog : public wxDialog
{
public:
	VideoConfigDialog(wxWindow* parent, const std::string &title, const std::string& ininame);
	~VideoConfigDialog();

	void Event_Backend(wxCommandEvent &ev)
	{
		VideoBackend* new_backend = g_available_video_backends[ev.GetInt()];

		if (g_video_backend != new_backend)
		{
			Close();

			g_video_backend = new_backend;
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend = g_video_backend->GetName();

			g_video_backend->ShowConfig(GetParent());
		}
		ev.Skip();
	}

protected:
	SWVideoConfig& vconfig;
	std::string ininame;
};
