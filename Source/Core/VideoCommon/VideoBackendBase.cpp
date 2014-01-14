// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackendBase.h"

// TODO: ugly
#ifdef _WIN32
#include "../VideoBackends/D3D/VideoBackend.h"
#endif
#if !defined(USE_GLES) || USE_GLES3
#include "../VideoBackends/OGL/VideoBackend.h"
#endif
#include "../VideoBackends/Software/VideoBackend.h"

std::vector<VideoBackend*> g_available_video_backends;
VideoBackend* g_video_backend = NULL;
static VideoBackend* s_default_backend = NULL;

#ifdef _WIN32
#include <windows.h>

// http://msdn.microsoft.com/en-us/library/ms725491.aspx
static bool IsGteVista()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 6;

	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);

	return VerifyVersionInfo(&osvi, VER_MAJORVERSION, dwlConditionMask) != FALSE;
}
#endif

void VideoBackend::PopulateList()
{
	VideoBackend* backends[4] = { NULL };

	// OGL > D3D11 > SW
#if !defined(USE_GLES) || USE_GLES3
	g_available_video_backends.push_back(backends[0] = new OGL::VideoBackend);
#endif
#ifdef _WIN32
	if (IsGteVista())
		g_available_video_backends.push_back(backends[1] = new DX11::VideoBackend);
#endif
	g_available_video_backends.push_back(backends[3] = new SW::VideoSoftware);

	for (auto& backend : backends)
	{
		if (backend)
		{
			s_default_backend = g_video_backend = backend;
			break;
		}
	}
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
	if (name.length() == 0) // If NULL, set it to the default backend (expected behavior)
		g_video_backend = s_default_backend;

	for (std::vector<VideoBackend*>::const_iterator it = g_available_video_backends.begin(); it != g_available_video_backends.end(); ++it)
		if (name == (*it)->GetName())
			g_video_backend = *it;
}
