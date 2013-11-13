
#ifndef SW_VIDEO_BACKEND_H_
#define SW_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace SW
{

class VideoSoftware : public VideoBackend
{
	bool Initialize(void *&) override;
	void Shutdown() override;

	std::string GetName() override;

	void EmuStateChange(EMUSTATE_CHANGE newState) override;

	void RunLoop(bool enable) override;

	void ShowConfig(void* parent) override;

	void Video_Prepare() override;
	void Video_Cleanup() override;

	void Video_EnterLoop() override;
	void Video_ExitLoop() override;
	void Video_BeginField(u32, u32, u32) override;
	void Video_EndField() override;

	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32) override;
	u32 Video_GetQueryResult(PerfQueryType type) override;

	void Video_AddMessage(const char* pstr, unsigned int milliseconds) override;
	void Video_ClearMessages() override;
	bool Video_Screenshot(const char* filename) override;

	int Video_LoadTexture(char *imagedata, u32 width, u32 height);
	void Video_DeleteTexture(int texID);
	void Video_DrawTexture(int texID, float *coords);

	void Video_SetRendering(bool bEnabled) override;

	void Video_GatherPipeBursted() override;
	bool Video_IsHiWatermarkActive() override;
	bool Video_IsPossibleWaitingSetDrawDone() override;
	void Video_AbortFrame() override;

	readFn16  Video_CPRead16() override;
	writeFn16 Video_CPWrite16() override;
	readFn16  Video_PERead16() override;
	writeFn16 Video_PEWrite16() override;
	writeFn32 Video_PEWrite32() override;

	void UpdateFPSDisplay(const char*) override;
	unsigned int PeekMessages() override;

	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;
	void DoState(PointerWrap &p) override;

public:
	void CheckInvalidState() override;
};

}

#endif
