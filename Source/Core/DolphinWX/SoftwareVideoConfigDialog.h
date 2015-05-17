// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO: Merge this with the original VideoConfigDiag or something
//       This is an awful way of managing a separate video backend.

#pragma once

#include <string>
#include <vector>

#include <wx/dialog.h>

#include "Core/ConfigManager.h"
#include "VideoBackends/Software/SWVideoConfig.h"
#include "VideoCommon/VideoBackendBase.h"

class SoftwareVideoConfigDialog : public wxDialog
{
public:
	SoftwareVideoConfigDialog(wxWindow* parent, const std::string &title, const std::string& ininame);
	~SoftwareVideoConfigDialog();

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
