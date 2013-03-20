
#ifndef DX9_VIDEO_BACKEND_H_
#define DX9_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace DX9
{

class VideoBackend : public VideoBackendHardware
{
	bool Initialize(void *&);
	void Shutdown();

	std::string GetName();
	std::string GetDisplayName();

	void Video_Prepare();
	void Video_Cleanup();

	void ShowConfig(void* parent);

	void UpdateFPSDisplay(const char*);
	unsigned int PeekMessages();
};

}

#endif
