// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/VideoBackendBase.h"

std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
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
	// OGL > D3D11 > D3D12 > SW
	g_available_video_backends.push_back(std::make_unique<OGL::VideoBackend>());
#if defined(_WIN32)
	g_available_video_backends.push_back(std::make_unique<DX11::VideoBackend>());
#if !(defined(_MSC_VER) && _MSC_VER <= 1800)
	// More robust way to check for D3D12 support than (unreliable) OS version checks.
	HMODULE d3d12_module = LoadLibraryA("d3d12.dll");
	if (d3d12_module != nullptr)
	{
		FreeLibrary(d3d12_module);
		g_available_video_backends.push_back(std::make_unique<DX12::VideoBackend>());
	}
#endif
#endif
	g_available_video_backends.push_back(std::make_unique<SW::VideoSoftware>());

	for (auto & backend : g_available_video_backends)
	{
		if (backend != nullptr)
		{
			s_default_backend = backend.get();
			g_video_backend = backend.get();
			return;
		}
	}
}

void VideoBackendBase::ClearList()
{
	g_available_video_backends.clear();
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
	// If empty, set it to the default backend (expected behavior)
	if (name.empty())
		g_video_backend = s_default_backend;

	for (auto & backend : g_available_video_backends)
	{
		if (name == backend->GetName())
		{
			g_video_backend = backend.get();
			return;
		}
	}
}
