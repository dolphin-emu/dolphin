
#ifndef SW_VIDEO_BACKEND_H_
#define SW_VIDEO_BACKEND_H_

#include "VideoBackendBase.h"

namespace SW
{

class VideoBackend : public VideoBackendLLE
{
	bool Initialize(void *&);
	void Shutdown();

	std::string GetName();

	void EmuStateChange(EMUSTATE_CHANGE newState);

	void DoState(PointerWrap &p);
	void RunLoop(bool enable);

	void ShowConfig(void* parent);

	void Video_Prepare();

	void Video_EnterLoop();
	void Video_ExitLoop();
	void Video_BeginField(u32, FieldType, u32, u32);
	void Video_EndField();
	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32);

	void Video_AddMessage(const char* pstr, unsigned int milliseconds);
	void Video_ClearMessages();
	bool Video_Screenshot(const char* filename);

	void Video_SetRendering(bool bEnabled);

	bool Video_IsPossibleWaitingSetDrawDone();
	void Video_AbortFrame();

	void UpdateFPSDisplay(const char*);
	unsigned int PeekMessages();
};

}

#endif
