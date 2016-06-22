// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace Vulkan {

class VideoBackend : public VideoBackendBase
{
	bool Initialize(void* window_handle) override;
	void Shutdown() override;

	std::string GetName() const override { return "Vulkan"; }
	std::string GetDisplayName() const override { return "Vulkan"; }

	void Video_Prepare() override;
	void Video_Cleanup() override;

	void ShowConfig(void* parent) override;

	unsigned int PeekMessages() override { return 0; }
};

}
