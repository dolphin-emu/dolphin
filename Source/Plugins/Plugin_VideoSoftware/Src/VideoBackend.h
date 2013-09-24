
#ifndef SW_VIDEO_BACKEND_H_
#define SW_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace SW
{

class VideoSoftware : public VideoBackend
{
	bool Initialize(void *&);
	void Shutdown();

	std::string GetName();

	void EmuStateChange(EMUSTATE_CHANGE newState);

	void RunLoop(bool enable);

	void ShowConfig(void* parent);

	void Video_Prepare();
	void Video_Cleanup();

	void Video_EnterLoop();
	void Video_ExitLoop();
	void Video_BeginField(u32, u32, u32);
	void Video_EndField();
	
	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32);
	u32 Video_GetQueryResult(PerfQueryType type);

	void Video_AddMessage(const char* pstr, unsigned int milliseconds);
	void Video_ClearMessages();
	bool Video_Screenshot(const char* filename);

	int Video_LoadTexture(char *imagedata, u32 width, u32 height);
	void Video_DeleteTexture(int texID);
	void Video_DrawTexture(int texID, float *coords);

	void Video_SetRendering(bool bEnabled);

	void Video_GatherPipeBursted();
	bool Video_IsHiWatermarkActive();
	bool Video_IsPossibleWaitingSetDrawDone();
	void Video_AbortFrame();

	readFn16  Video_CPRead16();
	writeFn16 Video_CPWrite16();
	readFn16  Video_PERead16();
	writeFn16 Video_PEWrite16();
	writeFn32 Video_PEWrite32();

	void UpdateFPSDisplay(const char*);
	unsigned int PeekMessages();

	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true);
	void DoState(PointerWrap &p);
	
public:
	void CheckInvalidState();
};

}

#endif
