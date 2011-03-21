
#ifndef DX11_VIDEO_BACKEND_H_
#define DX11_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace DX11
{

class VideoBackend : public VideoBackendHardware
{
public:
	VideoBackend() : VideoBackendHardware("gfx_dx11") {}

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
