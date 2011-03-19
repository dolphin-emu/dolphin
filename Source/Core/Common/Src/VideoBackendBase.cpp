// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "VideoBackendBase.h"

// TODO: ugly
#ifdef _WIN32
#include "../../../Plugins/Plugin_VideoDX9/Src/VideoBackend.h"
#include "../../../Plugins/Plugin_VideoDX11/Src/VideoBackend.h"
#endif
#include "../../../Plugins/Plugin_VideoOGL/Src/VideoBackend.h"
#include "../../../Plugins/Plugin_VideoSoftware/Src/VideoBackend.h"

std::vector<VideoBackend*> g_available_video_backends;
VideoBackend* g_video_backend = NULL;

#ifdef _WIN32
// http://msdn.microsoft.com/en-us/library/ms725491.aspx
static bool IsGteVista() 
{
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 6;

	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);

	return VerifyVersionInfo(&osvi, VER_MAJORVERSION, dwlConditionMask);
}
#endif

void VideoBackend::PopulateList()
{
#ifdef _WIN32
	g_available_video_backends.push_back(new DX9::VideoBackend);
	if (IsGteVista())
		g_available_video_backends.push_back(new DX11::VideoBackend);
#endif
	g_available_video_backends.push_back(new OGL::VideoBackend);
	g_available_video_backends.push_back(new SW::VideoSoftware);

	g_video_backend = g_available_video_backends.front();
}

void VideoBackend::ClearList()
{
	while (!g_available_video_backends.empty())
	{
		delete g_available_video_backends.back();
		g_available_video_backends.pop_back();
	}
}

void VideoBackend::ActivateBackend(const std::string& name)
{
	for (std::vector<VideoBackend*>::const_iterator it = g_available_video_backends.begin(); it != g_available_video_backends.end(); ++it)
		if (name == (*it)->GetName())
			g_video_backend = *it;
}
