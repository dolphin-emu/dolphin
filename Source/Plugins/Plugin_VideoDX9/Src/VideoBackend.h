
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

	void Video_Prepare();

	void ShowConfig(void* parent);

	void UpdateFPSDisplay(const char*);
	unsigned int PeekMessages();
};

}

#endif
