// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/VideoBackendBase.h"

std::vector<VideoBackendBase*> g_available_video_backends;
VideoBackendBase* g_video_backend = nullptr;
static VideoBackendBase* s_default_backend = nullptr;

#ifdef _WIN32
#include <windows.h>

// Nvidia drivers >= v302 will check if the application exports a global
// variable named NvOptimusEnablement to know if it should run the app in high
// performance graphics mode or using the IGP.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
}
#endif

void VideoBackendBase::PopulateList()
{
	VideoBackendBase* backends[4] = { nullptr };

	// OGL > D3D11 > D3D12 > SW
	g_available_video_backends.push_back(backends[0] = new OGL::VideoBackend);
#ifdef _WIN32
	g_available_video_backends.push_back(backends[1] = new DX11::VideoBackend);

	// More robust way to check for D3D12 support than (unreliable) OS version checks.
	HMODULE d3d12_module = LoadLibraryA("d3d12.dll");
	if (d3d12_module != NULL)
	{
		FreeLibrary(d3d12_module);
		g_available_video_backends.push_back(backends[2] = new DX12::VideoBackend);
	}
#endif
	g_available_video_backends.push_back(backends[3] = new SW::VideoSoftware);

	for (VideoBackendBase* backend : backends)
	{
		if (backend)
		{
			s_default_backend = g_video_backend = backend;
			break;
		}
	}
}

void VideoBackendBase::ClearList()
{
	while (!g_available_video_backends.empty())
	{
		delete g_available_video_backends.back();
		g_available_video_backends.pop_back();
	}
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
	if (name.length() == 0) // If nullptr, set it to the default backend (expected behavior)
		g_video_backend = s_default_backend;

	for (VideoBackendBase* backend : g_available_video_backends)
		if (name == backend->GetName())
			g_video_backend = backend;
}
