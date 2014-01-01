
#ifndef OGL_VIDEO_BACKEND_H_
#define OGL_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace OGL
{

class VideoBackend : public VideoBackendHardware
{
	bool Initialize(void *&) override;
	void Shutdown() override;

	std::string GetName() override;
	std::string GetDisplayName() override;

	void Video_Prepare() override;
	void Video_Cleanup() override;

	void ShowConfig(void* parent) override;

	void UpdateFPSDisplay(const char*) override;
	unsigned int PeekMessages() override;
};

}

#endif
