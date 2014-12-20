#pragma once

#include <string>
#include "VideoCommon/VideoBackendBase.h"

namespace MMIO { class Mapping; }

namespace SW
{

class VideoSoftware : public VideoBackend
{
	bool Initialize(void *window_handle) override;
	void Shutdown() override;

	std::string GetName() const override;
	std::string GetDisplayName() const override;

	void EmuStateChange(EMUSTATE_CHANGE newState) override;

	void RunLoop(bool enable) override;

	void ShowConfig(void* parent) override;

	void Video_Prepare() override;
	void Video_Cleanup() override;

	void Video_EnterLoop() override;
	void Video_ExitLoop() override;
	void Video_BeginField(u32, u32, u32, u32) override;
	void Video_EndField() override;

	u32 Video_AccessEFB(EFBAccessType, u32, u32, u32) override;
	u32 Video_GetQueryResult(PerfQueryType type) override;
	u16 Video_GetBoundingBox(int index) override;

	void Video_AddMessage(const std::string& msg, unsigned int milliseconds) override;
	void Video_ClearMessages() override;
	bool Video_Screenshot(const std::string& filename) override;

	int Video_LoadTexture(char *imagedata, u32 width, u32 height);
	void Video_DeleteTexture(int texID);
	void Video_DrawTexture(int texID, float *coords);

	void Video_SetRendering(bool bEnabled) override;

	void Video_GatherPipeBursted() override;
	bool Video_IsPossibleWaitingSetDrawDone() override;

	void RegisterCPMMIO(MMIO::Mapping* mmio, u32 base) override;

	unsigned int PeekMessages() override;

	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;
	void DoState(PointerWrap &p) override;

public:
	void CheckInvalidState() override;
};

}
